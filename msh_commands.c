/*
* commands.c
*
*  Created on: Aug 21, 2021
*      Author: pawel
*/

/* Includes -------------------------------------------------------------- */

#include <string.h>

#include "msh.h"

/* Helper Functions / Macros --------------------------------------------- */

#define ARRAY_SIZE(_arr) (sizeof(_arr) / sizeof(_arr[0]))

/**
 * @brief   if (ARG_MATCH(argv[n], "pattern")) matches "pattern" to the nth argument
 * @note    argv[0] is the command name
 */
#define ARG_MATCH(_arg, _pattern) \
    ((strncmp(_arg, _pattern, sizeof(_pattern) - 1) == 0) \
     && (_arg[sizeof(_pattern) - 1] == '\0'))

/* Commands -------------------------------------------------------------- */

#define MSH_CMD_DEFINITION(_name) int msh_cmd_##_name(const int argc, const char *const argv[])

MSH_CMD_DEFINITION(hello)
{
    msh_printf("Hello world!");
    return 0;
}

MSH_CMD_DEFINITION(help)
{
    for (size_t i = 0; i < msh_commsnds_size; i++)
    {
        msh_printf("%s: %s", msh_commands[i].name, msh_commands[i].man);
    }
    return 0;
}

MSH_CMD_DEFINITION(man)
{
    msh_printf("Use arrow keys to edit the current line.");
    msh_printf("Use ctrl + L to clear the widow.");
    msh_printf("Use tab to cycle through autocompleted commands.");
    return 0;
}

MSH_CMD_DEFINITION(log)
{
    if (argc != 2)
    {
        return -1;
    }

    if (ARG_MATCH(argv[1], "on"))
    {
        msh_enable_logs(true);
    }
    else if (ARG_MATCH(argv[1], "off"))
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

#define MSH_COMMANDS \
    MSH_COMMAND(help, "lists all commands") \
    MSH_COMMAND(man, "manual for the terminal") \
    MSH_COMMAND(log, "on - turns logs on; off - turns logs off") \
    MSH_COMMAND(hello, "say hello!")

static const msh_command_t commands[] = 
{
#define MSH_COMMAND(_name, _man)    \
    {                               \
    .callback = msh_cmd_##_name,    \
    .name_len = sizeof(#_name) - 1, \
    .name = #_name,                 \
    .man = _man                     \
    },
    MSH_COMMANDS
#undef MSH_COMMAND
};

const msh_command_t *const msh_commands = commands;
const size_t msh_commsnds_size = ARRAY_SIZE(commands);
