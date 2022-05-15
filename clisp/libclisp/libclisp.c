#include <stdio.h>
#include <stdlib.h>

#include "libclisp.h"

static lval eval_op(lval x, char* op, lval y)
{
    if (x.type == LVAL_ERR) {
        return x;
    }
    if (y.type == LVAL_ERR) {
        return y;
    }

    if (strcmp(op, "+") == 0) {
        return lval_num(x.num + y.num);
    }
    if (strcmp(op, "-") == 0) {
        return lval_num(x.num - y.num);
    }
    if (strcmp(op, "*") == 0) {
        return lval_num(x.num * y.num);
    }
    if (strcmp(op, "/") == 0) {
        return y.num == 0
            ? lval_err(LERR_DIV_ZERO)
            : lval_num(x.num / y.num);
    }

    return lval_err(LERR_BAD_OP);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

lval lval_num(long x)
{
    lval v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}

lval lval_err(int x)
{
    lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

void lval_print(lval v)
{
    switch (v.type) {
    case LVAL_NUM:
        printf("%li", v.num);
        break;
    case LVAL_ERR:
        switch (v.err) {
        case LERR_DIV_ZERO:
            printf("Error: Division by zero.");
            break;
        case LERR_BAD_OP:
            printf("Error: Invalid operator.");
            break;
        case LERR_BAD_NUM:
            printf("Error: Invalid number.");
            break;
        }
        break;
    }
}

void lval_println(lval v)
{
    lval_print(v);
    putchar('\n');
}

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

lval eval(mpc_ast_t* t)
{
    // tagged as a number
    if (strstr(t->tag, "number")) {
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }

    // operator is always second child
    char* op = t->children[1]->contents;
    lval x = eval(t->children[2]);

    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}