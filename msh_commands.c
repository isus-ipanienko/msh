/*
 * commands.c
 *
 *  Created on: Aug 21, 2021
 *      Author: pawel
 */

/* Includes -------------------------------------------------------------- */

#include "msh.h"

/* Defines --------------------------------------------------------------- */

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

/* Commands -------------------------------------------------------------- */

int msh_hello(int argc, char *argv[])
{
	msh_printf("Hello world!");
	return 0;
}

int msh_help(int argc, char *argv[])
{
	for (size_t i = 0; i < msh_num_commands; i++)
	{
		msh_printf("%s: %s", msh_commands[i].name, msh_commands[i].help);
	}
	return 0;
}

int msh_man(int argc, char *argv[])
{
	msh_printf("Use arrow keys to edit the current line.");
	msh_printf("Use ctrl + L to clear the widow.");
	msh_printf("Use tab to cycle through autocompleted commands. (WIP)");
	return 0;
}

/* Command list ---------------------------------------------------------- */

static const struct msh_command_t commands[] = {
  {"help", msh_help, "lists all commands"},
  {"man", msh_man, "manual for the terminal"},
  {"hello", msh_hello, "say hello!"},
};

const struct msh_command_t *const msh_commands = commands;
const size_t msh_num_commands = ARRAY_SIZE(commands);
