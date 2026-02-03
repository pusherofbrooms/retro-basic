#include "basic.h"

#include <stdio.h>

int main(void) {
  BasicInterpreter *interp = basic_create();
  if (!interp) {
    fprintf(stderr, "Failed to initialize interpreter.\n");
    return 1;
  }
  basic_repl(interp);
  basic_destroy(interp);
  return 0;
}
