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

static void reset_runtime(BasicInterpreter *interp, int reset_variables) {
  interp->stop = 0;
  interp->error = 0;
  interp->error_msg[0] = '\0';
  interp->data.cursor = 0;
  for_stack_free(interp);
  gosub_stack_free(interp);
  if (reset_variables) {
    vars_free(interp);
    arrays_free(interp);
  }
}

static void execute_statement(Tokenizer *tz, BasicInterpreter *interp, size_t *next_line_index);

static void execute_statement_list(Tokenizer *tz, BasicInterpreter *interp, size_t *next_line_index) {
  while (tz->current.type != TOK_EOF && !interp->stop && !interp->error) {
    execute_statement(tz, interp, next_line_index);
    if (interp->stop || interp->error) {
      return;
    }
    if (tz->current.type != TOK_EOF && tz->current.type != TOK_COLON) {
      runtime_error(interp, "SYNTAX ERROR");
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
    if (interp->error) {
      value_free(&val);
      return;
    }
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
  if (start.type != VAL_NUMBER) {
    value_free(&start);
    runtime_error(interp, "TYPE MISMATCH");
    return;
  }
  if (!token_is(tz, TOK_IDENT, "TO")) {
    value_free(&start);
    runtime_error(interp, "SYNTAX ERROR");
    return;
  }
  tokenizer_next(tz);
  Value limit = parse_expression(tz, interp);
  if (limit.type != VAL_NUMBER) {
    value_free(&start);
    value_free(&limit);
    runtime_error(interp, "TYPE MISMATCH");
    return;
  }
  Value step = value_number(1.0f);
  if (token_is(tz, TOK_IDENT, "STEP")) {
    tokenizer_next(tz);
    value_free(&step);
    step = parse_expression(tz, interp);
    if (step.type != VAL_NUMBER) {
      value_free(&start);
      value_free(&limit);
      value_free(&step);
      runtime_error(interp, "TYPE MISMATCH");
      return;
    }
    if (step.number == 0.0f) {
      value_free(&start);
      value_free(&limit);
      value_free(&step);
      runtime_error(interp, "BAD ARGUMENTS");
      return;
    }
  }
  if (name[strlen(name) - 1] == '$') {
    value_free(&start);
    value_free(&limit);
    value_free(&step);
    runtime_error(interp, "TYPE MISMATCH");
    return;
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
  reset_runtime(interp, 1);
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
