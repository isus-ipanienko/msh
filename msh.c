/* Includes -------------------------------------------------------------- */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "msh.h"

/* Config  --------------------------------------------------------------- */

#define MAX_PRINTF_LENGTH     256
#define MAX_COMMAND_LENGTH    32
#define MAX_ARG_NUM           5

#define MSH_PROMPT            "msh> "

/* Defines --------------------------------------------------------------- */

#define KEY_CODE_CR                         0x0D
#define KEY_CODE_LF                         0x0A
#define KEY_CODE_BACKSPACE                  0x08
#define KEY_CODE_BACKSPACE_ALT              0x7F
#define KEY_CODE_ARROW_UP                   'A'
#define KEY_CODE_ARROW_DOWN                 'B'
#define KEY_CODE_ARROW_RIGHT                'C'
#define KEY_CODE_ARROW_LEFT                 'D'
#define KEY_CODE_CLEAR_LINE_AFTER_CURSOR    'K'
#define KEY_CODE_CLEAR_TERMINAL             '\f'
#define KEY_CODE_ESCAPE                     '\e'
#define KEY_CODE_BRACKET                    0x5B
#define KEY_CODE_SPACE                      0x20
#define KEY_CODE_TAB                        '\t'

/* Static variables ------------------------------------------------------ */

enum escape_machine
{
    ESCAPE_IDLE,
    ESCAPE_SET,
};

enum autocomplete_machine
{
    AUTOCOMPLETE_TEMPLATE_NOT_SET,
    AUTOCOMPLETE_TEMPLATE_SET,
};

struct msh_context
{
    write_callback_t write;

    char command_buffer[MAX_COMMAND_LENGTH + 1];
    char autocomplete_buffer[MAX_COMMAND_LENGTH + 1];

    size_t current_command_length;
    size_t cursor_pointer;
    size_t autocomplete_candidate_index_offset;

    enum escape_machine escape_state;
    enum autocomplete_machine autocomplete_state;

    bool logs_enabled;
};

static struct msh_context ctx;

/* Static methods -------------------------------------------------------- */

static void _print_newline()
{
    const char buff[2] =
    {
        '\n',
        '\r'
    };

    ctx.write("\n\r",
              sizeof(buff));
}

static void _print_prompt()
{
    ctx.write(MSH_PROMPT,
              sizeof(MSH_PROMPT) - 1);
}

static void _move_cursor(const char dir)
{
    const char dir_buff[3] =
    {
        KEY_CODE_ESCAPE,
        KEY_CODE_BRACKET,
        dir
    };

    ctx.write(dir_buff,
              3);
}

static void _seek_left()
{
    if (ctx.cursor_pointer > 0)
    {
        _move_cursor(KEY_CODE_ARROW_LEFT);
        ctx.cursor_pointer--;
    }
}

static void _seek_right()
{
    if (ctx.cursor_pointer < ctx.current_command_length)
    {
        _move_cursor(KEY_CODE_ARROW_RIGHT);
        ctx.cursor_pointer++;
    }
}

static void _clear_line()
{
    const char ret = '\r';

    ctx.write(&ret,
              1);
    _move_cursor(KEY_CODE_CLEAR_LINE_AFTER_CURSOR);
}

static int _msh_vsnprintf(const char *format,
                          va_list args)
{
    char printf_buffer[MAX_PRINTF_LENGTH + 1] = {0};
    int length = vsnprintf(printf_buffer,
                           sizeof(printf_buffer),
                           format,
                           args);

    length = (length < MAX_PRINTF_LENGTH) ? length : MAX_PRINTF_LENGTH;

    if (length > 0)
    {
        ctx.write(printf_buffer,
                  length);
    }

    return length;
}

static void _clear_buffers()
{
    memset(ctx.command_buffer,
           0,
           sizeof(ctx.command_buffer));

    memset(ctx.autocomplete_buffer,
           0,
           sizeof(ctx.autocomplete_buffer));

    ctx.current_command_length = 0;
    ctx.cursor_pointer = 0;
    ctx.autocomplete_candidate_index_offset = 0;
    ctx.autocomplete_state = AUTOCOMPLETE_TEMPLATE_NOT_SET;
}

static void _print_command()
{
    _print_prompt();

    ctx.write(ctx.command_buffer,
              ctx.current_command_length);
}

static int _parse(const char *argv[])
{
    int argc = 0;

    if (ctx.command_buffer[0] == '\0')
    {
        return argc;
    }

    argv[argc++] = ctx.command_buffer;

    for (size_t i = 1; (i < ctx.current_command_length) && (argc < MAX_ARG_NUM); ++i)
    {
        char *const c = &ctx.command_buffer[i];

        if (*c == ' ')
        {
            *c = '\0';

            if (*(c + 1) != ' ')
            {
                argv[argc++] = c + 1;
            }
        }
    }

    return argc;
}

static bool _find_command(size_t *index,
                          const char *const cmd)
{
    for (size_t cmd_index = 0; cmd_index < msh_commsnds_size; cmd_index++)
    {
        if (cmd[msh_commands[cmd_index].name_len] == '\0')
        {
            if (strncmp(cmd,
                        msh_commands[cmd_index].name,
                        msh_commands[cmd_index].name_len) == 0)
            {
                *index = cmd_index;

                return true;
            }
        }
    }

    return false;
}

static void _execute_command()
{
    const char *argv[MAX_ARG_NUM] = {0};
    const int argc = _parse(argv);

    if (argc == 0)
    {
        goto CLEANUP;
    }

    size_t command_index = msh_commsnds_size;

    if (!_find_command(&command_index,
                       argv[0]))
    {
        msh_printf("ERROR: Command not found!");
        goto CLEANUP;
    }

    int retval = msh_commands[command_index].callback(argc,
                                                      argv);

    if (retval < 0)
    {
        msh_printf("%s returned with exit code %i.",
                   msh_commands[command_index].name,
                   retval);
    }

CLEANUP:
    _clear_buffers();
    _print_newline();
    _print_prompt();
}

static void _autocomplete()
{
    if (ctx.autocomplete_state == AUTOCOMPLETE_TEMPLATE_NOT_SET)
    {
        ctx.autocomplete_state = AUTOCOMPLETE_TEMPLATE_SET;
        memcpy(ctx.autocomplete_buffer,
               ctx.command_buffer,
               ctx.current_command_length);
        memset(&ctx.autocomplete_buffer[ctx.current_command_length],
               0,
               sizeof(ctx.autocomplete_buffer) - ctx.current_command_length);
    }

    size_t matched_command_index = msh_commsnds_size;
    size_t autocomplete_length = strlen(ctx.autocomplete_buffer);

    for (unsigned char attempts = 0; attempts < 2; attempts++)
    {
        for (size_t candidate = 0; candidate < msh_commsnds_size; candidate++)
        {
            if (msh_commands[candidate].name_len <= autocomplete_length)
            {
                continue;
            }

            if (candidate >= ctx.autocomplete_candidate_index_offset)
            {
                if (strncmp(ctx.autocomplete_buffer,
                            msh_commands[candidate].name,
                            autocomplete_length) == 0)
                {
                    matched_command_index = candidate;
                    ctx.autocomplete_candidate_index_offset = (candidate + 1) % msh_commsnds_size;
                    memcpy(&ctx.command_buffer[autocomplete_length],
                           &msh_commands[candidate].name[autocomplete_length],
                           msh_commands[candidate].name_len - autocomplete_length);
                    memset(&ctx.command_buffer[msh_commands[candidate].name_len],
                           0,
                           sizeof(ctx.command_buffer) - msh_commands[candidate].name_len);
                    ctx.current_command_length = msh_commands[candidate].name_len;
                    ctx.cursor_pointer = msh_commands[candidate].name_len;
                    break;
                }
            }
        }

        if (matched_command_index < msh_commsnds_size)
        {
            _clear_line();
            _print_command();
            break;
        }
        else
        {
            ctx.autocomplete_candidate_index_offset = 0;
        }
    }
}

static void _delete_key(char backspace_mode)
{
    if (ctx.cursor_pointer == 0)
    {
        return;
    }

    size_t offset = backspace_mode != KEY_CODE_BACKSPACE_ALT;

    ctx.command_buffer[ctx.current_command_length - offset] = '\0';

    ctx.write(&backspace_mode,
              1);

    if (ctx.cursor_pointer == ctx.current_command_length)
    {
        ctx.current_command_length--;
    }

    ctx.cursor_pointer--;

    ctx.autocomplete_state = AUTOCOMPLETE_TEMPLATE_NOT_SET;
}

static void _special_char(const char input)
{
    if (input == KEY_CODE_BRACKET)
    {
        return;
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

    if (input == KEY_CODE_CLEAR_TERMINAL)
    {
        _clear_buffers();
        ctx.write(&input,
                  1);
        _print_prompt();
    }
}

static void _add_alphanumeric(const char input)
{
    if ((ctx.current_command_length == MAX_COMMAND_LENGTH) && (ctx.current_command_length == ctx.cursor_pointer))
    {
        return;
    }

    ctx.command_buffer[ctx.cursor_pointer] = input;

    ctx.write(&input,
              1);

    if (ctx.cursor_pointer == ctx.current_command_length)
    {
        ctx.current_command_length++;
    }

    ctx.cursor_pointer++;

    ctx.autocomplete_state = AUTOCOMPLETE_TEMPLATE_NOT_SET;
}

/* Public methods -------------------------------------------------------- */

bool msh_process(const char input)
{
    if ((isalnum(input) || (input == KEY_CODE_SPACE)) && (ctx.escape_state == ESCAPE_IDLE))
    {
        _add_alphanumeric(input);

        return true;
    }

    switch (input)
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
        _special_char(input);
        break;
    }

    return true;
}

int msh_printf(const char *format,
               ...)
{
    _print_newline();

    int ret = 0;
    va_list args;

    va_start(args,
             format);

    ret = _msh_vsnprintf(format,
                         args);

    va_end(args);

    return ret;
}

int msh_log(const char *format,
            ...)
{
    int ret = 0;

    if (!ctx.logs_enabled)
    {
        return ret;
    }

    _clear_line();

    va_list args;

    va_start(args,
             format);

    ret = _msh_vsnprintf(format,
                         args);

    va_end(args);

    _print_newline();
    _print_command();

    return ret;
}

void msh_enable_logs(bool enable)
{
    ctx.logs_enabled = enable;
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
