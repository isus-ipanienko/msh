/*
 * commands.c
 *
 *  Created on: Aug 21, 2021
 *      Author: pawel
 */

/* Includes -------------------------------------------------------------- */

#include <string.h>

#include "msh.h"

/* Helper Macros --------------------------------------------------------- */

#define ARRAY_SIZE(_arr)    (sizeof(_arr) / sizeof(_arr[0]))

/**
 * @brief   if (ARGV_MATCH(n, "pattern")) matches "pattern" to the nth argument
 * @note    n = 0 is the command name
 */
#define ARGV_MATCH(_argn, _pattern) \
    (strcmp(argv[_argn],            \
            _pattern) == 0)

#define ASSERT_ARGC(_n) \
    do                  \
    {                   \
        if (argc != _n) \
        {               \
            return -1;  \
        }               \
    } while (0)

#define MSH_CMD_DEF(_name)              \
    int msh_cmd_##_name(const int argc, \
                        const char *const argv[])

/* Commands -------------------------------------------------------------- */

MSH_CMD_DEF(hello)
{
    msh_printf("Hello world!");

    return 0;
}

MSH_CMD_DEF(help)
{
    for (size_t cmd = 0; cmd < msh_commands_size; cmd++)
    {
        msh_printf("%s: %s",
                   msh_commands[cmd].name,
                   msh_commands[cmd].man);
    }

    return 0;
}

MSH_CMD_DEF(man)
{
    msh_printf("Use arrow keys to edit the current line.");
    msh_printf("Use ctrl + L to clear the widow.");
    msh_printf("Use tab to cycle through autocompleted commands.");

    return 0;
}

MSH_CMD_DEF(log)
{
    ASSERT_ARGC(2);

    if (ARGV_MATCH(1,
                   "on"))
    {
        msh_enable_logs(true);
    }
    else if (ARGV_MATCH(1,
                        "off"))
    {
        msh_enable_logs(false);
    }
    else
    {
        return -2;
    }

    return 0;
}

/* Command list ---------------------------------------------------------- */

#define MSH_COMMANDS                                        \
    MSH_COMMAND(help,                                       \
                "lists all commands")                       \
    MSH_COMMAND(man,                                        \
                "manual for the terminal")                  \
    MSH_COMMAND(log,                                        \
                "on - turns logs on; off - turns logs off") \
    MSH_COMMAND(hello,                                      \
                "say hello!")

static const msh_command_t commands[] =
{
#define MSH_COMMAND(_name, _man)        \
    {                                   \
        .callback = msh_cmd_##_name,    \
        .name_len = sizeof(#_name) - 1, \
        .name = #_name,                 \
        .man = _man                     \
    },
    MSH_COMMANDS
#undef MSH_COMMAND
};

const msh_command_t *const msh_commands = commands;
const size_t msh_commands_size = ARRAY_SIZE(commands);
