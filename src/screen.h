#ifndef SCREEN_H
#define SCREEN_H

#include <stddef.h>

#include "terminal.h"

typedef struct BasicScreen BasicScreen;

BasicScreen *basic_screen_create(size_t width, size_t height);
void basic_screen_destroy(BasicScreen *screen);

void basic_screen_clear(BasicScreen *screen);
void basic_screen_set_cursor(BasicScreen *screen, int row, int column);
void basic_screen_set_color(BasicScreen *screen, int foreground, int background);
void basic_screen_write_char(BasicScreen *screen, char ch);
void basic_screen_write_text(BasicScreen *screen, const char *text);
void basic_screen_put(BasicScreen *screen, int row, int column, char ch);
void basic_screen_present(BasicScreen *screen, BasicTerminal *terminal);

size_t basic_screen_width(const BasicScreen *screen);
size_t basic_screen_height(const BasicScreen *screen);

#endif
