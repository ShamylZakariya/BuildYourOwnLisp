#ifndef LIB_CLISP_H
#define LIB_CLISP_H

#include "../libmpc/mpc.h"

typedef struct {
    mpc_parser_t* Number;
    mpc_parser_t* Operator;
    mpc_parser_t* Expr;
    mpc_parser_t* Lispy;
} Grammar;

enum { LVAL_NUM,
    LVAL_ERR };
enum { LERR_DIV_ZERO,
    LERR_BAD_OP,
    LERR_BAD_NUM };

typedef struct {
    int type;
    union {
        long num;
        int err;
    };
} lval;

lval lval_num(long x);
lval lval_err(int x);
void lval_print(lval v);
void lval_println(lval v);

Grammar grammar_create();
void grammar_cleanup(Grammar* grammar);

lval eval(mpc_ast_t* t);

#endif