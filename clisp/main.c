#include <editline/readline.h>
#include <stdio.h>
#include <stdlib.h>

#include "libclisp/libclisp.h"
#include "libmpc/mpc.h"

int main(int argc, char** argv)
{
    lgrammar* grammar = lgrammar_new();
    lenv* env = lenv_new(grammar->lispy);

    // load builtins
    lenv_add_default_builtins(env, grammar);

    // load the stdlib
    lval* result = lval_load(env, "lib/std.lspy");
    if (result->type == LVAL_ERR) {
        lval_println(result);
    }
    lval_del(result);

    if (argc == 1) {
        puts("Lispy Version 0.0.0.0.1");
        puts("Press Ctrl+c to Exit\n");

        while (1) {
            char* input = readline("lispy> ");
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
    } else {
        for (int i = 1; i < argc; i++) {
            lval* result = lval_load(env, argv[i]);
            if (result->type == LVAL_ERR) {
                lval_println(result);
            }
            lval_del(result);
        }
    }

    lenv_del(env);
    lgrammar_del(grammar);
    return 0;
}