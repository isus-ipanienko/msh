/* Includes -------------------------------------------------------------- */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "msh.h"

/* Config  --------------------------------------------------------------- */

#define MAX_PRINTF_LEN     256
#define MAX_COMMAND_LENGTH    32
#define MAX_ARG_NUM           5

#define MSH_PROMPT            "msh> "

/* Defines --------------------------------------------------------------- */

#define KEY_CODE_CR                         0x0D
#define KEY_CODE_LF                         0x0A
#define KEY_CODE_BACKSPACE                  0x08
#define KEY_CODE_DELETE                     0x7F
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

const char *const msh_ret_strings[MSH_RET_TOP] = {
    "MSH_OK",
#define X(_retcode) #_retcode,
    MSH_RETCODES
#undef X
};

enum escape_machine {
    ESCAPE_IDLE,
    ESCAPE_SET,
};

struct autocomplete_ctx {
    char buff[MAX_COMMAND_LENGTH + 1];
    size_t len;
    size_t offset;
    bool is_set;
};

struct cmd_buff  {
    char buff[MAX_COMMAND_LENGTH + 1];
    size_t len;
    size_t pos;
};

struct msh_context {
    msh_write_cb_t write;
    struct cmd_buff cmd;
    struct autocomplete_ctx cmpl;
    enum escape_machine escape_state;
    bool initialized;
    bool logs_enabled;
};

static struct msh_context ctx;

/* Static methods -------------------------------------------------------- */

static void _print_newline(void)
{
    ctx.write("\n\r", 2);
}

static void _print_prompt(void)
{
    ctx.write(MSH_PROMPT, sizeof(MSH_PROMPT) - 1);
}

static void _move_cursor(const char dir)
{
    const char write_buff[3] = {
        KEY_CODE_ESCAPE,
        KEY_CODE_BRACKET,
        dir
    };

    ctx.write(write_buff, sizeof(write_buff));
}

static void _seek_left(void)
{
    if (ctx.cmd.pos == 0)
        return;

    _move_cursor(KEY_CODE_ARROW_LEFT);
    ctx.cmd.pos--;
}

static void _seek_right(void)
{
    if (ctx.cmd.pos == ctx.cmd.len)
        return;
    
    _move_cursor(KEY_CODE_ARROW_RIGHT);
    ctx.cmd.pos++;
}

static void _clear_line(void)
{
    ctx.write("\r", 1);
    _move_cursor(KEY_CODE_CLEAR_LINE_AFTER_CURSOR);
}

static int _msh_vsnprintf(const char *format, va_list *args)
{
    char printf_buffer[MAX_PRINTF_LEN + 1] = {0};
    int len = vsnprintf(printf_buffer, sizeof(printf_buffer), format, *args);

    if (len > 0)
        ctx.write(printf_buffer, (len < MAX_PRINTF_LEN) ? len : MAX_PRINTF_LEN);

    return len;
}

static void _clear_buffers(void)
{
    memset(&ctx.cmd, 0, sizeof(ctx.cmd));
    memset(&ctx.cmpl, 0, sizeof(ctx.cmpl));
}

static void _print_command(void)
{
    _print_prompt();
    ctx.write(ctx.cmd.buff, ctx.cmd.len);
}

static bool _find_command(size_t *const index, const char *const cmd)
{
    for (size_t i = 0; i < msh_cmd_cnt; i++) {
        if (strcmp(cmd, msh_cmds[i].name) == 0) {
            *index = i;
            return true;
        }
    }

    return false;
}

static int _parse(const char *argv[])
{
    int argc = 0;

    if (ctx.cmd.buff[0] == '\0')
        return argc;

    argv[argc++] = ctx.cmd.buff;

    for (size_t i = 1; (i < ctx.cmd.len) && (argc < MAX_ARG_NUM); ++i) {
        if (ctx.cmd.buff[i] == ' ') {
            ctx.cmd.buff[i] = '\0';
            if (ctx.cmd.buff[i + 1] != ' ')
                argv[argc++] = ctx.cmd.buff + i + 1;
        }
    }

    return argc;
}

static void _exec(void)
{
    const char *argv[MAX_ARG_NUM] = {0};

    const int argc = _parse(argv);
    if (argc == 0)
        goto exit;

    size_t idx = msh_cmd_cnt;
    if (!_find_command(&idx, argv[0])) {
        msh_printf("ERROR: Command not found!\n");
        goto exit;
    }

    enum msh_ret ret = msh_cmds[idx].callback(argc, argv);
    if (ret >= MSH_RET_TOP)
        msh_printf("%s returned with exit code %d (code out of range)\n",
                                                msh_cmds[idx].name, ret);
    else if (ret > 0)
        msh_printf("%s returned with exit code %s\n", msh_cmds[idx].name,
                                                      msh_ret_strings[ret]);

exit:
    _clear_buffers();
    _print_newline();
    _print_prompt();
}

static void _autocomplete(void)
{
    if (!ctx.cmpl.is_set) {
        ctx.cmpl.is_set = true;
        ctx.cmpl.offset = 0;
        ctx.cmpl.len = ctx.cmd.len;
        memcpy(ctx.cmpl.buff, ctx.cmd.buff, ctx.cmd.len);
        memset(ctx.cmpl.buff + ctx.cmd.len, 0, sizeof(ctx.cmpl.buff) - ctx.cmd.len);
    }

    size_t idx = msh_cmd_cnt;
    for (size_t i = ctx.cmpl.offset; i < (msh_cmd_cnt + ctx.cmpl.offset); i++) {
        const size_t try_idx = i % msh_cmd_cnt;
        if (strcmp(msh_cmds[try_idx].name, ctx.cmpl.buff) == 0) {
            idx = try_idx;
            break;
        }
    }

    if (idx < msh_cmd_cnt) {
        ctx.cmpl.offset = idx + 1;
        ctx.cmd.len = msh_cmds[idx].len;
        ctx.cmd.pos = msh_cmds[idx].len;
        memcpy(ctx.cmd.buff, msh_cmds[idx].name, msh_cmds[idx].len);
        memset(ctx.cmd.buff + msh_cmds[idx].len, 0, sizeof(ctx.cmd.buff) - msh_cmds[idx].len);
        _clear_line();
        _print_command();
    } else {
        ctx.cmpl.offset = 0;
    }
}

static void _delete(char mode)
{
    if (ctx.cmd.pos == 0)
        return;

    size_t offset = mode == KEY_CODE_BACKSPACE;
    ctx.cmd.buff[ctx.cmd.len - offset] = '\0';
    ctx.write(&mode, 1);

    if (ctx.cmd.pos == ctx.cmd.len)
        ctx.cmd.len--;

    ctx.cmd.pos--;
    ctx.cmpl.is_set = false;
}

static void _special_char(const char input)
{
    if (input == KEY_CODE_BRACKET)
        return;

    if (ctx.escape_state == ESCAPE_SET) {
        if (input == KEY_CODE_ARROW_LEFT)
            _seek_left();
        else if (input == KEY_CODE_ARROW_RIGHT)
            _seek_right();

        ctx.escape_state = ESCAPE_IDLE;
    }

    if (input == KEY_CODE_CLEAR_TERMINAL) {
        _clear_buffers();
        ctx.write(&input, 1);
        _print_prompt();
    }
}

static void _put(const char input)
{
    if ((ctx.cmd.len == MAX_COMMAND_LENGTH) && (ctx.cmd.len == ctx.cmd.pos))
        return;
    
    ctx.cmd.buff[ctx.cmd.pos] = input;
    ctx.write(&input, 1);

    if (ctx.cmd.pos == ctx.cmd.len)
        ctx.cmd.len++;

    ctx.cmd.pos++;
    ctx.cmpl.is_set = false;
}

/* Public methods -------------------------------------------------------- */

bool msh_process(const char input)
{
    if (!ctx.initialized)
        return false;

    if ((ctx.escape_state == ESCAPE_IDLE) &&
            (isalnum(input) || (input == KEY_CODE_SPACE))) {
        _put(input);
        return true;
    }

    switch (input) {
    case KEY_CODE_CR:
    case KEY_CODE_LF:
        _exec();
        break;
    case KEY_CODE_BACKSPACE:
    case KEY_CODE_DELETE:
        _delete(input);
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

int msh_printf(const char *format, ...)
{
    int ret = 0;

    if (!ctx.initialized)
        return ret;

    va_list args;
    va_start(args, format);
    ret = _msh_vsnprintf(format, &args);
    va_end(args);

    return ret;
}

int msh_log(const char *format, ...)
{
    int ret = 0;

    if (!ctx.logs_enabled || !ctx.initialized)
        return ret;

    _clear_line();

    va_list args;
    va_start(args, format);
    ret = _msh_vsnprintf(format, &args);
    va_end(args);

    _print_newline();
    _print_command();

    return ret;
}

void msh_enable_logs(bool enable)
{
    ctx.logs_enabled = enable;
}

bool msh_init(msh_write_cb_t write)
{
    if (write == NULL)
        return false;

    _clear_buffers();
    ctx.write = write;
    ctx.initialized = true;
    _print_newline();
    _print_prompt();

    return true;
}
