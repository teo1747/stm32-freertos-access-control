/*
 * The admin shell's line discipline and command dispatch. It knows nothing
 * about the UART: characters come in through shell_input and output leaves
 * through the write callback, so the same code runs in the unit tests.
 */
#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>

#define SHELL_LINE_MAX  96
#define SHELL_ARGS_MAX  8

struct shell;

struct shell_cmd {
    const char *name;
    const char *usage;      /* the arguments, for help */
    const char *help;
    /* argv[0] is the command; returns 0 or an error the shell reports */
    int (*run)(struct shell *sh, int argc, char **argv);
};

struct shell {
    const struct shell_cmd *cmds;
    size_t ncmds;
    void (*write)(void *ctx, const char *s, size_t n);
    void *ctx;
    const char *prompt;
    int echo;
    char line[SHELL_LINE_MAX];
    size_t len;
    char last;              /* the previous character, to swallow CR LF's LF */
};

enum {
    SHELL_OK = 0,
    SHELL_USAGE = -1,       /* wrong arguments: the shell prints the usage */
    SHELL_FAILED = -2,      /* the command printed its own error */
};

void shell_init(struct shell *sh, const struct shell_cmd *cmds, size_t ncmds,
                void (*write)(void *ctx, const char *s, size_t n), void *ctx);
void shell_prompt(struct shell *sh);
/* One received character; a complete line runs its command */
void shell_input(struct shell *sh, char c);
/* Splits and runs one line (modified in place) */
int shell_exec(struct shell *sh, char *line);

void shell_print(struct shell *sh, const char *s);
void shell_printf(struct shell *sh, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/* Splits on blanks, keeping "quoted strings" together; returns argc, or -1
 * for an unterminated quote or too many arguments */
int shell_split(char *line, char **argv, int max);

#endif
