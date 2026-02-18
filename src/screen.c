#include "screen.h"

#include "terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char ch;
  int foreground;
  int background;
} ScreenCell;

struct BasicScreen {
  size_t width;
  size_t height;
  ScreenCell *cells;
  ScreenCell *last_presented;
  int cursor_row;
  int cursor_column;
  int current_foreground;
  int current_background;
  int full_redraw;
};

static ScreenCell screen_default_cell(void) {
  ScreenCell cell;
  cell.ch = ' ';
  cell.foreground = -1;
  cell.background = -1;
  return cell;
}

static int screen_cell_equal(const ScreenCell *left, const ScreenCell *right) {
  return left->ch == right->ch &&
         left->foreground == right->foreground &&
         left->background == right->background;
}

static size_t screen_index(const BasicScreen *screen, int row, int column) {
  return (size_t)(row - 1) * screen->width + (size_t)(column - 1);
}

static void screen_clamp_cursor(BasicScreen *screen) {
  if (screen->cursor_row < 1) {
    screen->cursor_row = 1;
  } else if ((size_t)screen->cursor_row > screen->height) {
    screen->cursor_row = (int)screen->height;
  }

  if (screen->cursor_column < 1) {
    screen->cursor_column = 1;
  } else if ((size_t)screen->cursor_column > screen->width) {
    screen->cursor_column = (int)screen->width;
  }
}

BasicScreen *basic_screen_create(size_t width, size_t height) {
  BasicScreen *screen;
  ScreenCell fill;
  size_t count;
  size_t i;

  if (width == 0 || height == 0) {
    return NULL;
  }

  count = width * height;
  screen = (BasicScreen *)calloc(1, sizeof(BasicScreen));
  if (!screen) {
    return NULL;
  }

  screen->cells = (ScreenCell *)malloc(sizeof(ScreenCell) * count);
  screen->last_presented = (ScreenCell *)malloc(sizeof(ScreenCell) * count);
  if (!screen->cells || !screen->last_presented) {
    basic_screen_destroy(screen);
    return NULL;
  }

  screen->width = width;
  screen->height = height;
  screen->cursor_row = 1;
  screen->cursor_column = 1;
  screen->current_foreground = -1;
  screen->current_background = -1;
  screen->full_redraw = 1;

  fill = screen_default_cell();
  for (i = 0; i < count; i++) {
    screen->cells[i] = fill;
    screen->last_presented[i] = fill;
  }
  return screen;
}

void basic_screen_destroy(BasicScreen *screen) {
  if (!screen) {
    return;
  }
  free(screen->cells);
  free(screen->last_presented);
  free(screen);
}

void basic_screen_clear(BasicScreen *screen) {
  ScreenCell fill;
  size_t count;
  size_t i;

  if (!screen) {
    return;
  }

  fill = screen_default_cell();
  count = screen->width * screen->height;
  for (i = 0; i < count; i++) {
    screen->cells[i] = fill;
  }
  screen->cursor_row = 1;
  screen->cursor_column = 1;
  screen->full_redraw = 1;
}

void basic_screen_set_cursor(BasicScreen *screen, int row, int column) {
  if (!screen) {
    return;
  }
  screen->cursor_row = row;
  screen->cursor_column = column;
  screen_clamp_cursor(screen);
}

void basic_screen_set_color(BasicScreen *screen, int foreground, int background) {
  if (!screen) {
    return;
  }
  screen->current_foreground = foreground;
  screen->current_background = background;
}

void basic_screen_put(BasicScreen *screen, int row, int column, char ch) {
  size_t idx;
  ScreenCell *cell;

  if (!screen) {
    return;
  }
  if (row < 1 || column < 1) {
    return;
  }
  if ((size_t)row > screen->height || (size_t)column > screen->width) {
    return;
  }

  idx = screen_index(screen, row, column);
  cell = &screen->cells[idx];
  cell->ch = ch;
  cell->foreground = screen->current_foreground;
  cell->background = screen->current_background;
}

void basic_screen_write_char(BasicScreen *screen, char ch) {
  if (!screen) {
    return;
  }

  if (ch == '\r') {
    screen->cursor_column = 1;
    return;
  }
  if (ch == '\n') {
    screen->cursor_column = 1;
    screen->cursor_row++;
    if ((size_t)screen->cursor_row > screen->height) {
      screen->cursor_row = (int)screen->height;
    }
    return;
  }

  basic_screen_put(screen, screen->cursor_row, screen->cursor_column, ch);
  screen->cursor_column++;
  if ((size_t)screen->cursor_column > screen->width) {
    screen->cursor_column = 1;
    screen->cursor_row++;
    if ((size_t)screen->cursor_row > screen->height) {
      screen->cursor_row = (int)screen->height;
    }
  }
}

void basic_screen_write_text(BasicScreen *screen, const char *text) {
  if (!screen || !text) {
    return;
  }
  while (*text) {
    basic_screen_write_char(screen, *text);
    text++;
  }
}

void basic_screen_present(BasicScreen *screen, BasicTerminal *terminal) {
  const char *fallback_env;
  int active_foreground = -1000;
  int active_background = -1000;
  size_t row;
  size_t column;

  if (!screen || !terminal) {
    return;
  }

  fallback_env = getenv("BASIC_SCREEN_FALLBACK");
  if (!basic_terminal_has_ansi(terminal)) {
    if (!fallback_env || strcmp(fallback_env, "1") != 0) {
      return;
    }
    for (row = 1; row <= screen->height; row++) {
      int row_changed = 0;
      size_t idx_start;
      size_t idx_end;
      size_t trim_end;
      size_t i;
      char line[256];

      idx_start = screen_index(screen, (int)row, 1);
      idx_end = idx_start + screen->width;
      if (!row_changed) {
        for (i = idx_start; i < idx_end; i++) {
          if (!screen_cell_equal(&screen->cells[i], &screen->last_presented[i])) {
            row_changed = 1;
            break;
          }
        }
      }
      if (!row_changed) {
        continue;
      }

      if (screen->width >= sizeof(line)) {
        continue;
      }
      for (i = 0; i < screen->width; i++) {
        line[i] = screen->cells[idx_start + i].ch;
      }
      line[screen->width] = '\0';

      trim_end = screen->width;
      while (trim_end > 0 && line[trim_end - 1] == ' ') {
        trim_end--;
      }
      line[trim_end] = '\0';
      printf("SCREEN %zu %s\n", row, line);

      for (i = idx_start; i < idx_end; i++) {
        screen->last_presented[i] = screen->cells[i];
      }
    }
    screen->full_redraw = 0;
    basic_terminal_flush(terminal);
    return;
  }

  for (row = 1; row <= screen->height; row++) {
    for (column = 1; column <= screen->width; column++) {
      ScreenCell *cell;
      ScreenCell *previous;
      size_t idx;
      char out_ch;

      idx = screen_index(screen, (int)row, (int)column);
      cell = &screen->cells[idx];
      previous = &screen->last_presented[idx];
      if (!screen->full_redraw && screen_cell_equal(cell, previous)) {
        continue;
      }

      basic_terminal_move(terminal, (int)row, (int)column);
      if (cell->foreground != active_foreground || cell->background != active_background) {
        basic_terminal_color(terminal, cell->foreground, cell->background);
        active_foreground = cell->foreground;
        active_background = cell->background;
      }

      out_ch = cell->ch;
      basic_terminal_write_n(terminal, &out_ch, 1);
      *previous = *cell;
    }
  }

  screen->full_redraw = 0;
  basic_terminal_flush(terminal);
}

size_t basic_screen_width(const BasicScreen *screen) {
  return screen ? screen->width : 0;
}

size_t basic_screen_height(const BasicScreen *screen) {
  return screen ? screen->height : 0;
}
