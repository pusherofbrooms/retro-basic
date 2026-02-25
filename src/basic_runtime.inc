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
  interp->key_line_pos = 0;
  interp->key_line_len = 0;
  for_stack_free(interp);
  gosub_stack_free(interp);
  if (reset_variables) {
    vars_free(interp);
    arrays_free(interp);
    functions_free(interp);
    interp->can_continue = 0;
    interp->continue_index = 0;
  }
}

static int basic_save_file(BasicInterpreter *interp, const char *path);
static void basic_list_filtered(BasicInterpreter *interp, int has_start, int start_line, int has_end, int end_line);
static void basic_delete_filtered(BasicInterpreter *interp, int has_start, int start_line, int has_end, int end_line);
static void basic_find_text(BasicInterpreter *interp, const char *needle);
static int parse_line_range(Tokenizer *tz, BasicInterpreter *interp, int allow_single, int *has_start, int *start_line, int *has_end, int *end_line);
static void basic_continue(BasicInterpreter *interp);

static int parse_line_range(Tokenizer *tz, BasicInterpreter *interp, int allow_single,
                            int *has_start, int *start_line, int *has_end, int *end_line) {
  *has_start = 0;
  *has_end = 0;
  *start_line = 0;
  *end_line = 0;

  if (tz->current.type == TOK_NUMBER) {
    *has_start = 1;
    *start_line = (int)tz->current.number;
    tokenizer_next(tz);
  }

  if (token_is(tz, TOK_OP, "-")) {
    tokenizer_next(tz);
    if (tz->current.type == TOK_NUMBER) {
      *has_end = 1;
      *end_line = (int)tz->current.number;
      tokenizer_next(tz);
    } else if (tz->current.type != TOK_EOF) {
      runtime_error(interp, "SYNTAX ERROR");
      return 0;
    }
    if (!*has_start && !*has_end) {
      runtime_error(interp, "SYNTAX ERROR");
      return 0;
    }
  } else if (*has_start && allow_single) {
    *has_end = 1;
    *end_line = *start_line;
  }

  if (!*has_start && !*has_end) {
    runtime_error(interp, "SYNTAX ERROR");
    return 0;
  }

  if (tz->current.type != TOK_EOF) {
    runtime_error(interp, "SYNTAX ERROR");
    return 0;
  }

  return 1;
}

static void execute_statement(Tokenizer *tz, BasicInterpreter *interp, size_t *next_line_index);

static void execute_def(Tokenizer *tz, BasicInterpreter *interp) {
  if (tz->current.type != TOK_IDENT || strncmp(tz->current.text, "FN", 2) != 0) {
    runtime_error(interp, "SYNTAX ERROR");
    return;
  }
  char fn_name[128];
  strncpy(fn_name, tz->current.text, sizeof(fn_name));
  fn_name[sizeof(fn_name) - 1] = '\0';
  tokenizer_next(tz);
  if (!token_is(tz, TOK_LPAREN, NULL)) {
    runtime_error(interp, "SYNTAX ERROR");
    return;
  }
  tokenizer_next(tz);
  if (tz->current.type != TOK_IDENT) {
    runtime_error(interp, "SYNTAX ERROR");
    return;
  }
  char arg_name[128];
  strncpy(arg_name, tz->current.text, sizeof(arg_name));
  arg_name[sizeof(arg_name) - 1] = '\0';
  tokenizer_next(tz);
  if (!token_is(tz, TOK_RPAREN, NULL)) {
    runtime_error(interp, "SYNTAX ERROR");
    return;
  }
  tokenizer_next(tz);
  if (!token_is(tz, TOK_OP, "=")) {
    runtime_error(interp, "SYNTAX ERROR");
    return;
  }
  tokenizer_next(tz);
  if (tz->current.type == TOK_EOF || tz->current.type == TOK_COLON) {
    runtime_error(interp, "SYNTAX ERROR");
    return;
  }
  size_t expr_start = tz->token_start;
  while (tz->current.type != TOK_EOF && tz->current.type != TOK_COLON) {
    tokenizer_next(tz);
  }
  size_t expr_end = tz->token_start;
  while (expr_end > expr_start && isspace((unsigned char)tz->input[expr_end - 1])) {
    expr_end--;
  }
  if (expr_end <= expr_start) {
    runtime_error(interp, "SYNTAX ERROR");
    return;
  }
  size_t expr_len = expr_end - expr_start;
  char *expr = (char *)malloc(expr_len + 1);
  if (!expr) {
    runtime_error(interp, "OUT OF MEMORY");
    return;
  }
  memcpy(expr, tz->input + expr_start, expr_len);
  expr[expr_len] = '\0';
  int ok = functions_set(interp, fn_name, arg_name, expr);
  free(expr);
  if (!ok) {
    runtime_error(interp, "OUT OF MEMORY");
  }
}

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
      basic_terminal_write(interp->terminal, " ");
      tokenizer_next(tz);
      continue;
    }
    Value val = parse_expression(tz, interp);
    if (interp->error) {
      value_free(&val);
      return;
    }
    if (val.type == VAL_STRING) {
      basic_terminal_printf(interp->terminal, "%s", val.string ? val.string : "");
    } else {
      basic_terminal_printf(interp->terminal, "%g", val.number);
    }
    value_free(&val);
    newline = 1;
    if (tz->current.type == TOK_COMMA) {
      basic_terminal_write(interp->terminal, " ");
      tokenizer_next(tz);
    } else if (tz->current.type == TOK_SEMICOLON) {
      newline = 0;
      tokenizer_next(tz);
    } else {
      break;
    }
  }
  if (newline) {
    basic_terminal_write(interp->terminal, "\n");
  }
  basic_terminal_flush(interp->terminal);
}

static void execute_input(Tokenizer *tz, BasicInterpreter *interp) {
  char line[512];
  basic_terminal_write(interp->terminal, "? ");
  basic_terminal_flush(interp->terminal);
  if (!basic_terminal_read_line(interp->terminal, line, sizeof(line))) {
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

static int parse_row_column_args(Tokenizer *tz, BasicInterpreter *interp, int *row, int *column) {
  Value row_val = parse_expression(tz, interp);
  Value col_val;
  if (interp->error) {
    value_free(&row_val);
    return 0;
  }
  if (row_val.type != VAL_NUMBER) {
    value_free(&row_val);
    runtime_error(interp, "TYPE MISMATCH");
    return 0;
  }
  if (!token_is(tz, TOK_COMMA, NULL)) {
    value_free(&row_val);
    runtime_error(interp, "SYNTAX ERROR");
    return 0;
  }
  tokenizer_next(tz);
  col_val = parse_expression(tz, interp);
  if (interp->error) {
    value_free(&row_val);
    value_free(&col_val);
    return 0;
  }
  if (col_val.type != VAL_NUMBER) {
    value_free(&row_val);
    value_free(&col_val);
    runtime_error(interp, "TYPE MISMATCH");
    return 0;
  }
  *row = (int)row_val.number;
  *column = (int)col_val.number;
  value_free(&row_val);
  value_free(&col_val);
  return 1;
}

static void execute_cls(BasicInterpreter *interp) {
  basic_screen_clear(interp->screen);
  basic_terminal_clear(interp->terminal);
  basic_terminal_flush(interp->terminal);
}

static void execute_locate(Tokenizer *tz, BasicInterpreter *interp) {
  int row = 1;
  int column = 1;
  if (!parse_row_column_args(tz, interp, &row, &column)) {
    return;
  }
  basic_screen_set_cursor(interp->screen, row, column);
  basic_terminal_move(interp->terminal, row, column);
  basic_terminal_flush(interp->terminal);
}

static void execute_color(Tokenizer *tz, BasicInterpreter *interp) {
  Value fg_val = parse_expression(tz, interp);
  Value bg_val;
  int fg;
  int bg = -1;
  if (interp->error) {
    value_free(&fg_val);
    return;
  }
  if (fg_val.type != VAL_NUMBER) {
    value_free(&fg_val);
    runtime_error(interp, "TYPE MISMATCH");
    return;
  }
  fg = (int)fg_val.number;
  value_free(&fg_val);

  if (token_is(tz, TOK_COMMA, NULL)) {
    tokenizer_next(tz);
    bg_val = parse_expression(tz, interp);
    if (interp->error) {
      value_free(&bg_val);
      return;
    }
    if (bg_val.type != VAL_NUMBER) {
      value_free(&bg_val);
      runtime_error(interp, "TYPE MISMATCH");
      return;
    }
    bg = (int)bg_val.number;
    value_free(&bg_val);
  }

  basic_screen_set_color(interp->screen, fg, bg);
  basic_terminal_color(interp->terminal, fg, bg);
  basic_terminal_flush(interp->terminal);
}

static void execute_print_at(Tokenizer *tz, BasicInterpreter *interp) {
  int row = 1;
  int column = 1;
  int newline = 1;
  if (!parse_row_column_args(tz, interp, &row, &column)) {
    return;
  }

  basic_screen_set_cursor(interp->screen, row, column);
  if (token_is(tz, TOK_COMMA, NULL)) {
    tokenizer_next(tz);
  }

  while (tz->current.type != TOK_EOF && tz->current.type != TOK_COLON) {
    if (tz->current.type == TOK_SEMICOLON) {
      newline = 0;
      tokenizer_next(tz);
      continue;
    }
    if (tz->current.type == TOK_COMMA) {
      basic_screen_write_char(interp->screen, ' ');
      tokenizer_next(tz);
      continue;
    }

    Value val = parse_expression(tz, interp);
    if (interp->error) {
      value_free(&val);
      return;
    }
    if (val.type == VAL_STRING) {
      basic_screen_write_text(interp->screen, val.string ? val.string : "");
    } else {
      char number_text[64];
      snprintf(number_text, sizeof(number_text), "%g", val.number);
      basic_screen_write_text(interp->screen, number_text);
    }
    value_free(&val);
    newline = 1;
    if (tz->current.type == TOK_COMMA) {
      basic_screen_write_char(interp->screen, ' ');
      tokenizer_next(tz);
    } else if (tz->current.type == TOK_SEMICOLON) {
      newline = 0;
      tokenizer_next(tz);
    } else {
      break;
    }
  }
  if (newline) {
    basic_screen_write_char(interp->screen, '\n');
  }
  basic_screen_present(interp->screen, interp->terminal);
}

static char screen_command_char(Value *value) {
  char result = '*';
  if (value->type == VAL_STRING) {
    if (value->string && value->string[0] != '\0') {
      result = value->string[0];
    }
    return result;
  }
  result = (char)((int)value->number & 0xFF);
  if (result == '\0') {
    result = '*';
  }
  return result;
}

static void execute_plot(Tokenizer *tz, BasicInterpreter *interp) {
  int row = 1;
  int column = 1;
  char ch = '*';
  if (!parse_row_column_args(tz, interp, &row, &column)) {
    return;
  }
  if (token_is(tz, TOK_COMMA, NULL)) {
    Value char_val;
    tokenizer_next(tz);
    char_val = parse_expression(tz, interp);
    if (interp->error) {
      value_free(&char_val);
      return;
    }
    ch = screen_command_char(&char_val);
    value_free(&char_val);
  }
  basic_screen_put(interp->screen, row, column, ch);
  basic_screen_present(interp->screen, interp->terminal);
}

static void execute_line(Tokenizer *tz, BasicInterpreter *interp) {
  int row0 = 1;
  int col0 = 1;
  int row1 = 1;
  int col1 = 1;
  int dr;
  int dc;
  int step_r;
  int step_c;
  int err;
  char ch = '*';
  if (!parse_row_column_args(tz, interp, &row0, &col0)) {
    return;
  }
  if (!token_is(tz, TOK_COMMA, NULL)) {
    runtime_error(interp, "SYNTAX ERROR");
    return;
  }
  tokenizer_next(tz);
  if (!parse_row_column_args(tz, interp, &row1, &col1)) {
    return;
  }
  if (token_is(tz, TOK_COMMA, NULL)) {
    Value char_val;
    tokenizer_next(tz);
    char_val = parse_expression(tz, interp);
    if (interp->error) {
      value_free(&char_val);
      return;
    }
    ch = screen_command_char(&char_val);
    value_free(&char_val);
  }

  dr = abs(row1 - row0);
  dc = abs(col1 - col0);
  step_r = row0 < row1 ? 1 : -1;
  step_c = col0 < col1 ? 1 : -1;
  err = (dr > dc ? dr : -dc) / 2;

  while (1) {
    int err2;
    basic_screen_put(interp->screen, row0, col0, ch);
    if (row0 == row1 && col0 == col1) {
      break;
    }
    err2 = err;
    if (err2 > -dr) {
      err -= dc;
      row0 += step_r;
    }
    if (err2 < dc) {
      err += dr;
      col0 += step_c;
    }
  }
  basic_screen_present(interp->screen, interp->terminal);
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

static int gosub_push(BasicInterpreter *interp, size_t return_index) {
  if (interp->gosub_count == interp->gosub_capacity) {
    size_t new_cap = interp->gosub_capacity == 0 ? 8 : interp->gosub_capacity * 2;
    GosubFrame *new_stack = (GosubFrame *)realloc(interp->gosub_stack, new_cap * sizeof(GosubFrame));
    if (!new_stack) {
      runtime_error(interp, "OUT OF MEMORY");
      return 0;
    }
    interp->gosub_stack = new_stack;
    interp->gosub_capacity = new_cap;
  }
  interp->gosub_stack[interp->gosub_count++].return_index = return_index;
  return 1;
}

static void execute_on(Tokenizer *tz, BasicInterpreter *interp, size_t *next_line_index) {
  Value selector = parse_expression(tz, interp);
  if (interp->error) {
    value_free(&selector);
    return;
  }
  if (selector.type != VAL_NUMBER) {
    value_free(&selector);
    runtime_error(interp, "TYPE MISMATCH");
    return;
  }
  int branch = (int)selector.number;
  value_free(&selector);

  if (!token_is(tz, TOK_IDENT, "GOTO") && !token_is(tz, TOK_IDENT, "GOSUB")) {
    runtime_error(interp, "SYNTAX ERROR");
    return;
  }
  int is_gosub = token_is(tz, TOK_IDENT, "GOSUB");
  tokenizer_next(tz);

  int ordinal = 1;
  int target_line = 0;
  int has_target = 0;
  while (1) {
    if (tz->current.type != TOK_NUMBER) {
      runtime_error(interp, "SYNTAX ERROR");
      return;
    }
    if (ordinal == branch) {
      target_line = (int)tz->current.number;
      has_target = 1;
    }
    ordinal++;
    tokenizer_next(tz);
    if (token_is(tz, TOK_COMMA, NULL)) {
      tokenizer_next(tz);
      continue;
    }
    break;
  }

  if (!has_target || branch < 1) {
    return;
  }
  if (is_gosub && !gosub_push(interp, *next_line_index)) {
    return;
  }
  execute_goto(interp, next_line_index, target_line);
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
  if (token_is(tz, TOK_IDENT, "REM")) {
    tokenizer_next(tz);
    while (tz->current.type != TOK_EOF) {
      tokenizer_next(tz);
    }
    return;
  }
  if (token_is(tz, TOK_IDENT, "PRINT")) {
    tokenizer_next(tz);
    if (token_is(tz, TOK_OP, "@")) {
      tokenizer_next(tz);
      execute_print_at(tz, interp);
      return;
    }
    execute_print(tz, interp);
    return;
  }
  if (token_is(tz, TOK_IDENT, "CLS")) {
    tokenizer_next(tz);
    execute_cls(interp);
    return;
  }
  if (token_is(tz, TOK_IDENT, "LOCATE")) {
    tokenizer_next(tz);
    execute_locate(tz, interp);
    return;
  }
  if (token_is(tz, TOK_IDENT, "COLOR")) {
    tokenizer_next(tz);
    execute_color(tz, interp);
    return;
  }
  if (token_is(tz, TOK_IDENT, "PLOT")) {
    tokenizer_next(tz);
    execute_plot(tz, interp);
    return;
  }
  if (token_is(tz, TOK_IDENT, "LINE")) {
    tokenizer_next(tz);
    execute_line(tz, interp);
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
    if (!gosub_push(interp, *next_line_index)) {
      return;
    }
    execute_goto(interp, next_line_index, line_number);
    return;
  }
  if (token_is(tz, TOK_IDENT, "ON")) {
    tokenizer_next(tz);
    execute_on(tz, interp, next_line_index);
    return;
  }
  if (token_is(tz, TOK_IDENT, "DEF")) {
    tokenizer_next(tz);
    execute_def(tz, interp);
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
  if (token_is(tz, TOK_IDENT, "CONT")) {
    tokenizer_next(tz);
    basic_continue(interp);
    return;
  }
  if (token_is(tz, TOK_IDENT, "TRON")) {
    tokenizer_next(tz);
    interp->trace_enabled = 1;
    return;
  }
  if (token_is(tz, TOK_IDENT, "TROFF")) {
    tokenizer_next(tz);
    interp->trace_enabled = 0;
    return;
  }
  if (token_is(tz, TOK_IDENT, "RANDOMIZE")) {
    tokenizer_next(tz);
    if (tz->current.type == TOK_EOF || tz->current.type == TOK_COLON) {
      basic_randomize(interp, 0, 0.0f);
      return;
    }
    Value seed = parse_expression(tz, interp);
    if (interp->error) {
      value_free(&seed);
      return;
    }
    if (seed.type != VAL_NUMBER) {
      value_free(&seed);
      runtime_error(interp, "TYPE MISMATCH");
      return;
    }
    basic_randomize(interp, 1, seed.number);
    value_free(&seed);
    return;
  }
  if (token_is(tz, TOK_IDENT, "LIST")) {
    tokenizer_next(tz);
    if (tz->current.type == TOK_EOF) {
      basic_list(interp);
      return;
    }

    int has_start = 0;
    int has_end = 0;
    int start_line = 0;
    int end_line = 0;
    if (!parse_line_range(tz, interp, 1, &has_start, &start_line, &has_end, &end_line)) {
      return;
    }
    basic_list_filtered(interp, has_start, start_line, has_end, end_line);
    return;
  }
  if (token_is(tz, TOK_IDENT, "DELETE")) {
    tokenizer_next(tz);
    int has_start = 0;
    int has_end = 0;
    int start_line = 0;
    int end_line = 0;
    if (!parse_line_range(tz, interp, 1, &has_start, &start_line, &has_end, &end_line)) {
      return;
    }
    basic_delete_filtered(interp, has_start, start_line, has_end, end_line);
    return;
  }
  if (token_is(tz, TOK_IDENT, "FIND")) {
    tokenizer_next(tz);
    Value pattern = parse_expression(tz, interp);
    if (interp->error) {
      value_free(&pattern);
      return;
    }
    if (pattern.type != VAL_STRING) {
      value_free(&pattern);
      runtime_error(interp, "TYPE MISMATCH");
      return;
    }
    if (tz->current.type != TOK_EOF) {
      value_free(&pattern);
      runtime_error(interp, "SYNTAX ERROR");
      return;
    }
    basic_find_text(interp, pattern.string ? pattern.string : "");
    value_free(&pattern);
    return;
  }
  if (token_is(tz, TOK_IDENT, "NEW")) {
    tokenizer_next(tz);
    basic_new(interp);
    return;
  }
  if (token_is(tz, TOK_IDENT, "END")) {
    tokenizer_next(tz);
    interp->can_continue = 0;
    interp->stop = 1;
    return;
  }
  if (token_is(tz, TOK_IDENT, "STOP")) {
    tokenizer_next(tz);
    interp->can_continue = 1;
    interp->continue_index = *next_line_index;
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
  if (token_is(tz, TOK_IDENT, "SAVE")) {
    tokenizer_next(tz);
    Value path_val = parse_expression(tz, interp);
    if (path_val.type != VAL_STRING) {
      value_free(&path_val);
      runtime_error(interp, "TYPE MISMATCH");
      return;
    }
    basic_save_file(interp, path_val.string);
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
    tokenizer_init_cached(&tz, interp->program.lines[i].text,
                          interp->program.lines[i].tokens,
                          interp->program.lines[i].token_count);
    tokenizer_next(&tz);
    while (tz.current.type != TOK_EOF) {
      if (token_is(&tz, TOK_IDENT, "REM")) {
        break;
      }
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
  interp->terminal = basic_terminal_create_stdio();
  if (!interp->terminal) {
    free(interp);
    return NULL;
  }
  interp->screen = basic_screen_create(80, 25);
  if (!interp->screen) {
    basic_terminal_destroy(interp->terminal);
    free(interp);
    return NULL;
  }
  program_init(&interp->program);
  interp->vars = NULL;
  interp->arrays = NULL;
  interp->for_stack = NULL;
  interp->gosub_stack = NULL;
  interp->functions = NULL;
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
  functions_free(interp);
  for_stack_free(interp);
  gosub_stack_free(interp);
  data_list_free(&interp->data);
  basic_screen_destroy(interp->screen);
  basic_terminal_destroy(interp->terminal);
  free(interp);
}

void basic_list(BasicInterpreter *interp) {
  basic_list_filtered(interp, 0, 0, 0, 0);
}

static void basic_list_filtered(BasicInterpreter *interp, int has_start, int start_line, int has_end, int end_line) {
  for (size_t i = 0; i < interp->program.count; i++) {
    int line_number = interp->program.lines[i].number;
    if (has_start && line_number < start_line) {
      continue;
    }
    if (has_end && line_number > end_line) {
      continue;
    }
    basic_terminal_printf(interp->terminal, "%d %s\n", interp->program.lines[i].number, interp->program.lines[i].text);
  }
  basic_terminal_flush(interp->terminal);
}

static void basic_delete_filtered(BasicInterpreter *interp, int has_start, int start_line, int has_end, int end_line) {
  size_t i = 0;
  while (i < interp->program.count) {
    int line_number = interp->program.lines[i].number;
    if (has_start && line_number < start_line) {
      i++;
      continue;
    }
    if (has_end && line_number > end_line) {
      i++;
      continue;
    }
    program_delete_line(&interp->program, i);
  }
}

static int line_contains_text_case_insensitive(const char *line, const char *needle) {
  size_t needle_len = strlen(needle);
  if (needle_len == 0) {
    return 1;
  }

  for (size_t i = 0; line[i] != '\0'; i++) {
    size_t j = 0;
    while (needle[j] != '\0' && line[i + j] != '\0' &&
           toupper((unsigned char)line[i + j]) == toupper((unsigned char)needle[j])) {
      j++;
    }
    if (j == needle_len) {
      return 1;
    }
  }

  return 0;
}

static void basic_find_text(BasicInterpreter *interp, const char *needle) {
  for (size_t i = 0; i < interp->program.count; i++) {
    ProgramLine *line = &interp->program.lines[i];
    if (line_contains_text_case_insensitive(line->text, needle)) {
      basic_terminal_printf(interp->terminal, "%d %s\n", line->number, line->text);
    }
  }
  basic_terminal_flush(interp->terminal);
}

void basic_new(BasicInterpreter *interp) {
  program_free(&interp->program);
  program_init(&interp->program);
  vars_free(interp);
  arrays_free(interp);
  functions_free(interp);
  data_list_free(&interp->data);
  interp->can_continue = 0;
  interp->continue_index = 0;
  interp->key_line_pos = 0;
  interp->key_line_len = 0;
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
    if (!program_set_line(&interp->program, number, cursor)) {
      fclose(file);
      runtime_error(interp, "SYNTAX ERROR");
      print_error(interp);
      interp->error = 0;
      return 0;
    }
  }
  fclose(file);
  return 1;
}

static int basic_save_file(BasicInterpreter *interp, const char *path) {
  FILE *file = fopen(path, "w");
  if (!file) {
    runtime_error(interp, "FILE ERROR");
    print_error(interp);
    interp->error = 0;
    return 0;
  }

  for (size_t i = 0; i < interp->program.count; i++) {
    if (fprintf(file, "%d %s\n", interp->program.lines[i].number, interp->program.lines[i].text) < 0) {
      fclose(file);
      runtime_error(interp, "FILE ERROR");
      print_error(interp);
      interp->error = 0;
      return 0;
    }
  }

  if (fclose(file) != 0) {
    runtime_error(interp, "FILE ERROR");
    print_error(interp);
    interp->error = 0;
    return 0;
  }
  return 1;
}

static void run_from_index(BasicInterpreter *interp, size_t index) {
  interp->in_program = 1;
  interp->stop = 0;
  interp->error = 0;
  interp->error_msg[0] = '\0';
  while (index < interp->program.count && !interp->stop) {
    ProgramLine *line = &interp->program.lines[index];
    interp->current_line_number = line->number;
    if (interp->trace_enabled) {
      basic_terminal_printf(interp->terminal, "TRACE %d\n", line->number);
    }
    size_t next_index = index + 1;
    Tokenizer tz;
    tokenizer_init_cached(&tz, line->text, line->tokens, line->token_count);
    tokenizer_next(&tz);
    execute_statement_list(&tz, interp, &next_index);
    if (interp->error) {
      print_error(interp);
      interp->error = 0;
      interp->can_continue = 0;
      break;
    }
    index = next_index;
  }
  int stopped = interp->stop;
  interp->in_program = 0;
  if (!stopped) {
    interp->can_continue = 0;
  }
  interp->stop = 0;
}

void basic_run(BasicInterpreter *interp) {
  reset_runtime(interp, 1);
  collect_data(interp);
  run_from_index(interp, 0);
}

static void basic_continue(BasicInterpreter *interp) {
  if (!interp->can_continue) {
    runtime_error(interp, "CAN'T CONTINUE");
    return;
  }
  size_t index = interp->continue_index;
  interp->can_continue = 0;
  run_from_index(interp, index);
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
  while (basic_terminal_read_line(interp->terminal, line, sizeof(line))) {
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
      if (!program_set_line(&interp->program, number, cursor)) {
        runtime_error(interp, "SYNTAX ERROR");
        print_error(interp);
        interp->error = 0;
      }
    } else {
      execute_immediate(interp, cursor);
    }
  }
}
