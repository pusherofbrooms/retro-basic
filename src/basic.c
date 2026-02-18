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

static int builtin_arity(const char *name, size_t *min_args, size_t *max_args) {
  if (strcmp(name, "LEFT$") == 0 || strcmp(name, "RIGHT$") == 0) {
    *min_args = 2;
    *max_args = 2;
    return 1;
  }
  if (strcmp(name, "MID$") == 0) {
    *min_args = 2;
    *max_args = 3;
    return 1;
  }
  if (strcmp(name, "RND") == 0) {
    *min_args = 0;
    *max_args = 1;
    return 1;
  }
  if (strcmp(name, "LEN") == 0 || strcmp(name, "ASC") == 0 || strcmp(name, "CHR$") == 0 ||
      strcmp(name, "ABS") == 0 || strcmp(name, "INT") == 0 || strcmp(name, "SIN") == 0 ||
      strcmp(name, "COS") == 0 || strcmp(name, "TAN") == 0 || strcmp(name, "SQR") == 0 ||
      strcmp(name, "LOG") == 0 || strcmp(name, "EXP") == 0 || strcmp(name, "ATN") == 0 ||
      strcmp(name, "SGN") == 0) {
    *min_args = 1;
    *max_args = 1;
    return 1;
  }
  return 0;
}

#include "basic_parser.inc"

#include "basic_runtime.inc"
