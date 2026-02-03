#ifndef BASIC_H
#define BASIC_H

#include <stddef.h>

typedef struct BasicInterpreter BasicInterpreter;

BasicInterpreter *basic_create(void);
void basic_destroy(BasicInterpreter *interp);

void basic_repl(BasicInterpreter *interp);

int basic_load_file(BasicInterpreter *interp, const char *path, int replace_program);
void basic_run(BasicInterpreter *interp);
void basic_list(BasicInterpreter *interp);
void basic_new(BasicInterpreter *interp);

#endif
