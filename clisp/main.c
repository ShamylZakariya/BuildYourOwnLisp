#include <editline/readline.h>
#include <stdio.h>
#include <stdlib.h>

#include "libclisp/libclisp.h"
#include "libmpc/mpc.h"

int main(int argc, char** argv)
{
    Grammar grammar = grammar_create();

    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    while (1) {
        char* input = readline("lispy >");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, grammar.Lispy, &r)) {
            lval result = eval(r.output);
            lval_println(result);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    grammar_cleanup(&grammar);
    return 0;
}