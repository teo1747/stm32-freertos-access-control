#include <stdarg.h>
#include <string.h>
#include "shell.h"
#include "fmt.h"

void shell_init(struct shell *sh, const struct shell_cmd *cmds, size_t ncmds,
                void (*write)(void *ctx, const char *s, size_t n), void *ctx)
{
    memset(sh, 0, sizeof *sh);
    sh->cmds = cmds;
    sh->ncmds = ncmds;
    sh->write = write;
    sh->ctx = ctx;
    sh->prompt = "> ";
    sh->echo = 1;
}

void shell_print(struct shell *sh, const char *s)
{
    sh->write(sh->ctx, s, strlen(s));
}

void shell_printf(struct shell *sh, const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = fmt_vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof buf - 1)
        n = sizeof buf - 1;
    sh->write(sh->ctx, buf, (size_t)n);
}

void shell_prompt(struct shell *sh)
{
    shell_print(sh, sh->prompt);
}

int shell_split(char *line, char **argv, int max)
{
    int argc = 0;
    char *p = line;

    for (;;) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            return argc;
        if (argc == max)
            return -1;
        if (*p == '"') {
            argv[argc++] = ++p;
            while (*p && *p != '"')
                p++;
            if (!*p)
                return -1;
        } else {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t')
                p++;
            if (!*p)
                return argc;
        }
        *p++ = '\0';
    }
}

static void help(struct shell *sh)
{
    for (size_t i = 0; i < sh->ncmds; i++) {
        const struct shell_cmd *c = &sh->cmds[i];
        char left[40];
        fmt_snprintf(left, sizeof left, "%s %s", c->name, c->usage ? c->usage : "");
        shell_printf(sh, "  %-28s %s\r\n", left, c->help);
    }
    shell_printf(sh, "  %-28s %s\r\n", "help", "this list");
}

int shell_exec(struct shell *sh, char *line)
{
    char *argv[SHELL_ARGS_MAX];
    int argc = shell_split(line, argv, SHELL_ARGS_MAX);

    if (argc < 0) {
        shell_print(sh, "error: unbalanced quote or too many arguments\r\n");
        return SHELL_USAGE;
    }
    if (argc == 0)
        return SHELL_OK;
    if (strcmp(argv[0], "help") == 0 || strcmp(argv[0], "?") == 0) {
        help(sh);
        return SHELL_OK;
    }
    for (size_t i = 0; i < sh->ncmds; i++) {
        const struct shell_cmd *c = &sh->cmds[i];
        if (strcmp(argv[0], c->name) != 0)
            continue;
        int rc = c->run(sh, argc, argv);
        if (rc == SHELL_USAGE)
            shell_printf(sh, "usage: %s %s\r\n", c->name, c->usage ? c->usage : "");
        return rc;
    }
    shell_printf(sh, "unknown command: %s (try help)\r\n", argv[0]);
    return SHELL_USAGE;
}

void shell_input(struct shell *sh, char c)
{
    char last = sh->last;
    sh->last = c;

    switch (c) {
    case '\n':
        if (last == '\r')
            return;     /* the LF of a CR LF */
        /* fall through */
    case '\r':
        if (sh->echo)
            shell_print(sh, "\r\n");
        sh->line[sh->len] = '\0';
        shell_exec(sh, sh->line);
        sh->len = 0;
        shell_prompt(sh);
        return;
    case '\b':
    case 0x7f:
        if (sh->len) {
            sh->len--;
            if (sh->echo)
                shell_print(sh, "\b \b");
        }
        return;
    case 0x03:          /* Ctrl-C: drop the line */
        sh->len = 0;
        shell_print(sh, "^C\r\n");
        shell_prompt(sh);
        return;
    default:
        if (c < ' ' || c > '~')
            return;     /* other control characters and 8-bit noise */
        if (sh->len < SHELL_LINE_MAX - 1) {
            sh->line[sh->len++] = c;
            if (sh->echo)
                sh->write(sh->ctx, &c, 1);
        }
        return;
    }
}
