#ifndef INC_MSH_H_
#define INC_MSH_H_

#include <stddef.h>
#include <stdbool.h>

typedef void (*msh_write_cb_t)(const char *output, const size_t len);

#define MSH_RETCODES    \
    X(MSH_INVALID_ARGC) \
    X(MSH_INVALID_ARGS) \

enum msh_ret {
    MSH_OK = 0,
#define X(_retcode) _retcode,
    MSH_RETCODES
#undef X
    MSH_RET_TOP
};

struct msh_cmd
{
    enum msh_ret (*callback)(const int argc, const char *const argv[]);
    const char *const man;
    const char *const name;
    const size_t len;
};

extern const struct msh_cmd *const msh_cmds;
extern const size_t msh_cmd_cnt;

/* Public methods -------------------------------------------------------- */

bool msh_process(const char input);

int msh_printf(const char *format, ...);

int msh_log(const char *format, ...);

void msh_enable_logs(bool enable);

bool msh_init(msh_write_cb_t write);

#endif /* INC_MSH_H_ */
