#include "test.h"
#include "shell.h"

static char out[2048];
static size_t out_len;
static int last_argc;
static char last_args[SHELL_ARGS_MAX][32];

static void capture(void *ctx, const char *s, size_t n)
{
    if (out_len + n < sizeof out) {
        memcpy(out + out_len, s, n);
        out_len += n;
        out[out_len] = '\0';
    }
}

static void reset(void)
{
    out_len = 0;
    out[0] = '\0';
    last_argc = 0;
}

static int cmd_record(struct shell *sh, int argc, char **argv)
{
    last_argc = argc;
    for (int i = 0; i < argc; i++)
        snprintf(last_args[i], sizeof last_args[i], "%s", argv[i]);
    return SHELL_OK;
}

static int cmd_strict(struct shell *sh, int argc, char **argv)
{
    return argc == 2 ? SHELL_OK : SHELL_USAGE;
}

static const struct shell_cmd cmds[] = {
    { "rec", "[args]", "records its arguments", cmd_record },
    { "strict", "<one>", "takes one argument", cmd_strict },
};

static void type(struct shell *sh, const char *s)
{
    while (*s)
        shell_input(sh, *s++);
}

int main(void)
{
    struct shell sh;
    shell_init(&sh, cmds, 2, capture, NULL);

    /* Splitting: blanks, quotes */
    char line[] = "  user add  \"Ada Lovelace\" 04A1B2C3 ";
    char *argv[SHELL_ARGS_MAX];
    CHECK_INT(shell_split(line, argv, SHELL_ARGS_MAX), 4);
    CHECK_STR(argv[2], "Ada Lovelace");
    CHECK_STR(argv[3], "04A1B2C3");
    char bad[] = "a \"unterminated";
    CHECK_INT(shell_split(bad, argv, SHELL_ARGS_MAX), -1);
    char many[] = "1 2 3";
    CHECK_INT(shell_split(many, argv, 2), -1);

    /* A line typed with CR LF runs once, with echo and a prompt after */
    reset();
    type(&sh, "rec a b\r\n");
    CHECK_INT(last_argc, 3);
    CHECK_STR(last_args[1], "a");
    CHECK(strstr(out, "rec a b\r\n") == out);
    CHECK(strstr(out, "> ") != NULL);

    /* Backspace edits the line */
    reset();
    type(&sh, "rex\bc z\r");
    CHECK_INT(last_argc, 2);
    CHECK_STR(last_args[0], "rec");
    CHECK(strstr(out, "\b \b") != NULL);

    /* Ctrl-C drops the line */
    reset();
    type(&sh, "rec dropped\x03");
    CHECK_INT(last_argc, 0);
    CHECK(strstr(out, "^C") != NULL);

    /* Usage on wrong arguments, unknown commands, help */
    reset();
    type(&sh, "strict\r");
    CHECK(strstr(out, "usage: strict <one>") != NULL);
    reset();
    type(&sh, "nope\r");
    CHECK(strstr(out, "unknown command: nope") != NULL);
    reset();
    type(&sh, "help\r");
    CHECK(strstr(out, "records its arguments") != NULL);
    CHECK(strstr(out, "takes one argument") != NULL);

    /* An over-long line is cut, not overflowed */
    reset();
    for (int i = 0; i < 300; i++)
        shell_input(&sh, 'x');
    shell_input(&sh, '\r');
    CHECK(strstr(out, "unknown command") != NULL);

    return test_report("shell");
}
