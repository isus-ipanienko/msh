/* Includes -------------------------------------------------------------- */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "msh.h"

/* Defines --------------------------------------------------------------- */

#define MAX_COMMAND_LENGTH 128
#define MAX_PRINTF_LENGTH 128
#define MAX_ARG_NUM 5

#define KEY_CODE_CR 					0x0D
#define KEY_CODE_LF 					0x0A
#define KEY_CODE_BACKSPACE 				0x08
#define KEY_CODE_BACKSPACE_ALT			0x7F
#define KEY_CODE_ARROW_LEFT				'C'
#define KEY_CODE_ARROW_RIGHT			'D'
#define KEY_CODE_CLEAR					'\f'
#define KEY_CODE_ESCAPE					'\e'
#define KEY_CODE_BRACKET				0x91
#define KEY_CODE_SPACE					0x20
#define KEY_CODE_TAB					3

#define MSH_PROMPT						'>'

/* Static variables ------------------------------------------------------ */

enum escape_machine
{
	ESCAPE_IDLE,
	ESCAPE_SET
};

struct msh_context
{
	write_callback_t write;

	char printf_buffer[MAX_PRINTF_LENGTH];
	char command_buffer[MAX_COMMAND_LENGTH];

	size_t current_command_length;
	size_t cursor_pointer;

	char *argv[MAX_ARG_NUM];
	int argc;

	enum escape_machine escape_state;
};

static struct msh_context ctx;

/* Static methods -------------------------------------------------------- */

static inline void _print_char(const char c)
{
	ctx.write(&c);
}

static void _print_prompt()
{
	_print_char('\n');
	_print_char('\r');
	_print_char(MSH_PROMPT);
}

static void _print_left()
{
	_print_char(KEY_CODE_ESCAPE);
	_print_char(KEY_CODE_BRACKET);
	_print_char(KEY_CODE_ARROW_LEFT);
}

static void _print_right()
{
	_print_char(KEY_CODE_ESCAPE);
	_print_char(KEY_CODE_BRACKET);
	_print_char(KEY_CODE_ARROW_RIGHT);
}

static bool _seek_left()
{
	if (ctx.cursor_pointer == 0)
	{
		return false;
	}

	_print_left();
	ctx.cursor_pointer--;

	return true;
}

static bool _seek_right()
{
	if (ctx.cursor_pointer == ctx.current_command_length)
	{
		return false;
	}

	_print_right();
	ctx.cursor_pointer++;

	return true;
}

static bool _clear_buffers()
{
	memset(ctx.command_buffer, 0, sizeof(ctx.command_buffer));
	ctx.current_command_length = 0;
	ctx.cursor_pointer = 0;

	memset(ctx.argv, 0, sizeof(ctx.argv));
	ctx.argc = 0;

	return true;
}

static bool _parse()
{
	ctx.argv[0] = ctx.command_buffer;
	ctx.argc = 0;

	for (size_t i = 0; i < ctx.current_command_length && ctx.argc < MAX_ARG_NUM; ++i)
	{
		char *const c = &ctx.command_buffer[i];
		if (*c == ' ')
		{
			*c = '\0';
			if (*(c + 1) != ' ')
			{
				ctx.argv[ctx.argc++] = c + 1;
			}
		}
	}

	return true;
}

static bool _find_command(size_t *index)
{
	if (ctx.argc == 0)
	{
		return false;
	}

	if (ctx.argc > MAX_ARG_NUM)
	{
		msh_printf("ERROR: Too many arguments.");
	}

	for (size_t i = 0; i < msh_num_commands; i++)
	{
		if (strncmp(ctx.argv[0], msh_commands[i].name, strlen(msh_commands[i].name)) == 0)
		{
			*index = i;
			break;
		}
	}

	return true;
}

static bool _execute_command()
{
	bool ret = true;

	if (!_parse())
	{
		msh_printf("ERROR: Failed to parse arguments.");
		ret = false;
		goto EXECUTE_COMMAND_EXIT;
	}

	size_t command_index = msh_num_commands;
	if (!_find_command(&command_index))
	{
		msh_printf("ERROR: Failed to match a command name.");
		ret = false;
		goto EXECUTE_COMMAND_EXIT;
	}

	if (command_index >= msh_num_commands)
	{
		msh_printf("ERROR: Command index out of range.");
		ret = false;
		goto EXECUTE_COMMAND_EXIT;
	}

	int retval = msh_commands[command_index].callback(ctx.argc, ctx.argv);

	if (retval < 0)
	{
		msh_printf("s% returned with exit code %d.", msh_commands[command_index].name, retval);
		ret = false;
	}

	EXECUTE_COMMAND_EXIT:
	_clear_buffers();
	_print_prompt();
	return ret;
}

static bool _delete_key(char backspace_mode)
{
	if (ctx.cursor_pointer == 0)
	{
		return false;
	}

	size_t offset = backspace_mode != KEY_CODE_BACKSPACE_ALT;
	ctx.command_buffer[ctx.current_command_length - offset] = '\0';

	_print_char(backspace_mode);

	if (ctx.cursor_pointer == ctx.current_command_length)
	{
		ctx.current_command_length--;
	}
	ctx.cursor_pointer--;

	return true;
}

static bool _autocomplete()
{
	//TODO: autocomplete
	return true;
}

static bool _special_char(char input)
{
	if (input == KEY_CODE_BRACKET)
	{
		return false;
	}

	if (ctx.escape_state == ESCAPE_SET)
	{
		if (input == KEY_CODE_ARROW_LEFT)
		{
			_seek_left();
		}
		else if (input == KEY_CODE_ARROW_RIGHT)
		{
			_seek_right();
		}

		ctx.escape_state = ESCAPE_IDLE;
	}

	if (input == KEY_CODE_CLEAR)
	{
		_clear_buffers();
		_print_char(input);
	}

	return true;
}

static bool _add_alphanumeric(char input)
{
	if (ctx.current_command_length == MAX_COMMAND_LENGTH)
	{
		return false;
	}

	ctx.command_buffer[ctx.cursor_pointer] = input;

	_print_char(input);

	if (ctx.cursor_pointer == ctx.current_command_length)
	{
		ctx.current_command_length++;
	}
	ctx.cursor_pointer++;

	return true;
}

/* Public methods -------------------------------------------------------- */


bool msh_process(char input)
{
	switch(input)
	{
	case KEY_CODE_CR:
	case KEY_CODE_LF:
		_execute_command();
		break;
	case KEY_CODE_BACKSPACE:
		_delete_key(KEY_CODE_BACKSPACE);
		break;
	case KEY_CODE_BACKSPACE_ALT:
		_delete_key(KEY_CODE_BACKSPACE_ALT);
		break;
	case KEY_CODE_ESCAPE:
		ctx.escape_state = ESCAPE_SET;
		break;
	case KEY_CODE_TAB:
		_autocomplete();
		break;
	default:
		if ((isalnum(input) || input == KEY_CODE_SPACE) && ctx.escape_state == ESCAPE_IDLE)
		{
			_add_alphanumeric(input);
		}
		else
		{
			_special_char(input);
		}
		break;
	}

    return true;
}

bool msh_printf(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	int length = vsnprintf(ctx.printf_buffer, 2, format, args);
	length = (length < MAX_PRINTF_LENGTH) ? length : MAX_PRINTF_LENGTH;

	_print_char('\n');
	_print_char('\r');
	for (int i = 0; i < length; i++)
	{
		ctx.write(&(ctx.printf_buffer[i]));
	}

	va_end(args);
	return true;
}

bool msh_init(write_callback_t write)
{
	if (write == NULL)
	{
		return false;
	}

	_clear_buffers();

	ctx.write = write;
	_print_prompt();

	return true;
}
