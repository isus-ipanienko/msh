#ifndef INC_MSH_H_
#define INC_MSH_H_

#include <stddef.h>
#include <stdbool.h>

typedef void (*write_callback_t)(const char *output,
                                 const size_t len);

typedef struct
{
    int (*callback)(const int argc,
                    const char *const argv[]);
    const size_t name_len;
    const char *const name;
    const char *const man;
} msh_command_t;

extern const msh_command_t *const msh_commands;
extern const size_t msh_commands_size;

/* Public methods -------------------------------------------------------- */

bool msh_process(const char input);

int msh_printf(const char *format,
               ...);

int msh_log(const char *format,
            ...);

void msh_enable_logs(bool enable);

bool msh_init(write_callback_t write);

#endif /* INC_MSH_H_ */
