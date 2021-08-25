/* Includes -------------------------------------------------------------- */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "msh.h"

/* Defines --------------------------------------------------------------- */

#define MAX_COMMAND_LENGTH                      32
#define MAX_PRINTF_LENGTH                       64
#define MAX_ARG_NUM                             5

#define KEY_CODE_CR                             0x0D
#define KEY_CODE_LF                             0x0A
#define KEY_CODE_BACKSPACE                      0x08
#define KEY_CODE_BACKSPACE_ALT                  0x7F
#define KEY_CODE_ARROW_UP                       'A'
#define KEY_CODE_ARROW_DOWN                     'B'
#define KEY_CODE_ARROW_RIGHT                    'C'
#define KEY_CODE_ARROW_LEFT                     'D'
#define KEY_CODE_CLEAR_LINE_AFTER_CURSOR        'K'
#define KEY_CODE_CLEAR                          '\f'
#define KEY_CODE_ESCAPE                         '\e'
#define KEY_CODE_BRACKET                        0x5B
#define KEY_CODE_SPACE                          0x20
#define KEY_CODE_TAB                            '\t'

#define MSH_PROMPT                              "msh> "

/* Static variables ------------------------------------------------------ */

enum escape_machine
{
    ESCAPE_IDLE,
    ESCAPE_SET
};

enum autocomplete_machine
{
    AUTOCOMPLETE_TEMPLATE_NOT_SET,
    AUTOCOMPLETE_TEMPLATE_SET,
};

struct msh_context
{
    write_callback_t write;

    char printf_buffer[MAX_PRINTF_LENGTH + 1];
    char command_buffer[MAX_COMMAND_LENGTH + 1];
    char autocomplete_buffer[MAX_COMMAND_LENGTH + 1];

    size_t current_command_length;
    size_t cursor_pointer;
    size_t autocomplete_index_offset;

    char *argv[MAX_ARG_NUM];
    int argc;

    enum escape_machine escape_state;
    enum autocomplete_machine autocomplete_state;

    bool logs_enabled;
};

static struct msh_context ctx;

/* Static methods -------------------------------------------------------- */

static void _print_char(const char c)
{
    ctx.write(&c);
}

static void _print_newline()
{
    _print_char('\n');
    _print_char('\r');
}

static void _print_prompt()
{
    for (size_t i = 0; i < strlen(MSH_PROMPT); i++)
    {
        _print_char(MSH_PROMPT[i]);
    }
}

static void _move_cursor(char dir)
{
    _print_char(KEY_CODE_ESCAPE);
    _print_char(KEY_CODE_BRACKET);
    _print_char(dir);
}

static bool _seek_left()
{
    if (ctx.cursor_pointer == 0)
    {
        return false;
    }

    _move_cursor(KEY_CODE_ARROW_LEFT);
    ctx.cursor_pointer--;

    return true;
}

static bool _seek_right()
{
    if (ctx.cursor_pointer == ctx.current_command_length)
    {
        return false;
    }

    _move_cursor(KEY_CODE_ARROW_RIGHT);
    ctx.cursor_pointer++;

    return true;
}

static bool _msh_vsnprintf(const char* format, va_list args)
{
    int length = vsnprintf(ctx.printf_buffer, sizeof(ctx.printf_buffer), format, args);
    length = (length < MAX_PRINTF_LENGTH) ? length : MAX_PRINTF_LENGTH;

    if (length < 0)
    {
        return false;
    }

    for (size_t i = 0; i < length; i++)
    {
        _print_char(ctx.printf_buffer[i]);
    }

    return true;
}

static bool _clear_buffers()
{
    memset(ctx.command_buffer, 0, sizeof(ctx.command_buffer));
    ctx.current_command_length = 0;
    ctx.cursor_pointer = 0;

    memset(ctx.argv, 0, sizeof(ctx.argv));
    ctx.argc = 0;

    memset(ctx.autocomplete_buffer, 0, sizeof(ctx.autocomplete_buffer));
    ctx.autocomplete_index_offset = 0;
    ctx.autocomplete_state = AUTOCOMPLETE_TEMPLATE_NOT_SET;

    return true;
}

static void _print_command()
{
    _print_prompt();
    for (size_t i = 0; i < ctx.current_command_length; i++)
    {
        _print_char(ctx.command_buffer[i]);
    }
}

static bool _parse()
{
    ctx.argc = 0;

    if (ctx.command_buffer[0] != '\0')
    {
        ctx.argv[0] = ctx.command_buffer;
        ctx.argc = 1;
    }
    else
    {
        return true;
    }

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

    size_t command_name_length = 0;
    for (size_t i = 0; i < msh_num_commands; i++)
    {
        command_name_length = strlen(msh_commands[i].name);
        if (strncmp(ctx.argv[0], msh_commands[i].name, command_name_length) == 0)
        {
            if (strnlen(ctx.argv[0], command_name_length + 1) == command_name_length)
            {
                *index = i;
                return true;
            }
        }
    }

    return false;
}

static bool _execute_command()
{
    bool ret = true;

    if (!_parse())
    {
        msh_printf("ERROR: Failed to parse arguments.");
        ret = false;
        goto CLEANUP;
    }

    if (ctx.argc == 0)
    {
        ret = false;
        goto CLEANUP;
    }

    size_t command_index = msh_num_commands;
    if (!_find_command(&command_index))
    {
        msh_printf("ERROR: Failed to match a command name.");
        ret = false;
        goto CLEANUP;
    }

    if (command_index >= msh_num_commands)
    {
        msh_printf("ERROR: Command index out of range.");
        ret = false;
        goto CLEANUP;
    }

    int retval = msh_commands[command_index].callback(ctx.argc, ctx.argv);

    if (retval < 0)
    {
        msh_printf("%s returned with exit code %i.", msh_commands[command_index].name, retval);
        ret = false;
    }

    CLEANUP:
    _clear_buffers();
    _print_newline();
    _print_prompt();
    return ret;
}

static bool _autocomplete()
{
    if (ctx.autocomplete_state == AUTOCOMPLETE_TEMPLATE_NOT_SET)
    {
        ctx.autocomplete_state = AUTOCOMPLETE_TEMPLATE_SET;
        memcpy(ctx.autocomplete_buffer, ctx.command_buffer, ctx.current_command_length);
        memset(&ctx.autocomplete_buffer[ctx.current_command_length], 0, sizeof(ctx.autocomplete_buffer) - ctx.current_command_length);
    }

    size_t index = msh_num_commands;
    size_t command_name_length = 0;
    size_t autocomplete_length = strlen(ctx.autocomplete_buffer);
    for (uint8_t j = 0; j < 2; j++)
    {
        for (size_t i = 0; i < msh_num_commands; i++)
        {
            command_name_length = strlen(msh_commands[i].name);

            if (command_name_length <= autocomplete_length)
            {
                continue;
            }

            if (strncmp(ctx.autocomplete_buffer, msh_commands[i].name, autocomplete_length) == 0)
            {
                if (i >= ctx.autocomplete_index_offset)
                {
                    index = i;
                    ctx.autocomplete_index_offset = (i + 1) % msh_num_commands;
                    memcpy(&ctx.command_buffer[autocomplete_length], &msh_commands[i].name[autocomplete_length], command_name_length - autocomplete_length);
                    memset(&ctx.command_buffer[command_name_length], 0, sizeof(ctx.command_buffer) - command_name_length);
                    ctx.current_command_length = command_name_length;
                    ctx.cursor_pointer = command_name_length;
                    break;
                }
            }
        }

        if (index != msh_num_commands)
        {
            break;
        }

        ctx.autocomplete_index_offset = 0;
    }

    if (index == msh_num_commands)
    {
        return false;
    }

    _print_char('\r');
    _move_cursor(KEY_CODE_CLEAR_LINE_AFTER_CURSOR);
    _print_command();

    return true;
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

    ctx.autocomplete_state = AUTOCOMPLETE_TEMPLATE_NOT_SET;

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
        _print_prompt();
    }

    return true;
}

static bool _add_alphanumeric(char input)
{
    if ((ctx.current_command_length == MAX_COMMAND_LENGTH) &&
        (ctx.current_command_length == ctx.cursor_pointer))
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

    ctx.autocomplete_state = AUTOCOMPLETE_TEMPLATE_NOT_SET;

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

    _print_newline();
    _msh_vsnprintf(format, args);

    va_end(args);

    return true;
}

bool msh_log(const char* format, ...)
{
    if (!ctx.logs_enabled)
    {
        return false;
    }

    _print_char('\r');
    _move_cursor(KEY_CODE_CLEAR_LINE_AFTER_CURSOR);

    /* print log */
    va_list args;
    va_start(args, format);
    _msh_vsnprintf(format, args);
    va_end(args);

    _print_newline();
    _print_command();

    return true;
}

bool msh_enable_logs(bool enable)
{
    ctx.logs_enabled = enable;
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
    _print_newline();
    _print_prompt();

    return true;
}
