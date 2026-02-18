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
      UserFunction *user_fn = functions_find(interp, name);
      if (user_fn) {
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

      if (strcmp(name, "LEFT$") == 0 || strcmp(name, "MID$") == 0 || strcmp(name, "RIGHT$") == 0 ||
          strcmp(name, "LEN") == 0 || strcmp(name, "ASC") == 0 || strcmp(name, "CHR$") == 0 ||
          strcmp(name, "ABS") == 0 || strcmp(name, "INT") == 0 || strcmp(name, "RND") == 0 ||
          strcmp(name, "SIN") == 0 || strcmp(name, "COS") == 0 || strcmp(name, "TAN") == 0 ||
          strcmp(name, "SQR") == 0 || strcmp(name, "LOG") == 0 || strcmp(name, "EXP") == 0 ||
          strcmp(name, "ATN") == 0 || strcmp(name, "SGN") == 0) {
        Value args[3] = {0};
        size_t min_args = 0;
        size_t max_args = 0;
        size_t arg_count = 0;
        int arity_known = builtin_arity(name, &min_args, &max_args);
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

        int bad_arity = 0;
        if (!arity_known) {
          bad_arity = 1;
        }
        if (too_many_args) {
          bad_arity = 1;
        }
        if (arg_count < min_args || arg_count > max_args) {
          bad_arity = 1;
        }

        Value result = value_number(0.0f);
        if (bad_arity) {
          runtime_error(interp, "BAD ARGUMENTS");
        } else if (strcmp(name, "LEFT$") == 0) {
          char *s = value_to_string(&args[0]);
          int n = (int)args[1].number;
          size_t len = strlen(s);
          if (n < 0) n = 0;
          if ((size_t)n > len) n = (int)len;
          result = value_string(s, (size_t)n);
          free(s);
        } else if (strcmp(name, "RIGHT$") == 0) {
          char *s = value_to_string(&args[0]);
          int n = (int)args[1].number;
          size_t len = strlen(s);
          if (n < 0) n = 0;
          if ((size_t)n > len) n = (int)len;
          result = value_string(s + (len - (size_t)n), (size_t)n);
          free(s);
        } else if (strcmp(name, "MID$") == 0) {
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
        } else if (strcmp(name, "LEN") == 0) {
          char *s = value_to_string(&args[0]);
          result = value_number((float)strlen(s));
          free(s);
        } else if (strcmp(name, "ASC") == 0) {
          char *s = value_to_string(&args[0]);
          result = value_number(s[0] ? (float)(unsigned char)s[0] : 0.0f);
          free(s);
        } else if (strcmp(name, "CHR$") == 0) {
          char ch = (char)((int)args[0].number & 0xFF);
          result = value_string(&ch, 1);
        } else if (strcmp(name, "ABS") == 0) {
          result = value_number(fabsf(args[0].number));
        } else if (strcmp(name, "INT") == 0) {
          result = value_number(floorf(args[0].number));
        } else if (strcmp(name, "RND") == 0) {
          if (arg_count > 0 && args[0].type != VAL_NUMBER) {
            runtime_error(interp, "TYPE MISMATCH");
          } else {
            float arg = arg_count > 0 ? args[0].number : 1.0f;
            result = value_number(basic_rnd(interp, arg_count > 0, arg));
          }
        } else if (strcmp(name, "SIN") == 0) {
          result = value_number(sinf(args[0].number));
        } else if (strcmp(name, "COS") == 0) {
          result = value_number(cosf(args[0].number));
        } else if (strcmp(name, "TAN") == 0) {
          result = value_number(tanf(args[0].number));
        } else if (strcmp(name, "SQR") == 0) {
          result = value_number(sqrtf(args[0].number));
        } else if (strcmp(name, "LOG") == 0) {
          result = value_number(logf(args[0].number));
        } else if (strcmp(name, "EXP") == 0) {
          result = value_number(expf(args[0].number));
        } else if (strcmp(name, "ATN") == 0) {
          result = value_number(atanf(args[0].number));
        } else if (strcmp(name, "SGN") == 0) {
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
