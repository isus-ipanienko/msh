/* Includes -------------------------------------------------------------- */

#include <string.h>

#include "msh.h"

/* Helper Macros --------------------------------------------------------- */

#define ARRAY_SIZE(_arr) (sizeof(_arr) / sizeof(_arr[0]))

/* Commands -------------------------------------------------------------- */

enum msh_ret msh_cmd_hello(const int argc, const char *const argv[])
{
    msh_printf("Hello world!\n");
    return MSH_OK;
}

enum msh_ret msh_cmd_help(const int argc, const char *const argv[])
{
    enum msh_ret ret = MSH_OK;

    switch (argc) {
    case 1:
        msh_printf("Use arrow keys to edit the current line.\n");
        msh_printf("Use ctrl + L to clear the widow.\n");
        msh_printf("Use tab to autocomplete commands.\n");
        msh_printf("Type 'help name' to print instructions for a command.\n");
        msh_printf("Commands:\n");
        for (size_t cmd = 0; cmd < msh_cmd_cnt; cmd++)
            msh_printf("%s, ", msh_cmds[cmd].name);
        msh_printf("\n");
        break;
    case 2:
        ret = MSH_INVALID_ARGS;
        for (size_t idx = 0; idx < msh_cmd_cnt; idx++) {
            if (strcmp(argv[1], msh_cmds[idx].name) == 0) {
                msh_printf("%s: %s\n", msh_cmds[idx].name, msh_cmds[idx].man);
                ret = MSH_OK;
                break;
            }
        }
        break;
    default:
        ret = MSH_INVALID_ARGC;
        break;
    }

    return ret;
}

enum msh_ret msh_cmd_log(const int argc, const char *const argv[])
{
    if (argc != 2)
        return MSH_INVALID_ARGC;

    if (strcmp(argv[1], "on") == 0)
        msh_enable_logs(true);
    else if (strcmp(argv[1], "off") == 0)
        msh_enable_logs(false);
    else
        return MSH_INVALID_ARGS;

    return MSH_OK;
}

/* Command list ---------------------------------------------------------- */

#define MSH_COMMANDS                                               \
    MSH_COMMAND(help, msh_cmd_help,                                \
                "prints manuals and instructions for the terminal")\
    MSH_COMMAND(log, msh_cmd_log,                                  \
                "on - turns logs on; off - turns logs off")        \
    MSH_COMMAND(hello, msh_cmd_hello,                              \
                "say hello!")

static const struct msh_cmd commands[] =
{
#define MSH_COMMAND(_name, _func, _man) \
    {                                   \
        .callback = _func,              \
        .len = sizeof(#_name) - 1,      \
        .name = #_name,                 \
        .man = _man                     \
    },
    MSH_COMMANDS
#undef MSH_COMMAND
};

const struct msh_cmd *const msh_cmds = commands;
const size_t msh_cmd_cnt = ARRAY_SIZE(commands);
