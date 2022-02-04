#ifndef INC_MSH_H_
#define INC_MSH_H_

#include <stddef.h>
#include <stdbool.h>

typedef void (*write_callback_t)(const char *output,
                                 const size_t len);

typedef struct
{
    int (*callback)(int argc,
                    char *argv[]);
    const char *const name;
    const char *const help;
} msh_command_t;

extern const msh_command_t *const msh_commands;
extern const size_t msh_num_commands;

/* Public methods -------------------------------------------------------- */

bool msh_process(char input);

bool msh_printf(const char *format,
                ...);

bool msh_log(const char *format,
             ...);

bool msh_enable_logs(bool enable);

bool msh_init(write_callback_t write);

#endif /* INC_MSH_H_ */
