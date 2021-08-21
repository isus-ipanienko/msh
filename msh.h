

#ifndef INC_MSH_H_
#define INC_MSH_H_

#include <stddef.h>
#include <stdbool.h>

typedef void (*write_callback_t)(const char* output);

struct msh_command_t {
	const char *name;
	int (*callback)(int argc, char *argv[]);
	const char *help;
};

extern const struct msh_command_t *const msh_commands;
extern const size_t msh_num_commands;

/* Public methods -------------------------------------------------------- */

bool msh_process(char input);

bool msh_printf(const char* format, ...);

bool msh_init(write_callback_t write);

#endif /* INC_MSH_H_ */
