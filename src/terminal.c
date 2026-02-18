#include "terminal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <conio.h>
#include <io.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

struct BasicTerminal {
  FILE *input;
  FILE *output;
  int ansi_enabled;
};

static int stream_is_tty(FILE *stream) {
#if defined(_WIN32)
  return _isatty(_fileno(stream)) != 0;
#else
  return isatty(fileno(stream)) != 0;
#endif
}

static int terminal_has_ansi_support(FILE *output) {
  const char *term_name;

  if (!stream_is_tty(output)) {
    return 0;
  }

#if defined(_WIN32)
  HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
  if (handle == INVALID_HANDLE_VALUE) {
    return 0;
  }

  DWORD mode = 0;
  if (!GetConsoleMode(handle, &mode)) {
    return 0;
  }
  if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0) {
    if (!SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
      return 0;
    }
  }
#endif

  term_name = getenv("TERM");
  if (!term_name || term_name[0] == '\0') {
    return 0;
  }
  return strcmp(term_name, "dumb") != 0;
}

static int color_to_ansi(int color, int background) {
  if (color < 0) {
    return background ? 49 : 39;
  }
  if (color <= 7) {
    return (background ? 40 : 30) + color;
  }
  if (color <= 15) {
    return (background ? 100 : 90) + (color - 8);
  }
  return background ? 49 : 39;
}

BasicTerminal *basic_terminal_create_stdio(void) {
  BasicTerminal *term = (BasicTerminal *)calloc(1, sizeof(BasicTerminal));
  if (!term) {
    return NULL;
  }
  term->input = stdin;
  term->output = stdout;
  term->ansi_enabled = terminal_has_ansi_support(term->output);
  return term;
}

void basic_terminal_destroy(BasicTerminal *term) {
  free(term);
}

int basic_terminal_has_ansi(const BasicTerminal *term) {
  return term ? term->ansi_enabled : 0;
}

void basic_terminal_clear(BasicTerminal *term) {
  if (!term || !term->ansi_enabled || !term->output) {
    return;
  }
  fputs("\x1b[2J\x1b[H", term->output);
}

void basic_terminal_move(BasicTerminal *term, int row, int column) {
  if (!term || !term->ansi_enabled || !term->output) {
    return;
  }
  if (row < 1) {
    row = 1;
  }
  if (column < 1) {
    column = 1;
  }
  fprintf(term->output, "\x1b[%d;%dH", row, column);
}

void basic_terminal_color(BasicTerminal *term, int foreground, int background) {
  int fg_code;
  int bg_code;

  if (!term || !term->ansi_enabled || !term->output) {
    return;
  }

  fg_code = color_to_ansi(foreground, 0);
  bg_code = color_to_ansi(background, 1);
  fprintf(term->output, "\x1b[%d;%dm", fg_code, bg_code);
}

void basic_terminal_write(BasicTerminal *term, const char *text) {
  if (!term || !term->output || !text) {
    return;
  }
  fputs(text, term->output);
}

void basic_terminal_write_n(BasicTerminal *term, const char *text, size_t len) {
  if (!term || !term->output || !text || len == 0) {
    return;
  }
  (void)fwrite(text, 1, len, term->output);
}

void basic_terminal_printf(BasicTerminal *term, const char *fmt, ...) {
  va_list args;

  if (!term || !term->output || !fmt) {
    return;
  }

  va_start(args, fmt);
  vfprintf(term->output, fmt, args);
  va_end(args);
}

void basic_terminal_flush(BasicTerminal *term) {
  if (!term || !term->output) {
    return;
  }
  fflush(term->output);
}

char *basic_terminal_read_line(BasicTerminal *term, char *buffer, size_t size) {
  if (!term || !term->input || !buffer || size == 0) {
    return NULL;
  }
  return fgets(buffer, (int)size, term->input);
}

int basic_terminal_read_key_nonblocking(BasicTerminal *term, char *out) {
  if (!term || !out) {
    return -1;
  }

#if defined(_WIN32)
  if (!stream_is_tty(term->input)) {
    return -1;
  }
  if (!_kbhit()) {
    return 0;
  }
  *out = (char)_getch();
  return 1;
#else
  int fd;
  struct termios original;
  struct termios raw;
  ssize_t count;

  if (!stream_is_tty(term->input)) {
    return -1;
  }

  fd = fileno(term->input);
  if (tcgetattr(fd, &original) != 0) {
    return -1;
  }
  raw = original;
  raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  if (tcsetattr(fd, TCSANOW, &raw) != 0) {
    return -1;
  }

  count = read(fd, out, 1);
  (void)tcsetattr(fd, TCSANOW, &original);
  if (count == 1) {
    return 1;
  }
  return 0;
#endif
}

int basic_terminal_read_key_blocking(BasicTerminal *term, char *out) {
  if (!term || !out) {
    return -1;
  }

#if defined(_WIN32)
  if (!stream_is_tty(term->input)) {
    return -1;
  }
  *out = (char)_getch();
  return 1;
#else
  int fd;
  struct termios original;
  struct termios raw;
  ssize_t count;

  if (!stream_is_tty(term->input)) {
    return -1;
  }

  fd = fileno(term->input);
  if (tcgetattr(fd, &original) != 0) {
    return -1;
  }
  raw = original;
  raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  if (tcsetattr(fd, TCSANOW, &raw) != 0) {
    return -1;
  }

  count = read(fd, out, 1);
  (void)tcsetattr(fd, TCSANOW, &original);
  if (count == 1) {
    return 1;
  }
  return 0;
#endif
}
