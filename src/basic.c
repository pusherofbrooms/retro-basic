#include "basic.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  VAL_NUMBER,
  VAL_STRING
} ValueType;

typedef struct {
  ValueType type;
  float number;
  char *string;
  size_t length;
} Value;

typedef struct {
  char *name;
  Value value;
} Variable;

typedef struct {
  char *name;
  int is_string;
  size_t size;
  Value *values;
} Array;

typedef struct {
  int number;
  char *text;
} ProgramLine;

typedef struct {
  ProgramLine *lines;
  size_t count;
  size_t capacity;
} Program;

typedef struct {
  char *name;
  float limit;
  float step;
  size_t return_index;
} ForFrame;

typedef struct {
  size_t return_index;
} GosubFrame;

typedef struct {
  Value *items;
  size_t count;
  size_t capacity;
  size_t cursor;
} DataList;

typedef enum {
  TOK_EOF,
  TOK_NUMBER,
  TOK_STRING,
  TOK_IDENT,
  TOK_OP,
  TOK_COMMA,
  TOK_SEMICOLON,
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_COLON
} TokenType;

typedef struct {
  TokenType type;
  char text[128];
  float number;
  char string[256];
} Token;

typedef struct {
  const char *input;
  size_t pos;
  Token current;
} Tokenizer;

struct BasicInterpreter {
  Program program;
  Variable *vars;
  size_t var_count;
  size_t var_capacity;
  Array *arrays;
  size_t array_count;
  size_t array_capacity;
  ForFrame *for_stack;
  size_t for_count;
  size_t for_capacity;
  GosubFrame *gosub_stack;
  size_t gosub_count;
  size_t gosub_capacity;
  DataList data;
  int stop;
  int error;
  char error_msg[128];
  int in_program;
  int current_line_number;
};

static void value_free(Value *value) {
  if (value->type == VAL_STRING && value->string) {
    free(value->string);
  }
  value->type = VAL_NUMBER;
  value->number = 0.0f;
  value->string = NULL;
  value->length = 0;
}

static Value value_number(float num) {
  Value value;
  value.type = VAL_NUMBER;
  value.number = num;
  value.string = NULL;
  value.length = 0;
  return value;
}

static Value value_string(const char *text, size_t len) {
  Value value;
  value.type = VAL_STRING;
  value.number = 0.0f;
  value.string = (char *)malloc(len + 1);
  if (value.string) {
    memcpy(value.string, text, len);
    value.string[len] = '\0';
    value.length = len;
  } else {
    value.length = 0;
  }
  return value;
}

static Value value_copy(const Value *value) {
  if (value->type == VAL_STRING) {
    return value_string(value->string ? value->string : "", value->length);
  }
  return value_number(value->number);
}


static void program_init(Program *program) {
  program->lines = NULL;
  program->count = 0;
  program->capacity = 0;
}

static void program_free(Program *program) {
  for (size_t i = 0; i < program->count; i++) {
    free(program->lines[i].text);
  }
  free(program->lines);
  program->lines = NULL;
  program->count = 0;
  program->capacity = 0;
}

static size_t program_find_index(Program *program, int number, int *found) {
  size_t left = 0;
  size_t right = program->count;
  while (left < right) {
    size_t mid = left + (right - left) / 2;
    int mid_num = program->lines[mid].number;
    if (mid_num == number) {
      *found = 1;
      return mid;
    }
    if (mid_num < number) {
      left = mid + 1;
    } else {
      right = mid;
    }
  }
  *found = 0;
  return left;
}

static void program_delete_line(Program *program, size_t index) {
  if (index >= program->count) {
    return;
  }
  free(program->lines[index].text);
  for (size_t i = index + 1; i < program->count; i++) {
    program->lines[i - 1] = program->lines[i];
  }
  program->count--;
}

static void program_set_line(Program *program, int number, const char *text) {
  int found = 0;
  size_t index = program_find_index(program, number, &found);
  if (!text || text[0] == '\0') {
    if (found) {
      program_delete_line(program, index);
    }
    return;
  }

  if (found) {
    free(program->lines[index].text);
    program->lines[index].text = strdup(text);
    return;
  }

  if (program->count == program->capacity) {
    size_t new_cap = program->capacity == 0 ? 16 : program->capacity * 2;
    ProgramLine *new_lines = (ProgramLine *)realloc(program->lines, new_cap * sizeof(ProgramLine));
    if (!new_lines) {
      return;
    }
    program->lines = new_lines;
    program->capacity = new_cap;
  }

  for (size_t i = program->count; i > index; i--) {
    program->lines[i] = program->lines[i - 1];
  }
  program->lines[index].number = number;
  program->lines[index].text = strdup(text);
  program->count++;
}

static Variable *vars_find(BasicInterpreter *interp, const char *name) {
  for (size_t i = 0; i < interp->var_count; i++) {
    if (strcmp(interp->vars[i].name, name) == 0) {
      return &interp->vars[i];
    }
  }
  return NULL;
}

static Variable *vars_get(BasicInterpreter *interp, const char *name, int create) {
  Variable *var = vars_find(interp, name);
  if (var || !create) {
    return var;
  }

  if (interp->var_count == interp->var_capacity) {
    size_t new_cap = interp->var_capacity == 0 ? 32 : interp->var_capacity * 2;
    Variable *new_vars = (Variable *)realloc(interp->vars, new_cap * sizeof(Variable));
    if (!new_vars) {
      return NULL;
    }
    interp->vars = new_vars;
    interp->var_capacity = new_cap;
  }
  interp->vars[interp->var_count].name = strdup(name);
  interp->vars[interp->var_count].value = value_number(0.0f);
  return &interp->vars[interp->var_count++];
}

static void vars_free(BasicInterpreter *interp) {
  for (size_t i = 0; i < interp->var_count; i++) {
    free(interp->vars[i].name);
    value_free(&interp->vars[i].value);
  }
  free(interp->vars);
  interp->vars = NULL;
  interp->var_count = 0;
  interp->var_capacity = 0;
}

static Array *arrays_find(BasicInterpreter *interp, const char *name) {
  for (size_t i = 0; i < interp->array_count; i++) {
    if (strcmp(interp->arrays[i].name, name) == 0) {
      return &interp->arrays[i];
    }
  }
  return NULL;
}

static Array *arrays_get(BasicInterpreter *interp, const char *name, int create, int is_string) {
  Array *array = arrays_find(interp, name);
  if (array || !create) {
    return array;
  }
  if (interp->array_count == interp->array_capacity) {
    size_t new_cap = interp->array_capacity == 0 ? 8 : interp->array_capacity * 2;
    Array *new_arrays = (Array *)realloc(interp->arrays, new_cap * sizeof(Array));
    if (!new_arrays) {
      return NULL;
    }
    interp->arrays = new_arrays;
    interp->array_capacity = new_cap;
  }
  interp->arrays[interp->array_count].name = strdup(name);
  interp->arrays[interp->array_count].is_string = is_string;
  interp->arrays[interp->array_count].size = 0;
  interp->arrays[interp->array_count].values = NULL;
  return &interp->arrays[interp->array_count++];
}

static void arrays_free(BasicInterpreter *interp) {
  for (size_t i = 0; i < interp->array_count; i++) {
    free(interp->arrays[i].name);
    if (interp->arrays[i].values) {
      for (size_t j = 0; j < interp->arrays[i].size; j++) {
        value_free(&interp->arrays[i].values[j]);
      }
    }
    free(interp->arrays[i].values);
  }
  free(interp->arrays);
  interp->arrays = NULL;
  interp->array_count = 0;
  interp->array_capacity = 0;
}

static void for_stack_free(BasicInterpreter *interp) {
  for (size_t i = 0; i < interp->for_count; i++) {
    free(interp->for_stack[i].name);
  }
  free(interp->for_stack);
  interp->for_stack = NULL;
  interp->for_count = 0;
  interp->for_capacity = 0;
}

static void gosub_stack_free(BasicInterpreter *interp) {
  free(interp->gosub_stack);
  interp->gosub_stack = NULL;
  interp->gosub_count = 0;
  interp->gosub_capacity = 0;
}

static void data_list_free(DataList *data) {
  for (size_t i = 0; i < data->count; i++) {
    value_free(&data->items[i]);
  }
  free(data->items);
  data->items = NULL;
  data->count = 0;
  data->capacity = 0;
  data->cursor = 0;
}

static void tokenizer_init(Tokenizer *tz, const char *input) {
  tz->input = input;
  tz->pos = 0;
  tz->current.type = TOK_EOF;
  tz->current.text[0] = '\0';
  tz->current.string[0] = '\0';
  tz->current.number = 0.0f;
}

static void tokenizer_skip_spaces(Tokenizer *tz) {
  while (tz->input[tz->pos] && isspace((unsigned char)tz->input[tz->pos])) {
    tz->pos++;
  }
}

static void tokenizer_next(Tokenizer *tz) {
  tokenizer_skip_spaces(tz);
  char c = tz->input[tz->pos];
  tz->current.text[0] = '\0';
  tz->current.string[0] = '\0';
  tz->current.number = 0.0f;
  if (c == '\0') {
    tz->current.type = TOK_EOF;
    return;
  }

  if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)tz->input[tz->pos + 1]))) {
    char *endptr = NULL;
    tz->current.number = strtof(&tz->input[tz->pos], &endptr);
    tz->pos = (size_t)(endptr - tz->input);
    tz->current.type = TOK_NUMBER;
    return;
  }

  if (c == '"') {
    tz->pos++;
    size_t start = tz->pos;
    while (tz->input[tz->pos] && tz->input[tz->pos] != '"') {
      tz->pos++;
    }
    size_t len = tz->pos - start;
    if (len >= sizeof(tz->current.string)) {
      len = sizeof(tz->current.string) - 1;
    }
    memcpy(tz->current.string, tz->input + start, len);
    tz->current.string[len] = '\0';
    tz->current.type = TOK_STRING;
    if (tz->input[tz->pos] == '"') {
      tz->pos++;
    }
    return;
  }

  if (isalpha((unsigned char)c) || c == '_') {
    size_t start = tz->pos;
    tz->pos++;
    while (isalnum((unsigned char)tz->input[tz->pos]) || tz->input[tz->pos] == '$' || tz->input[tz->pos] == '_') {
      tz->pos++;
    }
    size_t len = tz->pos - start;
    if (len >= sizeof(tz->current.text)) {
      len = sizeof(tz->current.text) - 1;
    }
    for (size_t i = 0; i < len; i++) {
      tz->current.text[i] = (char)toupper((unsigned char)tz->input[start + i]);
    }
    tz->current.text[len] = '\0';
    tz->current.type = TOK_IDENT;
    return;
  }

  tz->pos++;
  switch (c) {
    case ',':
      tz->current.type = TOK_COMMA;
      return;
    case ';':
      tz->current.type = TOK_SEMICOLON;
      return;
    case '(':
      tz->current.type = TOK_LPAREN;
      return;
    case ')':
      tz->current.type = TOK_RPAREN;
      return;
    case ':':
      tz->current.type = TOK_COLON;
      return;
    case '<':
      if (tz->input[tz->pos] == '=') {
        tz->pos++;
        strncpy(tz->current.text, "<=", sizeof(tz->current.text));
      } else if (tz->input[tz->pos] == '>') {
        tz->pos++;
        strncpy(tz->current.text, "<>", sizeof(tz->current.text));
      } else {
        strncpy(tz->current.text, "<", sizeof(tz->current.text));
      }
      tz->current.type = TOK_OP;
      return;
    case '>':
      if (tz->input[tz->pos] == '=') {
        tz->pos++;
        strncpy(tz->current.text, ">=", sizeof(tz->current.text));
      } else {
        strncpy(tz->current.text, ">", sizeof(tz->current.text));
      }
      tz->current.type = TOK_OP;
      return;
    case '=':
    case '+':
    case '-':
    case '*':
    case '/':
    case '^':
      tz->current.type = TOK_OP;
      tz->current.text[0] = c;
      tz->current.text[1] = '\0';
      return;
    default:
      tz->current.type = TOK_EOF;
      return;
  }
}

static int token_is(Tokenizer *tz, TokenType type, const char *text) {
  if (tz->current.type != type) {
    return 0;
  }
  if (text) {
    return strcmp(tz->current.text, text) == 0;
  }
  return 1;
}

static void runtime_error(BasicInterpreter *interp, const char *message) {
  interp->error = 1;
  strncpy(interp->error_msg, message, sizeof(interp->error_msg) - 1);
  interp->error_msg[sizeof(interp->error_msg) - 1] = '\0';
}

static void print_error(BasicInterpreter *interp) {
  if (!interp->error) {
    return;
  }
  if (interp->in_program) {
    printf("%s IN %d\n", interp->error_msg, interp->current_line_number);
  } else {
    printf("%s\n", interp->error_msg);
  }
}

static char *value_to_string(const Value *value) {
  if (value->type == VAL_STRING) {
    return strdup(value->string ? value->string : "");
  }
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%g", value->number);
  return strdup(buffer);
}

static Value parse_expression(Tokenizer *tz, BasicInterpreter *interp);

static Value parse_primary(Tokenizer *tz, BasicInterpreter *interp) {
  if (tz->current.type == TOK_NUMBER) {
    float num = tz->current.number;
    tokenizer_next(tz);
    return value_number(num);
  }
  if (tz->current.type == TOK_STRING) {
    Value val = value_string(tz->current.string, strlen(tz->current.string));
    tokenizer_next(tz);
    return val;
  }
  if (tz->current.type == TOK_IDENT) {
    char name[128];
    strncpy(name, tz->current.text, sizeof(name));
    tokenizer_next(tz);

    if (token_is(tz, TOK_LPAREN, NULL)) {
      tokenizer_next(tz);
      if (strcmp(name, "LEFT$") == 0 || strcmp(name, "MID$") == 0 || strcmp(name, "RIGHT$") == 0 ||
          strcmp(name, "LEN") == 0 || strcmp(name, "ASC") == 0 || strcmp(name, "CHR$") == 0 ||
          strcmp(name, "ABS") == 0 || strcmp(name, "INT") == 0 || strcmp(name, "RND") == 0 ||
          strcmp(name, "SIN") == 0 || strcmp(name, "COS") == 0 || strcmp(name, "TAN") == 0 ||
          strcmp(name, "SQR") == 0 || strcmp(name, "LOG") == 0 || strcmp(name, "EXP") == 0 ||
          strcmp(name, "ATN") == 0 || strcmp(name, "SGN") == 0) {
        Value args[3];
        size_t arg_count = 0;
        if (!token_is(tz, TOK_RPAREN, NULL)) {
          while (1) {
            args[arg_count++] = parse_expression(tz, interp);
            if (token_is(tz, TOK_COMMA, NULL)) {
              tokenizer_next(tz);
              continue;
            }
            break;
          }
        }
        if (!token_is(tz, TOK_RPAREN, NULL)) {
          runtime_error(interp, "SYNTAX ERROR");
        } else {
          tokenizer_next(tz);
        }

        Value result = value_number(0.0f);
        if (strcmp(name, "LEFT$") == 0 && arg_count >= 2) {
          char *s = value_to_string(&args[0]);
          int n = (int)args[1].number;
          size_t len = strlen(s);
          if (n < 0) n = 0;
          if ((size_t)n > len) n = (int)len;
          result = value_string(s, (size_t)n);
          free(s);
        } else if (strcmp(name, "RIGHT$") == 0 && arg_count >= 2) {
          char *s = value_to_string(&args[0]);
          int n = (int)args[1].number;
          size_t len = strlen(s);
          if (n < 0) n = 0;
          if ((size_t)n > len) n = (int)len;
          result = value_string(s + (len - (size_t)n), (size_t)n);
          free(s);
        } else if (strcmp(name, "MID$") == 0 && arg_count >= 2) {
          char *s = value_to_string(&args[0]);
          int start = (int)args[1].number;
          int length = arg_count >= 3 ? (int)args[2].number : (int)strlen(s);
          if (start < 1) start = 1;
          if (length < 0) length = 0;
          size_t slen = strlen(s);
          size_t offset = (size_t)(start - 1);
          if (offset > slen) offset = slen;
          size_t max_len = slen - offset;
          size_t use_len = (size_t)length;
          if (use_len > max_len) use_len = max_len;
          result = value_string(s + offset, use_len);
          free(s);
        } else if (strcmp(name, "LEN") == 0 && arg_count >= 1) {
          char *s = value_to_string(&args[0]);
          result = value_number((float)strlen(s));
          free(s);
        } else if (strcmp(name, "ASC") == 0 && arg_count >= 1) {
          char *s = value_to_string(&args[0]);
          result = value_number(s[0] ? (float)(unsigned char)s[0] : 0.0f);
          free(s);
        } else if (strcmp(name, "CHR$") == 0 && arg_count >= 1) {
          char ch = (char)((int)args[0].number & 0xFF);
          result = value_string(&ch, 1);
        } else if (strcmp(name, "ABS") == 0 && arg_count >= 1) {
          result = value_number(fabsf(args[0].number));
        } else if (strcmp(name, "INT") == 0 && arg_count >= 1) {
          result = value_number(floorf(args[0].number));
        } else if (strcmp(name, "RND") == 0) {
          result = value_number((float)rand() / (float)RAND_MAX);
        } else if (strcmp(name, "SIN") == 0 && arg_count >= 1) {
          result = value_number(sinf(args[0].number));
        } else if (strcmp(name, "COS") == 0 && arg_count >= 1) {
          result = value_number(cosf(args[0].number));
        } else if (strcmp(name, "TAN") == 0 && arg_count >= 1) {
          result = value_number(tanf(args[0].number));
        } else if (strcmp(name, "SQR") == 0 && arg_count >= 1) {
          result = value_number(sqrtf(args[0].number));
        } else if (strcmp(name, "LOG") == 0 && arg_count >= 1) {
          result = value_number(logf(args[0].number));
        } else if (strcmp(name, "EXP") == 0 && arg_count >= 1) {
          result = value_number(expf(args[0].number));
        } else if (strcmp(name, "ATN") == 0 && arg_count >= 1) {
          result = value_number(atanf(args[0].number));
        } else if (strcmp(name, "SGN") == 0 && arg_count >= 1) {
          if (args[0].number > 0) result = value_number(1.0f);
          else if (args[0].number < 0) result = value_number(-1.0f);
          else result = value_number(0.0f);
        } else {
          runtime_error(interp, "SYNTAX ERROR");
        }

        for (size_t i = 0; i < arg_count; i++) {
          value_free(&args[i]);
        }
        return result;
      }

      Value index_val = parse_expression(tz, interp);
      if (!token_is(tz, TOK_RPAREN, NULL)) {
        runtime_error(interp, "SYNTAX ERROR");
      } else {
        tokenizer_next(tz);
      }
      int idx = (int)index_val.number;
      value_free(&index_val);
      Array *array = arrays_find(interp, name);
      if (!array) {
        runtime_error(interp, "UNDEF'D STATEMENT");
        return value_number(0.0f);
      }
      if (idx < 0 || (size_t)idx >= array->size) {
        runtime_error(interp, "BAD SUBSCRIPT");
        return value_number(0.0f);
      }
      return value_copy(&array->values[idx]);
    }

    Variable *var = vars_get(interp, name, 1);
    if (!var) {
      runtime_error(interp, "OUT OF MEMORY");
      return value_number(0.0f);
    }
    return value_copy(&var->value);
  }
  if (token_is(tz, TOK_LPAREN, NULL)) {
    tokenizer_next(tz);
    Value val = parse_expression(tz, interp);
    if (token_is(tz, TOK_RPAREN, NULL)) {
      tokenizer_next(tz);
    } else {
      runtime_error(interp, "SYNTAX ERROR");
    }
    return val;
  }
  runtime_error(interp, "SYNTAX ERROR");
  return value_number(0.0f);
}

static Value parse_unary(Tokenizer *tz, BasicInterpreter *interp) {
  if (token_is(tz, TOK_OP, "+")) {
    tokenizer_next(tz);
    return parse_unary(tz, interp);
  }
  if (token_is(tz, TOK_OP, "-")) {
    tokenizer_next(tz);
    Value val = parse_unary(tz, interp);
    if (val.type == VAL_NUMBER) {
      val.number = -val.number;
      return val;
    }
    runtime_error(interp, "TYPE MISMATCH");
    return val;
  }
  return parse_primary(tz, interp);
}

static Value parse_power(Tokenizer *tz, BasicInterpreter *interp) {
  Value left = parse_unary(tz, interp);
  while (token_is(tz, TOK_OP, "^")) {
    tokenizer_next(tz);
    Value right = parse_unary(tz, interp);
    if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
      left.number = powf(left.number, right.number);
    } else {
      runtime_error(interp, "TYPE MISMATCH");
    }
    value_free(&right);
  }
  return left;
}

static Value parse_mul(Tokenizer *tz, BasicInterpreter *interp) {
  Value left = parse_power(tz, interp);
  while (token_is(tz, TOK_OP, "*") || token_is(tz, TOK_OP, "/")) {
    char op[2];
    strncpy(op, tz->current.text, sizeof(op));
    tokenizer_next(tz);
    Value right = parse_power(tz, interp);
    if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
      if (strcmp(op, "*") == 0) {
        left.number *= right.number;
      } else {
        if (right.number == 0.0f) {
          runtime_error(interp, "DIVISION BY ZERO");
        } else {
          left.number /= right.number;
        }
      }
    } else {
      runtime_error(interp, "TYPE MISMATCH");
    }
    value_free(&right);
  }
  return left;
}

static Value parse_add(Tokenizer *tz, BasicInterpreter *interp) {
  Value left = parse_mul(tz, interp);
  while (token_is(tz, TOK_OP, "+") || token_is(tz, TOK_OP, "-")) {
    char op[2];
    strncpy(op, tz->current.text, sizeof(op));
    tokenizer_next(tz);
    Value right = parse_mul(tz, interp);
    if (strcmp(op, "+") == 0) {
      if (left.type == VAL_STRING || right.type == VAL_STRING) {
        char *ls = value_to_string(&left);
        char *rs = value_to_string(&right);
        size_t len = strlen(ls) + strlen(rs);
        char *buf = (char *)malloc(len + 1);
        if (!buf) {
          runtime_error(interp, "OUT OF MEMORY");
          free(ls);
          free(rs);
          value_free(&left);
          value_free(&right);
          return value_number(0.0f);
        }
        memcpy(buf, ls, strlen(ls));
        memcpy(buf + strlen(ls), rs, strlen(rs));
        buf[len] = '\0';
        value_free(&left);
        value_free(&right);
        free(ls);
        free(rs);
        left = value_string(buf, len);
        free(buf);
      } else {
        left.number += right.number;
      }
    } else {
      if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
        left.number -= right.number;
      } else {
        runtime_error(interp, "TYPE MISMATCH");
      }
    }
    value_free(&right);
  }
  return left;
}

static Value parse_relational(Tokenizer *tz, BasicInterpreter *interp) {
  Value left = parse_add(tz, interp);
  while (token_is(tz, TOK_OP, "=") || token_is(tz, TOK_OP, "<>") || token_is(tz, TOK_OP, "<") ||
         token_is(tz, TOK_OP, "<=") || token_is(tz, TOK_OP, ">") || token_is(tz, TOK_OP, ">=")) {
    char op[3];
    strncpy(op, tz->current.text, sizeof(op));
    tokenizer_next(tz);
    Value right = parse_add(tz, interp);
    int result = 0;
    if (left.type == VAL_STRING || right.type == VAL_STRING) {
      char *ls = value_to_string(&left);
      char *rs = value_to_string(&right);
      int cmp = strcmp(ls, rs);
      if (strcmp(op, "=") == 0) result = (cmp == 0);
      else if (strcmp(op, "<>") == 0) result = (cmp != 0);
      else if (strcmp(op, "<") == 0) result = (cmp < 0);
      else if (strcmp(op, "<=") == 0) result = (cmp <= 0);
      else if (strcmp(op, ">") == 0) result = (cmp > 0);
      else if (strcmp(op, ">=") == 0) result = (cmp >= 0);
      free(ls);
      free(rs);
    } else {
      if (strcmp(op, "=") == 0) result = (left.number == right.number);
      else if (strcmp(op, "<>") == 0) result = (left.number != right.number);
      else if (strcmp(op, "<") == 0) result = (left.number < right.number);
      else if (strcmp(op, "<=") == 0) result = (left.number <= right.number);
      else if (strcmp(op, ">") == 0) result = (left.number > right.number);
      else if (strcmp(op, ">=") == 0) result = (left.number >= right.number);
    }
    value_free(&left);
    value_free(&right);
    left = value_number(result ? 1.0f : 0.0f);
  }
  return left;
}

static Value parse_expression(Tokenizer *tz, BasicInterpreter *interp) {
  return parse_relational(tz, interp);
}

static int parse_variable_target(Tokenizer *tz, BasicInterpreter *interp, char *out_name, size_t out_size, int *is_array, int *index) {
  if (tz->current.type != TOK_IDENT) {
    runtime_error(interp, "SYNTAX ERROR");
    return 0;
  }
  strncpy(out_name, tz->current.text, out_size);
  out_name[out_size - 1] = '\0';
  tokenizer_next(tz);
  if (token_is(tz, TOK_LPAREN, NULL)) {
    *is_array = 1;
    tokenizer_next(tz);
    Value idx_val = parse_expression(tz, interp);
    if (!token_is(tz, TOK_RPAREN, NULL)) {
      runtime_error(interp, "SYNTAX ERROR");
    } else {
      tokenizer_next(tz);
    }
    *index = (int)idx_val.number;
    value_free(&idx_val);
  } else {
    *is_array = 0;
    *index = 0;
  }
  return 1;
}

static void data_list_append(DataList *data, Value value) {
  if (data->count == data->capacity) {
    size_t new_cap = data->capacity == 0 ? 16 : data->capacity * 2;
    Value *new_items = (Value *)realloc(data->items, new_cap * sizeof(Value));
    if (!new_items) {
      value_free(&value);
      return;
    }
    data->items = new_items;
    data->capacity = new_cap;
  }
  data->items[data->count++] = value;
}

static void parse_data_statement(Tokenizer *tz, BasicInterpreter *interp, DataList *data) {
  while (tz->current.type != TOK_EOF) {
    if (tz->current.type == TOK_STRING) {
      Value val = value_string(tz->current.string, strlen(tz->current.string));
      data_list_append(data, val);
      tokenizer_next(tz);
    } else {
      Value val = parse_expression(tz, interp);
      data_list_append(data, val);
    }
    if (token_is(tz, TOK_COMMA, NULL)) {
      tokenizer_next(tz);
      continue;
    }
    break;
  }
}

static void parse_data_discard(Tokenizer *tz, BasicInterpreter *interp) {
  while (tz->current.type != TOK_EOF) {
    if (tz->current.type == TOK_STRING) {
      tokenizer_next(tz);
    } else {
      Value val = parse_expression(tz, interp);
      value_free(&val);
    }
    if (token_is(tz, TOK_COMMA, NULL)) {
      tokenizer_next(tz);
      continue;
    }
    break;
  }
}

static void reset_runtime(BasicInterpreter *interp) {
  interp->stop = 0;
  interp->error = 0;
  interp->error_msg[0] = '\0';
  interp->data.cursor = 0;
  for_stack_free(interp);
  gosub_stack_free(interp);
}

static void execute_statement(Tokenizer *tz, BasicInterpreter *interp, size_t *next_line_index);

static void execute_statement_list(Tokenizer *tz, BasicInterpreter *interp, size_t *next_line_index) {
  while (tz->current.type != TOK_EOF && !interp->stop && !interp->error) {
    execute_statement(tz, interp, next_line_index);
    if (interp->stop || interp->error) {
      return;
    }
    if (tz->current.type == TOK_COLON) {
      tokenizer_next(tz);
      continue;
    }
    break;
  }
}

static void execute_print(Tokenizer *tz, BasicInterpreter *interp) {
  int newline = 1;
  while (tz->current.type != TOK_EOF && tz->current.type != TOK_COLON) {
    if (tz->current.type == TOK_SEMICOLON) {
      newline = 0;
      tokenizer_next(tz);
      continue;
    }
    if (tz->current.type == TOK_COMMA) {
      printf(" ");
      tokenizer_next(tz);
      continue;
    }
    Value val = parse_expression(tz, interp);
    if (val.type == VAL_STRING) {
      printf("%s", val.string ? val.string : "");
    } else {
      printf("%g", val.number);
    }
    value_free(&val);
    newline = 1;
    if (tz->current.type == TOK_COMMA) {
      printf(" ");
      tokenizer_next(tz);
    } else if (tz->current.type == TOK_SEMICOLON) {
      newline = 0;
      tokenizer_next(tz);
    } else {
      break;
    }
  }
  if (newline) {
    printf("\n");
  }
}

static void execute_input(Tokenizer *tz, BasicInterpreter *interp) {
  char line[512];
  printf("? ");
  if (!fgets(line, sizeof(line), stdin)) {
    runtime_error(interp, "INPUT ERROR");
    return;
  }
  char *newline = strchr(line, '\n');
  if (newline) {
    *newline = '\0';
  }

  char *cursor = line;
  while (tz->current.type != TOK_EOF) {
    char name[128];
    int is_array = 0;
    int index = 0;
    if (!parse_variable_target(tz, interp, name, sizeof(name), &is_array, &index)) {
      return;
    }
    char *field = cursor;
    char *comma = strchr(cursor, ',');
    if (comma) {
      *comma = '\0';
      cursor = comma + 1;
    } else {
      cursor = line + strlen(line);
    }
    while (isspace((unsigned char)*field)) field++;
    char *end = field + strlen(field);
    while (end > field && isspace((unsigned char)end[-1])) {
      end[-1] = '\0';
      end--;
    }

    int is_string = name[strlen(name) - 1] == '$';
    Value value;
    if (is_string) {
      if (field[0] == '"' && end - field >= 2 && end[-1] == '"') {
        field++;
        end[-1] = '\0';
      }
      value = value_string(field, strlen(field));
    } else {
      value = value_number(strtof(field, NULL));
    }

    if (is_array) {
      Array *array = arrays_find(interp, name);
      if (!array || index < 0 || (size_t)index >= array->size) {
        value_free(&value);
        runtime_error(interp, "BAD SUBSCRIPT");
        return;
      }
      value_free(&array->values[index]);
      array->values[index] = value;
    } else {
      Variable *var = vars_get(interp, name, 1);
      if (!var) {
        value_free(&value);
        runtime_error(interp, "OUT OF MEMORY");
        return;
      }
      value_free(&var->value);
      var->value = value;
    }

    if (tz->current.type == TOK_COMMA) {
      tokenizer_next(tz);
      continue;
    }
    break;
  }
}

static void execute_let(Tokenizer *tz, BasicInterpreter *interp) {
  char name[128];
  int is_array = 0;
  int index = 0;
  if (!parse_variable_target(tz, interp, name, sizeof(name), &is_array, &index)) {
    return;
  }
  if (!token_is(tz, TOK_OP, "=")) {
    runtime_error(interp, "SYNTAX ERROR");
    return;
  }
  tokenizer_next(tz);
  Value value = parse_expression(tz, interp);
  if (interp->error) {
    value_free(&value);
    return;
  }
  int is_string = name[strlen(name) - 1] == '$';
  if (is_array) {
    Array *array = arrays_find(interp, name);
    if (!array) {
      value_free(&value);
      runtime_error(interp, "UNDEF'D STATEMENT");
      return;
    }
    if (index < 0 || (size_t)index >= array->size) {
      value_free(&value);
      runtime_error(interp, "BAD SUBSCRIPT");
      return;
    }
    if (is_string && value.type != VAL_STRING) {
      value_free(&value);
      runtime_error(interp, "TYPE MISMATCH");
      return;
    }
    if (!is_string && value.type != VAL_NUMBER) {
      value_free(&value);
      runtime_error(interp, "TYPE MISMATCH");
      return;
    }
    value_free(&array->values[index]);
    array->values[index] = value;
  } else {
    Variable *var = vars_get(interp, name, 1);
    if (!var) {
      value_free(&value);
      runtime_error(interp, "OUT OF MEMORY");
      return;
    }
    if (is_string && value.type != VAL_STRING) {
      value_free(&value);
      runtime_error(interp, "TYPE MISMATCH");
      return;
    }
    if (!is_string && value.type != VAL_NUMBER) {
      value_free(&value);
      runtime_error(interp, "TYPE MISMATCH");
      return;
    }
    value_free(&var->value);
    var->value = value;
  }
}

static void execute_if(Tokenizer *tz, BasicInterpreter *interp, size_t *next_line_index) {
  Value condition = parse_expression(tz, interp);
  int truthy = (condition.type == VAL_NUMBER ? condition.number != 0.0f : condition.length != 0);
  value_free(&condition);
  if (!token_is(tz, TOK_IDENT, "THEN")) {
    runtime_error(interp, "SYNTAX ERROR");
    return;
  }
  tokenizer_next(tz);
  if (!truthy) {
    return;
  }
  if (tz->current.type == TOK_NUMBER) {
    int line_number = (int)tz->current.number;
    tokenizer_next(tz);
    int found = 0;
    size_t idx = program_find_index(&interp->program, line_number, &found);
    if (!found) {
      runtime_error(interp, "UNDEF'D STATEMENT");
      return;
    }
    *next_line_index = idx;
    return;
  }
  execute_statement_list(tz, interp, next_line_index);
}

static void execute_goto(BasicInterpreter *interp, size_t *next_line_index, int line_number) {
  int found = 0;
  size_t idx = program_find_index(&interp->program, line_number, &found);
  if (!found) {
    runtime_error(interp, "UNDEF'D STATEMENT");
    return;
  }
  *next_line_index = idx;
}

static void execute_for(Tokenizer *tz, BasicInterpreter *interp, size_t current_index) {
  char name[128];
  int is_array = 0;
  int index = 0;
  if (!parse_variable_target(tz, interp, name, sizeof(name), &is_array, &index)) {
    return;
  }
  if (is_array) {
    runtime_error(interp, "SYNTAX ERROR");
    return;
  }
  if (!token_is(tz, TOK_OP, "=")) {
    runtime_error(interp, "SYNTAX ERROR");
    return;
  }
  tokenizer_next(tz);
  Value start = parse_expression(tz, interp);
  if (!token_is(tz, TOK_IDENT, "TO")) {
    value_free(&start);
    runtime_error(interp, "SYNTAX ERROR");
    return;
  }
  tokenizer_next(tz);
  Value limit = parse_expression(tz, interp);
  Value step = value_number(1.0f);
  if (token_is(tz, TOK_IDENT, "STEP")) {
    tokenizer_next(tz);
    value_free(&step);
    step = parse_expression(tz, interp);
  }

  Variable *var = vars_get(interp, name, 1);
  if (!var) {
    value_free(&start);
    value_free(&limit);
    value_free(&step);
    runtime_error(interp, "OUT OF MEMORY");
    return;
  }
  value_free(&var->value);
  var->value = value_number(start.number);

  if (interp->for_count == interp->for_capacity) {
    size_t new_cap = interp->for_capacity == 0 ? 8 : interp->for_capacity * 2;
    ForFrame *new_stack = (ForFrame *)realloc(interp->for_stack, new_cap * sizeof(ForFrame));
    if (!new_stack) {
      runtime_error(interp, "OUT OF MEMORY");
      value_free(&start);
      value_free(&limit);
      value_free(&step);
      return;
    }
    interp->for_stack = new_stack;
    interp->for_capacity = new_cap;
  }

  interp->for_stack[interp->for_count].name = strdup(name);
  interp->for_stack[interp->for_count].limit = limit.number;
  interp->for_stack[interp->for_count].step = step.number;
  interp->for_stack[interp->for_count].return_index = current_index + 1;
  interp->for_count++;

  value_free(&start);
  value_free(&limit);
  value_free(&step);
}

static void execute_next(Tokenizer *tz, BasicInterpreter *interp, size_t *next_line_index) {
  char name[128] = {0};
  if (tz->current.type == TOK_IDENT) {
    strncpy(name, tz->current.text, sizeof(name));
    tokenizer_next(tz);
  }
  if (interp->for_count == 0) {
    runtime_error(interp, "NEXT WITHOUT FOR");
    return;
  }
  ForFrame *frame = &interp->for_stack[interp->for_count - 1];
  if (name[0] != '\0' && strcmp(frame->name, name) != 0) {
    runtime_error(interp, "NEXT WITHOUT FOR");
    return;
  }
  Variable *var = vars_get(interp, frame->name, 0);
  if (!var || var->value.type != VAL_NUMBER) {
    runtime_error(interp, "TYPE MISMATCH");
    return;
  }
  var->value.number += frame->step;
  int continue_loop = frame->step >= 0 ? (var->value.number <= frame->limit) : (var->value.number >= frame->limit);
  if (continue_loop) {
    *next_line_index = frame->return_index;
  } else {
    free(frame->name);
    interp->for_count--;
  }
}

static void execute_dim(Tokenizer *tz, BasicInterpreter *interp) {
  while (tz->current.type != TOK_EOF) {
    if (tz->current.type != TOK_IDENT) {
      runtime_error(interp, "SYNTAX ERROR");
      return;
    }
    char name[128];
    strncpy(name, tz->current.text, sizeof(name));
    tokenizer_next(tz);
    if (!token_is(tz, TOK_LPAREN, NULL)) {
      runtime_error(interp, "SYNTAX ERROR");
      return;
    }
    tokenizer_next(tz);
    Value size_val = parse_expression(tz, interp);
    if (!token_is(tz, TOK_RPAREN, NULL)) {
      value_free(&size_val);
      runtime_error(interp, "SYNTAX ERROR");
      return;
    }
    tokenizer_next(tz);
    int size = (int)size_val.number;
    if (size < 0) size = 0;
    Array *array = arrays_get(interp, name, 1, name[strlen(name) - 1] == '$');
    if (!array) {
      value_free(&size_val);
      runtime_error(interp, "OUT OF MEMORY");
      return;
    }
    for (size_t i = 0; i < array->size; i++) {
      value_free(&array->values[i]);
    }
    free(array->values);
    array->size = (size_t)size + 1;
    array->values = (Value *)calloc(array->size, sizeof(Value));
    if (!array->values) {
      runtime_error(interp, "OUT OF MEMORY");
      return;
    }
    for (size_t i = 0; i < array->size; i++) {
      array->values[i] = array->is_string ? value_string("", 0) : value_number(0.0f);
    }
    value_free(&size_val);
    if (tz->current.type == TOK_COMMA) {
      tokenizer_next(tz);
      continue;
    }
    break;
  }
}

static void execute_read(Tokenizer *tz, BasicInterpreter *interp) {
  while (tz->current.type != TOK_EOF) {
    char name[128];
    int is_array = 0;
    int index = 0;
    if (!parse_variable_target(tz, interp, name, sizeof(name), &is_array, &index)) {
      return;
    }
    if (interp->data.cursor >= interp->data.count) {
      runtime_error(interp, "OUT OF DATA");
      return;
    }
    Value value = value_copy(&interp->data.items[interp->data.cursor++]);
    int is_string = name[strlen(name) - 1] == '$';
    if (is_array) {
      Array *array = arrays_find(interp, name);
      if (!array || index < 0 || (size_t)index >= array->size) {
        value_free(&value);
        runtime_error(interp, "BAD SUBSCRIPT");
        return;
      }
      if (is_string && value.type != VAL_STRING) {
        value_free(&value);
        runtime_error(interp, "TYPE MISMATCH");
        return;
      }
      if (!is_string && value.type != VAL_NUMBER) {
        value_free(&value);
        runtime_error(interp, "TYPE MISMATCH");
        return;
      }
      value_free(&array->values[index]);
      array->values[index] = value;
    } else {
      Variable *var = vars_get(interp, name, 1);
      if (!var) {
        value_free(&value);
        runtime_error(interp, "OUT OF MEMORY");
        return;
      }
      if (is_string && value.type != VAL_STRING) {
        value_free(&value);
        runtime_error(interp, "TYPE MISMATCH");
        return;
      }
      if (!is_string && value.type != VAL_NUMBER) {
        value_free(&value);
        runtime_error(interp, "TYPE MISMATCH");
        return;
      }
      value_free(&var->value);
      var->value = value;
    }
    if (tz->current.type == TOK_COMMA) {
      tokenizer_next(tz);
      continue;
    }
    break;
  }
}

static void execute_statement(Tokenizer *tz, BasicInterpreter *interp, size_t *next_line_index) {
  if (tz->current.type == TOK_EOF) {
    return;
  }
  if (token_is(tz, TOK_IDENT, "PRINT")) {
    tokenizer_next(tz);
    execute_print(tz, interp);
    return;
  }
  if (token_is(tz, TOK_IDENT, "INPUT")) {
    tokenizer_next(tz);
    execute_input(tz, interp);
    return;
  }
  if (token_is(tz, TOK_IDENT, "LET")) {
    tokenizer_next(tz);
    execute_let(tz, interp);
    return;
  }
  if (token_is(tz, TOK_IDENT, "IF")) {
    tokenizer_next(tz);
    execute_if(tz, interp, next_line_index);
    return;
  }
  if (token_is(tz, TOK_IDENT, "GOTO")) {
    tokenizer_next(tz);
    if (tz->current.type != TOK_NUMBER) {
      runtime_error(interp, "SYNTAX ERROR");
      return;
    }
    int line_number = (int)tz->current.number;
    tokenizer_next(tz);
    execute_goto(interp, next_line_index, line_number);
    return;
  }
  if (token_is(tz, TOK_IDENT, "GOSUB")) {
    tokenizer_next(tz);
    if (tz->current.type != TOK_NUMBER) {
      runtime_error(interp, "SYNTAX ERROR");
      return;
    }
    int line_number = (int)tz->current.number;
    tokenizer_next(tz);
    if (interp->gosub_count == interp->gosub_capacity) {
      size_t new_cap = interp->gosub_capacity == 0 ? 8 : interp->gosub_capacity * 2;
      GosubFrame *new_stack = (GosubFrame *)realloc(interp->gosub_stack, new_cap * sizeof(GosubFrame));
      if (!new_stack) {
        runtime_error(interp, "OUT OF MEMORY");
        return;
      }
      interp->gosub_stack = new_stack;
      interp->gosub_capacity = new_cap;
    }
    interp->gosub_stack[interp->gosub_count++].return_index = *next_line_index;
    execute_goto(interp, next_line_index, line_number);
    return;
  }
  if (token_is(tz, TOK_IDENT, "RETURN")) {
    tokenizer_next(tz);
    if (interp->gosub_count == 0) {
      runtime_error(interp, "RETURN WITHOUT GOSUB");
      return;
    }
    *next_line_index = interp->gosub_stack[--interp->gosub_count].return_index;
    return;
  }
  if (token_is(tz, TOK_IDENT, "FOR")) {
    tokenizer_next(tz);
    execute_for(tz, interp, *next_line_index - 1);
    return;
  }
  if (token_is(tz, TOK_IDENT, "NEXT")) {
    tokenizer_next(tz);
    execute_next(tz, interp, next_line_index);
    return;
  }
  if (token_is(tz, TOK_IDENT, "DIM")) {
    tokenizer_next(tz);
    execute_dim(tz, interp);
    return;
  }
  if (token_is(tz, TOK_IDENT, "READ")) {
    tokenizer_next(tz);
    execute_read(tz, interp);
    return;
  }
  if (token_is(tz, TOK_IDENT, "DATA")) {
    tokenizer_next(tz);
    if (interp->in_program) {
      parse_data_discard(tz, interp);
    } else {
      parse_data_statement(tz, interp, &interp->data);
    }
    return;
  }
  if (token_is(tz, TOK_IDENT, "RESTORE")) {
    tokenizer_next(tz);
    interp->data.cursor = 0;
    return;
  }
  if (token_is(tz, TOK_IDENT, "RUN")) {
    tokenizer_next(tz);
    basic_run(interp);
    return;
  }
  if (token_is(tz, TOK_IDENT, "LIST")) {
    tokenizer_next(tz);
    basic_list(interp);
    return;
  }
  if (token_is(tz, TOK_IDENT, "NEW")) {
    tokenizer_next(tz);
    basic_new(interp);
    return;
  }
  if (token_is(tz, TOK_IDENT, "END") || token_is(tz, TOK_IDENT, "STOP")) {
    tokenizer_next(tz);
    interp->stop = 1;
    return;
  }
  if (token_is(tz, TOK_IDENT, "LOAD")) {
    tokenizer_next(tz);
    Value path_val = parse_expression(tz, interp);
    if (path_val.type != VAL_STRING) {
      value_free(&path_val);
      runtime_error(interp, "TYPE MISMATCH");
      return;
    }
    basic_load_file(interp, path_val.string, 1);
    value_free(&path_val);
    return;
  }

  if (tz->current.type == TOK_IDENT) {
    execute_let(tz, interp);
    return;
  }

  runtime_error(interp, "SYNTAX ERROR");
}

static void collect_data(BasicInterpreter *interp) {
  data_list_free(&interp->data);
  for (size_t i = 0; i < interp->program.count; i++) {
    Tokenizer tz;
    tokenizer_init(&tz, interp->program.lines[i].text);
    tokenizer_next(&tz);
    while (tz.current.type != TOK_EOF) {
      if (token_is(&tz, TOK_IDENT, "DATA")) {
        tokenizer_next(&tz);
        parse_data_statement(&tz, interp, &interp->data);
        break;
      }
      if (tz.current.type == TOK_COLON) {
        tokenizer_next(&tz);
        continue;
      }
      tokenizer_next(&tz);
    }
  }
}

BasicInterpreter *basic_create(void) {
  BasicInterpreter *interp = (BasicInterpreter *)calloc(1, sizeof(BasicInterpreter));
  if (!interp) {
    return NULL;
  }
  program_init(&interp->program);
  interp->vars = NULL;
  interp->arrays = NULL;
  interp->for_stack = NULL;
  interp->gosub_stack = NULL;
  interp->data.items = NULL;
  interp->data.count = 0;
  interp->data.capacity = 0;
  interp->data.cursor = 0;
  return interp;
}

void basic_destroy(BasicInterpreter *interp) {
  if (!interp) {
    return;
  }
  program_free(&interp->program);
  vars_free(interp);
  arrays_free(interp);
  for_stack_free(interp);
  gosub_stack_free(interp);
  data_list_free(&interp->data);
  free(interp);
}

void basic_list(BasicInterpreter *interp) {
  for (size_t i = 0; i < interp->program.count; i++) {
    printf("%d %s\n", interp->program.lines[i].number, interp->program.lines[i].text);
  }
}

void basic_new(BasicInterpreter *interp) {
  program_free(&interp->program);
  program_init(&interp->program);
  vars_free(interp);
  arrays_free(interp);
  data_list_free(&interp->data);
}

int basic_load_file(BasicInterpreter *interp, const char *path, int replace_program) {
  FILE *file = fopen(path, "r");
  if (!file) {
    runtime_error(interp, "FILE NOT FOUND");
    print_error(interp);
    interp->error = 0;
    return 0;
  }
  if (replace_program) {
    basic_new(interp);
  }
  char line[1024];
  while (fgets(line, sizeof(line), file)) {
    char *newline = strchr(line, '\n');
    if (newline) {
      *newline = '\0';
    }
    char *cursor = line;
    while (isspace((unsigned char)*cursor)) cursor++;
    if (!isdigit((unsigned char)*cursor)) {
      continue;
    }
    int number = (int)strtol(cursor, &cursor, 10);
    while (isspace((unsigned char)*cursor)) cursor++;
    program_set_line(&interp->program, number, cursor);
  }
  fclose(file);
  return 1;
}

void basic_run(BasicInterpreter *interp) {
  reset_runtime(interp);
  collect_data(interp);
  interp->in_program = 1;
  size_t index = 0;
  while (index < interp->program.count && !interp->stop) {
    ProgramLine *line = &interp->program.lines[index];
    interp->current_line_number = line->number;
    size_t next_index = index + 1;
    Tokenizer tz;
    tokenizer_init(&tz, line->text);
    tokenizer_next(&tz);
    execute_statement_list(&tz, interp, &next_index);
    if (interp->error) {
      print_error(interp);
      interp->error = 0;
      break;
    }
    index = next_index;
  }
  interp->in_program = 0;
}

static void execute_immediate(BasicInterpreter *interp, const char *input) {
  Tokenizer tz;
  tokenizer_init(&tz, input);
  tokenizer_next(&tz);
  size_t next_index = 0;
  interp->in_program = 0;
  execute_statement_list(&tz, interp, &next_index);
  if (interp->error) {
    print_error(interp);
    interp->error = 0;
  }
}

void basic_repl(BasicInterpreter *interp) {
  char line[1024];
  while (fgets(line, sizeof(line), stdin)) {
    char *newline = strchr(line, '\n');
    if (newline) {
      *newline = '\0';
    }
    char *cursor = line;
    while (isspace((unsigned char)*cursor)) cursor++;
    if (*cursor == '\0') {
      continue;
    }
    if (isdigit((unsigned char)*cursor)) {
      int number = (int)strtol(cursor, &cursor, 10);
      while (isspace((unsigned char)*cursor)) cursor++;
      program_set_line(&interp->program, number, cursor);
    } else {
      execute_immediate(interp, cursor);
    }
  }
}
