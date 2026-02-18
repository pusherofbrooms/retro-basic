#ifndef TERMINAL_H
#define TERMINAL_H

#include <stddef.h>

typedef struct BasicTerminal BasicTerminal;

BasicTerminal *basic_terminal_create_stdio(void);
void basic_terminal_destroy(BasicTerminal *term);
int basic_terminal_has_ansi(const BasicTerminal *term);

void basic_terminal_clear(BasicTerminal *term);
void basic_terminal_move(BasicTerminal *term, int row, int column);
void basic_terminal_color(BasicTerminal *term, int foreground, int background);
void basic_terminal_write(BasicTerminal *term, const char *text);
void basic_terminal_write_n(BasicTerminal *term, const char *text, size_t len);
void basic_terminal_printf(BasicTerminal *term, const char *fmt, ...);
void basic_terminal_flush(BasicTerminal *term);
char *basic_terminal_read_line(BasicTerminal *term, char *buffer, size_t size);
int basic_terminal_read_key_nonblocking(BasicTerminal *term, char *out);
int basic_terminal_read_key_blocking(BasicTerminal *term, char *out);

#endif
