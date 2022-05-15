#include <stdio.h>
#include <stdlib.h>

#include "libclisp.h"

static long eval_op(long x, char* op, long y)
{
    if (strcmp(op, "+") == 0) {
        return x + y;
    }
    if (strcmp(op, "-") == 0) {
        return x - y;
    }
    if (strcmp(op, "*") == 0) {
        return x * y;
    }
    if (strcmp(op, "/") == 0) {
        return x / y;
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Grammar grammar_create()
{
    // define polish notation grammar
    Grammar grammar;
    grammar.Number = mpc_new("number");
    grammar.Operator = mpc_new("operator");
    grammar.Expr = mpc_new("expr");
    grammar.Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                  \
        number   : /-?[0-9]+/;                             \
        operator : '+' | '-' | '*' | '/';                  \
        expr     : <number> | '(' <operator> <expr>+ ')' ; \
        lispy    : /^/ <operator> <expr>+ /$/;             \
    ",
        grammar.Number, grammar.Operator, grammar.Expr, grammar.Lispy);
    return grammar;
}

void grammar_cleanup(Grammar* grammar)
{
    mpc_cleanup(4, grammar->Number, grammar->Operator, grammar->Expr, grammar->Lispy);
    grammar->Number = NULL;
    grammar->Operator = NULL;
    grammar->Expr = NULL;
    grammar->Lispy = NULL;
}

long eval(mpc_ast_t* t)
{
    // tagged as a number
    if (strstr(t->tag, "number")) {
        return atoi(t->contents);
    }

    // operator is always second child
    char* op = t->children[1]->contents;

    long x = eval(t->children[2]);

    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}