#ifndef LIB_CLISP_H
#define LIB_CLISP_H

#include "../libmpc/mpc.h"

///////////////////////////////////////////////////////////////////////

enum {
    LVAL_ERR,
    LVAL_NUM,
    LVAL_SYM,
    LVAL_SEXPR,
    LVAL_QEXPR
};

typedef struct lval {
    int type;
    long num;
    char* err;
    char* sym;
    int count;
    struct lval** cell;
} lval;

lval* lval_num(long x);
lval* lval_sym(char* s);
lval* lval_sexpr();
lval* lval_qexpr();
lval* lval_err(char* message);
lval* lval_add(lval* v, lval* x);
lval* lval_read_num(mpc_ast_t* t);
lval* lval_read(mpc_ast_t* t);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);
lval* lval_join(lval* x, lval* y);
void lval_del(lval* v);
void lval_print(lval* v);
void lval_println(lval* v);

///////////////////////////////////////////////////////////////////////

typedef struct {
    mpc_parser_t* number;
    mpc_parser_t* symbol;
    mpc_parser_t* sexpr;
    mpc_parser_t* qexpr;
    mpc_parser_t* expr;
    mpc_parser_t* lispy;
} lgrammar;

lgrammar* lgrammar_new();
void lgrammar_del(lgrammar* grammar);

///////////////////////////////////////////////////////////////////////

lval* lval_eval(lval* v);
lval* lval_eval_sexpr(lval* v);

#endif