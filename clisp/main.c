#include <editline/readline.h>
#include <stdio.h>
#include <stdlib.h>

#include "libclisp/libclisp.h"
#include "libmpc/mpc.h"

int main(int argc, char** argv)
{
    lgrammar* grammar = lgrammar_new();
    lenv* env = lenv_new();
    lenv_add_default_builtins(env, grammar);

    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");

    while (1) {
        char* input = readline("lispy >");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, grammar->lispy, &r)) {
            lval* x = lval_read(r.output);
            x = lval_eval(env, x);
            lval_println(x);

            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    lenv_del(env);
    lgrammar_del(grammar);
    return 0;
}