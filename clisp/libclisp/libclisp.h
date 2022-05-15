#ifndef LIB_CLISP_H
#define LIB_CLISP_H

#include "../libmpc/mpc.h"

typedef struct {
    mpc_parser_t* Number;
    mpc_parser_t* Operator;
    mpc_parser_t* Expr;
    mpc_parser_t* Lispy;
} Grammar;

Grammar grammar_create();
void grammar_cleanup(Grammar *grammar);

long eval(mpc_ast_t* t);

#endif