#include "basic.h"
#include "screen.h"
#include "terminal.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
  char *name;
  char *arg_name;
  char *expr;
} UserFunction;

typedef struct Token Token;

typedef struct {
  int number;
  char *text;
  Token *tokens;
  size_t token_count;
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
  TOK_COLON,
  TOK_INVALID
} TokenType;

struct Token {
  TokenType type;
  char text[128];
  float number;
  char string[256];
  size_t start;
};

typedef struct {
  const char *input;
  size_t pos;
  size_t token_start;
  Token current;
  const Token *cached_tokens;
  size_t cached_count;
  size_t cached_index;
  int use_cached_tokens;
} Tokenizer;

struct BasicInterpreter {
  Program program;
  Variable *vars;
  size_t var_count;
  size_t var_capacity;
  Array *arrays;
  size_t array_count;
  size_t array_capacity;
  UserFunction *functions;
  size_t function_count;
  size_t function_capacity;
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
  int can_continue;
  size_t continue_index;
  int trace_enabled;
  uint32_t rng_state;
  int rng_seeded;
  float rnd_last;
  int rnd_has_last;
  BasicTerminal *terminal;
  BasicScreen *screen;
  char key_line[512];
  size_t key_line_pos;
  size_t key_line_len;
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

static int tokenize_line(const char *input, Token **out_tokens, size_t *out_count);

static void program_line_free(ProgramLine *line) {
  free(line->text);
  free(line->tokens);
  line->text = NULL;
  line->tokens = NULL;
  line->token_count = 0;
}

static void program_free(Program *program) {
  for (size_t i = 0; i < program->count; i++) {
    program_line_free(&program->lines[i]);
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
  program_line_free(&program->lines[index]);
  for (size_t i = index + 1; i < program->count; i++) {
    program->lines[i - 1] = program->lines[i];
  }
  program->count--;
}

static int program_set_line(Program *program, int number, const char *text) {
  int found = 0;
  size_t index = program_find_index(program, number, &found);
  if (!text || text[0] == '\0') {
    if (found) {
      program_delete_line(program, index);
    }
    return 1;
  }

  char *new_text = strdup(text);
  Token *new_tokens = NULL;
  size_t new_token_count = 0;
  if (!new_text) {
    return 0;
  }
  if (!tokenize_line(text, &new_tokens, &new_token_count)) {
    free(new_text);
    return 0;
  }

  if (found) {
    program_line_free(&program->lines[index]);
    program->lines[index].text = new_text;
    program->lines[index].tokens = new_tokens;
    program->lines[index].token_count = new_token_count;
    return 1;
  }

  if (program->count == program->capacity) {
    size_t new_cap = program->capacity == 0 ? 16 : program->capacity * 2;
    ProgramLine *new_lines = (ProgramLine *)realloc(program->lines, new_cap * sizeof(ProgramLine));
    if (!new_lines) {
      free(new_text);
      free(new_tokens);
      return 0;
    }
    program->lines = new_lines;
    program->capacity = new_cap;
  }

  for (size_t i = program->count; i > index; i--) {
    program->lines[i] = program->lines[i - 1];
  }
  program->lines[index].number = number;
  program->lines[index].text = new_text;
  program->lines[index].tokens = new_tokens;
  program->lines[index].token_count = new_token_count;
  program->count++;
  return 1;
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
  tz->token_start = 0;
  tz->current.type = TOK_EOF;
  tz->current.text[0] = '\0';
  tz->current.string[0] = '\0';
  tz->current.number = 0.0f;
  tz->current.start = 0;
  tz->cached_tokens = NULL;
  tz->cached_count = 0;
  tz->cached_index = 0;
  tz->use_cached_tokens = 0;
}

static void tokenizer_init_cached(Tokenizer *tz, const char *input, const Token *tokens, size_t token_count) {
  tokenizer_init(tz, input);
  tz->cached_tokens = tokens;
  tz->cached_count = token_count;
  tz->use_cached_tokens = 1;
}

static void tokenizer_skip_spaces(Tokenizer *tz) {
  while (tz->input[tz->pos] && isspace((unsigned char)tz->input[tz->pos])) {
    tz->pos++;
  }
}

static void tokenizer_next(Tokenizer *tz) {
  if (tz->use_cached_tokens) {
    if (tz->cached_index >= tz->cached_count) {
      tz->current.type = TOK_EOF;
      tz->token_start = tz->input ? strlen(tz->input) : 0;
      return;
    }
    tz->current = tz->cached_tokens[tz->cached_index++];
    tz->token_start = tz->current.start;
    return;
  }

  tokenizer_skip_spaces(tz);
  tz->token_start = tz->pos;
  char c = tz->input[tz->pos];
  tz->current.text[0] = '\0';
  tz->current.string[0] = '\0';
  tz->current.number = 0.0f;
  tz->current.start = tz->token_start;
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
    if (tz->input[tz->pos] != '"') {
      tz->current.type = TOK_INVALID;
      strncpy(tz->current.text, "UNTERMINATED STRING", sizeof(tz->current.text));
      tz->current.text[sizeof(tz->current.text) - 1] = '\0';
      return;
    }
    size_t len = tz->pos - start;
    if (len >= sizeof(tz->current.string)) {
      len = sizeof(tz->current.string) - 1;
    }
    memcpy(tz->current.string, tz->input + start, len);
    tz->current.string[len] = '\0';
    tz->current.type = TOK_STRING;
    tz->pos++;
    return;
  }

  if (c == '\'') {
    tz->current.type = TOK_EOF;
    tz->pos = strlen(tz->input);
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
      tz->current.text[sizeof(tz->current.text) - 1] = '\0';
      tz->current.type = TOK_OP;
      return;
    case '>':
      if (tz->input[tz->pos] == '=') {
        tz->pos++;
        strncpy(tz->current.text, ">=", sizeof(tz->current.text));
      } else {
        strncpy(tz->current.text, ">", sizeof(tz->current.text));
      }
      tz->current.text[sizeof(tz->current.text) - 1] = '\0';
      tz->current.type = TOK_OP;
      return;
    case '=':
    case '+':
    case '-':
    case '*':
    case '/':
    case '^':
    case '@':
      tz->current.type = TOK_OP;
      tz->current.text[0] = c;
      tz->current.text[1] = '\0';
      return;
    default:
      tz->current.type = TOK_INVALID;
      tz->current.text[0] = c;
      tz->current.text[1] = '\0';
      return;
  }
}

static int tokenize_line(const char *input, Token **out_tokens, size_t *out_count) {
  Tokenizer tz;
  size_t count = 0;
  size_t capacity = 0;
  Token *tokens = NULL;

  tokenizer_init(&tz, input);
  while (1) {
    tokenizer_next(&tz);
    if (tz.current.type == TOK_INVALID) {
      free(tokens);
      return 0;
    }
    if (count == capacity) {
      size_t new_capacity = capacity == 0 ? 16 : capacity * 2;
      Token *new_tokens = (Token *)realloc(tokens, new_capacity * sizeof(Token));
      if (!new_tokens) {
        free(tokens);
        return 0;
      }
      tokens = new_tokens;
      capacity = new_capacity;
    }
    tokens[count++] = tz.current;
    if (tz.current.type == TOK_EOF) {
      break;
    }
  }

  *out_tokens = tokens;
  *out_count = count;
  return 1;
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
    basic_terminal_printf(interp->terminal, "%s IN %d\n", interp->error_msg, interp->current_line_number);
  } else {
    basic_terminal_printf(interp->terminal, "%s\n", interp->error_msg);
  }
  basic_terminal_flush(interp->terminal);
}

static int key_buffer_take(BasicInterpreter *interp, char *out) {
  if (interp->key_line_pos >= interp->key_line_len) {
    interp->key_line_pos = 0;
    interp->key_line_len = 0;
    return 0;
  }
  *out = interp->key_line[interp->key_line_pos++];
  if (interp->key_line_pos >= interp->key_line_len) {
    interp->key_line_pos = 0;
    interp->key_line_len = 0;
  }
  return 1;
}

static int key_buffer_fill_from_line(BasicInterpreter *interp) {
  char line[sizeof(interp->key_line)];
  char *newline;
  size_t len;

  if (!basic_terminal_read_line(interp->terminal, line, sizeof(line))) {
    return 0;
  }

  newline = strchr(line, '\n');
  if (newline) {
    *newline = '\0';
  }

  len = strlen(line);
  if (len == 0) {
    return 0;
  }
  if (len > sizeof(interp->key_line)) {
    len = sizeof(interp->key_line);
  }
  memcpy(interp->key_line, line, len);
  interp->key_line_len = len;
  interp->key_line_pos = 0;
  return 1;
}

static int basic_read_key(BasicInterpreter *interp, int blocking, char *out) {
  int status;

  if (key_buffer_take(interp, out)) {
    return 1;
  }

  status = basic_terminal_read_key_nonblocking(interp->terminal, out);
  if (status == 1) {
    return 1;
  }
  if (!blocking) {
    return 0;
  }

  if (status == 0) {
    status = basic_terminal_read_key_blocking(interp->terminal, out);
    if (status == 1) {
      return 1;
    }
  }

  while (interp->key_line_len == 0) {
    if (!key_buffer_fill_from_line(interp)) {
      return 0;
    }
  }
  return key_buffer_take(interp, out);
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

static UserFunction *functions_find(BasicInterpreter *interp, const char *name) {
  for (size_t i = 0; i < interp->function_count; i++) {
    if (strcmp(interp->functions[i].name, name) == 0) {
      return &interp->functions[i];
    }
  }
  return NULL;
}

static void functions_free(BasicInterpreter *interp) {
  for (size_t i = 0; i < interp->function_count; i++) {
    free(interp->functions[i].name);
    free(interp->functions[i].arg_name);
    free(interp->functions[i].expr);
  }
  free(interp->functions);
  interp->functions = NULL;
  interp->function_count = 0;
  interp->function_capacity = 0;
}

static int functions_set(BasicInterpreter *interp, const char *name, const char *arg_name, const char *expr) {
  UserFunction *existing = functions_find(interp, name);
  if (existing) {
    char *new_arg_name = strdup(arg_name);
    char *new_expr = strdup(expr);
    if (!new_arg_name || !new_expr) {
      free(new_arg_name);
      free(new_expr);
      return 0;
    }
    free(existing->arg_name);
    free(existing->expr);
    existing->arg_name = new_arg_name;
    existing->expr = new_expr;
    return 1;
  }

  if (interp->function_count == interp->function_capacity) {
    size_t new_cap = interp->function_capacity == 0 ? 8 : interp->function_capacity * 2;
    UserFunction *new_functions = (UserFunction *)realloc(interp->functions, new_cap * sizeof(UserFunction));
    if (!new_functions) {
      return 0;
    }
    interp->functions = new_functions;
    interp->function_capacity = new_cap;
  }

  UserFunction *fn = &interp->functions[interp->function_count];
  fn->name = strdup(name);
  fn->arg_name = strdup(arg_name);
  fn->expr = strdup(expr);
  if (!fn->name || !fn->arg_name || !fn->expr) {
    free(fn->name);
    free(fn->arg_name);
    free(fn->expr);
    return 0;
  }
  interp->function_count++;
  return 1;
}

static void vars_remove(BasicInterpreter *interp, const char *name) {
  for (size_t i = 0; i < interp->var_count; i++) {
    if (strcmp(interp->vars[i].name, name) != 0) {
      continue;
    }
    free(interp->vars[i].name);
    value_free(&interp->vars[i].value);
    for (size_t j = i + 1; j < interp->var_count; j++) {
      interp->vars[j - 1] = interp->vars[j];
    }
    interp->var_count--;
    return;
  }
}

static Value functions_eval(BasicInterpreter *interp, UserFunction *fn, const Value *arg) {
  Variable *existing = vars_find(interp, fn->arg_name);
  Value old_value = value_number(0.0f);
  int had_existing = existing != NULL;
  if (had_existing) {
    old_value = value_copy(&existing->value);
  }

  Variable *param = vars_get(interp, fn->arg_name, 1);
  if (!param) {
    runtime_error(interp, "OUT OF MEMORY");
    value_free(&old_value);
    return value_number(0.0f);
  }
  value_free(&param->value);
  param->value = value_copy(arg);

  Tokenizer fn_tz;
  tokenizer_init(&fn_tz, fn->expr);
  tokenizer_next(&fn_tz);
  Value result = parse_expression(&fn_tz, interp);
  if (!interp->error && fn_tz.current.type != TOK_EOF) {
    runtime_error(interp, "SYNTAX ERROR");
    value_free(&result);
    result = value_number(0.0f);
  }

  if (had_existing) {
    value_free(&param->value);
    param->value = old_value;
  } else {
    vars_remove(interp, fn->arg_name);
    value_free(&old_value);
  }
  return result;
}

static uint32_t rng_seed_from_number(float number) {
  int32_t seed = (int32_t)number;
  if (seed == 0) {
    return 1u;
  }
  return (uint32_t)seed;
}

static void rng_seed(BasicInterpreter *interp, uint32_t seed) {
  if (seed == 0) {
    seed = 1u;
  }
  interp->rng_state = seed;
  interp->rng_seeded = 1;
  interp->rnd_has_last = 0;
  interp->rnd_last = 0.0f;
}

static float rng_next(BasicInterpreter *interp) {
  if (!interp->rng_seeded) {
    rng_seed(interp, (uint32_t)time(NULL));
  }
  interp->rng_state = interp->rng_state * 1664525u + 1013904223u;
  float value = (float)(interp->rng_state >> 8) / 16777216.0f;
  interp->rnd_last = value;
  interp->rnd_has_last = 1;
  return value;
}

static float basic_rnd(BasicInterpreter *interp, int has_arg, float arg) {
  if (!has_arg || arg > 0.0f) {
    return rng_next(interp);
  }
  if (arg == 0.0f) {
    if (!interp->rnd_has_last) {
      return rng_next(interp);
    }
    return interp->rnd_last;
  }
  rng_seed(interp, rng_seed_from_number(-arg));
  return rng_next(interp);
}

static void basic_randomize(BasicInterpreter *interp, int has_seed, float seed) {
  if (has_seed) {
    rng_seed(interp, rng_seed_from_number(seed));
    return;
  }
  rng_seed(interp, (uint32_t)time(NULL));
}

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
  if (strcmp(name, "INKEY$") == 0 || strcmp(name, "GETKEY$") == 0) {
    *min_args = 0;
    *max_args = 0;
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
