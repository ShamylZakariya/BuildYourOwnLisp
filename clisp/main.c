#include <editline/readline.h>
#include <stdio.h>
#include <stdlib.h>

#include "libclisp/libclisp.h"

int main(int argc, char** argv)
{
    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    while (1) {
        char* input = readline("lispy >");
        add_history(input);

        printf("Echo: %s\n", input);
        free(input);
    }

    return 0;
}