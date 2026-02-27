typedef Value (*BuiltinEvalFn)(BasicInterpreter *interp, Value *args, size_t arg_count);

typedef struct {
  const char *name;
  size_t min_args;
  size_t max_args;
  BuiltinEvalFn evaluator;
} BuiltinDescriptor;

static Value eval_builtin_left(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)interp;
  (void)arg_count;
  char *s = value_to_string(&args[0]);
  int n = (int)args[1].number;
  size_t len = strlen(s);
  if (n < 0) n = 0;
  if ((size_t)n > len) n = (int)len;
  Value result = value_string(s, (size_t)n);
  free(s);
  return result;
}

static Value eval_builtin_right(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)interp;
  (void)arg_count;
  char *s = value_to_string(&args[0]);
  int n = (int)args[1].number;
  size_t len = strlen(s);
  if (n < 0) n = 0;
  if ((size_t)n > len) n = (int)len;
  Value result = value_string(s + (len - (size_t)n), (size_t)n);
  free(s);
  return result;
}

static Value eval_builtin_mid(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)interp;
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
  Value result = value_string(s + offset, use_len);
  free(s);
  return result;
}

static Value eval_builtin_len(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)interp;
  (void)arg_count;
  char *s = value_to_string(&args[0]);
  Value result = value_number((float)strlen(s));
  free(s);
  return result;
}

static Value eval_builtin_asc(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)interp;
  (void)arg_count;
  char *s = value_to_string(&args[0]);
  Value result = value_number(s[0] ? (float)(unsigned char)s[0] : 0.0f);
  free(s);
  return result;
}

static Value eval_builtin_chr(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)interp;
  (void)arg_count;
  char ch = (char)((int)args[0].number & 0xFF);
  return value_string(&ch, 1);
}

static Value eval_builtin_inkey(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)args;
  (void)arg_count;
  char ch;
  if (basic_read_key(interp, 0, &ch)) {
    return value_string(&ch, 1);
  }
  return value_string("", 0);
}

static Value eval_builtin_getkey(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)args;
  (void)arg_count;
  char ch;
  if (basic_read_key(interp, 1, &ch)) {
    return value_string(&ch, 1);
  }
  return value_string("", 0);
}

static Value eval_builtin_abs(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)interp;
  (void)arg_count;
  return value_number(fabsf(args[0].number));
}

static Value eval_builtin_int(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)interp;
  (void)arg_count;
  return value_number(floorf(args[0].number));
}

static Value eval_builtin_rnd(BasicInterpreter *interp, Value *args, size_t arg_count) {
  if (arg_count > 0 && args[0].type != VAL_NUMBER) {
    runtime_error(interp, "TYPE MISMATCH");
    return value_number(0.0f);
  }
  float arg = arg_count > 0 ? args[0].number : 1.0f;
  return value_number(basic_rnd(interp, arg_count > 0, arg));
}

static Value eval_builtin_sin(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)interp;
  (void)arg_count;
  return value_number(sinf(args[0].number));
}

static Value eval_builtin_cos(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)interp;
  (void)arg_count;
  return value_number(cosf(args[0].number));
}

static Value eval_builtin_tan(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)interp;
  (void)arg_count;
  return value_number(tanf(args[0].number));
}

static Value eval_builtin_sqr(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)interp;
  (void)arg_count;
  return value_number(sqrtf(args[0].number));
}

static Value eval_builtin_log(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)interp;
  (void)arg_count;
  return value_number(logf(args[0].number));
}

static Value eval_builtin_exp(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)interp;
  (void)arg_count;
  return value_number(expf(args[0].number));
}

static Value eval_builtin_atn(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)interp;
  (void)arg_count;
  return value_number(atanf(args[0].number));
}

static Value eval_builtin_sgn(BasicInterpreter *interp, Value *args, size_t arg_count) {
  (void)interp;
  (void)arg_count;
  if (args[0].number > 0) return value_number(1.0f);
  if (args[0].number < 0) return value_number(-1.0f);
  return value_number(0.0f);
}

static const BuiltinDescriptor BUILTIN_DESCRIPTORS[] = {
    {"LEFT$", 2, 2, eval_builtin_left},   {"MID$", 2, 3, eval_builtin_mid},
    {"RIGHT$", 2, 2, eval_builtin_right}, {"LEN", 1, 1, eval_builtin_len},
    {"ASC", 1, 1, eval_builtin_asc},      {"CHR$", 1, 1, eval_builtin_chr},
    {"INKEY$", 0, 0, eval_builtin_inkey}, {"GETKEY$", 0, 0, eval_builtin_getkey},
    {"ABS", 1, 1, eval_builtin_abs},      {"INT", 1, 1, eval_builtin_int},
    {"RND", 0, 1, eval_builtin_rnd},      {"SIN", 1, 1, eval_builtin_sin},
    {"COS", 1, 1, eval_builtin_cos},      {"TAN", 1, 1, eval_builtin_tan},
    {"SQR", 1, 1, eval_builtin_sqr},      {"LOG", 1, 1, eval_builtin_log},
    {"EXP", 1, 1, eval_builtin_exp},      {"ATN", 1, 1, eval_builtin_atn},
    {"SGN", 1, 1, eval_builtin_sgn},
};

static const BuiltinDescriptor *lookup_builtin_descriptor(const char *name) {
  for (size_t i = 0; i < sizeof(BUILTIN_DESCRIPTORS) / sizeof(BUILTIN_DESCRIPTORS[0]); i++) {
    if (strcmp(BUILTIN_DESCRIPTORS[i].name, name) == 0) {
      return &BUILTIN_DESCRIPTORS[i];
    }
  }
  return NULL;
}

static Value parse_builtin_function_call(Tokenizer *tz, BasicInterpreter *interp,
                                         const BuiltinDescriptor *builtin) {
  Value args[3] = {0};
  size_t arg_count = 0;
  int too_many_args = 0;

  if (!token_is(tz, TOK_RPAREN, NULL)) {
    while (1) {
      Value parsed = parse_expression(tz, interp);
      if (arg_count < sizeof(args) / sizeof(args[0])) {
        args[arg_count++] = parsed;
      } else {
        value_free(&parsed);
        arg_count++;
        too_many_args = 1;
      }
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

  int bad_arity = too_many_args;
  if (arg_count < builtin->min_args || arg_count > builtin->max_args) {
    bad_arity = 1;
  }

  Value result = value_number(0.0f);
  if (bad_arity) {
    runtime_error(interp, "BAD ARGUMENTS");
  } else {
    result = builtin->evaluator(interp, args, arg_count);
  }

  size_t stored_arg_count = arg_count;
  if (stored_arg_count > sizeof(args) / sizeof(args[0])) {
    stored_arg_count = sizeof(args) / sizeof(args[0]);
  }
  for (size_t i = 0; i < stored_arg_count; i++) {
    value_free(&args[i]);
  }
  return result;
}

static Value parse_user_function_call(Tokenizer *tz, BasicInterpreter *interp, UserFunction *user_fn) {
  if (token_is(tz, TOK_RPAREN, NULL)) {
    runtime_error(interp, "BAD ARGUMENTS");
    return value_number(0.0f);
  }
  Value arg = parse_expression(tz, interp);
  if (token_is(tz, TOK_COMMA, NULL)) {
    value_free(&arg);
    runtime_error(interp, "BAD ARGUMENTS");
    return value_number(0.0f);
  }
  if (!token_is(tz, TOK_RPAREN, NULL)) {
    value_free(&arg);
    runtime_error(interp, "SYNTAX ERROR");
    return value_number(0.0f);
  }
  tokenizer_next(tz);
  Value result = functions_eval(interp, user_fn, &arg);
  value_free(&arg);
  return result;
}

static Value parse_array_access(Tokenizer *tz, BasicInterpreter *interp, const char *name) {
  Value index1_val = parse_expression(tz, interp);
  int index1 = (int)index1_val.number;
  int has_index2 = 0;
  int index2 = 0;
  value_free(&index1_val);

  if (token_is(tz, TOK_COMMA, NULL)) {
    tokenizer_next(tz);
    Value index2_val = parse_expression(tz, interp);
    has_index2 = 1;
    index2 = (int)index2_val.number;
    value_free(&index2_val);
  }

  if (!token_is(tz, TOK_RPAREN, NULL)) {
    runtime_error(interp, "SYNTAX ERROR");
  } else {
    tokenizer_next(tz);
  }

  Array *array = arrays_find(interp, name);
  if (!array) {
    runtime_error(interp, "UNDEF'D STATEMENT");
    return value_number(0.0f);
  }
  size_t linear_index = 0;
  if (!array_resolve_index(array, index1, has_index2, index2, &linear_index)) {
    runtime_error(interp, "BAD SUBSCRIPT");
    return value_number(0.0f);
  }
  return value_copy(&array->values[linear_index]);
}

static Value parse_identifier_value(Tokenizer *tz, BasicInterpreter *interp) {
  char name[128];
  snprintf(name, sizeof(name), "%s", tz->current.text);
  tokenizer_next(tz);

  if (token_is(tz, TOK_LPAREN, NULL)) {
    tokenizer_next(tz);

    UserFunction *user_fn = functions_find(interp, name);
    if (user_fn) {
      return parse_user_function_call(tz, interp, user_fn);
    }

    const BuiltinDescriptor *builtin = lookup_builtin_descriptor(name);
    if (builtin) {
      return parse_builtin_function_call(tz, interp, builtin);
    }

    return parse_array_access(tz, interp, name);
  }

  Variable *var = vars_get(interp, name, 1);
  if (!var) {
    runtime_error(interp, "OUT OF MEMORY");
    return value_number(0.0f);
  }
  return value_copy(&var->value);
}

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
    return parse_identifier_value(tz, interp);
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
  if (token_is(tz, TOK_OP, "^")) {
    tokenizer_next(tz);
    Value right = parse_power(tz, interp);
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
    op[0] = tz->current.text[0];
    op[1] = '\0';
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
    op[0] = tz->current.text[0];
    op[1] = '\0';
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
    op[0] = tz->current.text[0];
    op[1] = tz->current.text[1] == '\0' ? '\0' : tz->current.text[1];
    op[2] = '\0';
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

static Value parse_not(Tokenizer *tz, BasicInterpreter *interp) {
  if (token_is(tz, TOK_IDENT, "NOT")) {
    tokenizer_next(tz);
    Value value = parse_not(tz, interp);
    if (value.type != VAL_NUMBER) {
      runtime_error(interp, "TYPE MISMATCH");
      return value;
    }
    value.number = value.number == 0.0f ? 1.0f : 0.0f;
    return value;
  }
  return parse_relational(tz, interp);
}

static Value parse_and(Tokenizer *tz, BasicInterpreter *interp) {
  Value left = parse_not(tz, interp);
  while (token_is(tz, TOK_IDENT, "AND")) {
    tokenizer_next(tz);
    Value right = parse_not(tz, interp);
    if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
      left.number = (left.number != 0.0f && right.number != 0.0f) ? 1.0f : 0.0f;
    } else {
      runtime_error(interp, "TYPE MISMATCH");
    }
    value_free(&right);
  }
  return left;
}

static Value parse_or(Tokenizer *tz, BasicInterpreter *interp) {
  Value left = parse_and(tz, interp);
  while (token_is(tz, TOK_IDENT, "OR")) {
    tokenizer_next(tz);
    Value right = parse_and(tz, interp);
    if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
      left.number = (left.number != 0.0f || right.number != 0.0f) ? 1.0f : 0.0f;
    } else {
      runtime_error(interp, "TYPE MISMATCH");
    }
    value_free(&right);
  }
  return left;
}

static Value parse_expression(Tokenizer *tz, BasicInterpreter *interp) {
  return parse_or(tz, interp);
}
