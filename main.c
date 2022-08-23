#include <unistd.h>
#include "msh.h"

void example_print(const char *output, const size_t len)
{
    write(STDOUT_FILENO, output, len);
}

int main(void)
{
    if (!msh_init(&example_print))
        return -1;

    char c;
    while (read(STDIN_FILENO, &c, 1) && msh_process(c));
}
