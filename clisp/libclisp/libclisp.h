#ifndef LIB_CLISP_H
#define LIB_CLISP_H

#include "../libmpc/mpc.h"

///////////////////////////////////////////////////////////////////////

struct lval;
struct lenv;
struct lgrammar;
typedef struct lval lval;
typedef struct lenv lenv;
typedef struct lgrammar lgrammar;

///////////////////////////////////////////////////////////////////////

typedef lval* (*lbuiltin)(lenv*, lval*);

enum {
    LVAL_ERR,
    LVAL_NUM,
    LVAL_SYM,
    LVAL_FUN,
    LVAL_SEXPR,
    LVAL_QEXPR
};

char* ltype_name(int t);

typedef struct lval {
    int type;

    // basics
    long num;
    char* err;
    char* sym;

    // function - either builtin or defined by user
    lbuiltin builtin;
    lenv* env;
    lval* formals;
    lval* body;

    int count;
    struct lval** cell;
} lval;

lval* lval_copy(lval* v);
lval* lval_num(long x);
lval* lval_sym(char* s);
lval* lval_sexpr();
lval* lval_qexpr();
lval* lval_fun(lbuiltin fun);
lval* lval_lambda(lval* formals, lval* body);
lval* lval_err(char* fmt, ...);
lval* lval_add(lval* v, lval* x);
lval* lval_read_num(mpc_ast_t* t);
lval* lval_read(mpc_ast_t* t);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);
lval* lval_join(lval* x, lval* y);
lval* lval_call(lenv* e, lval* f, lval* a);
void lval_del(lval* v);
void lval_print(lval* v);
void lval_println(lval* v);

///////////////////////////////////////////////////////////////////////

struct lenv {
    lenv* parent;
    int count;
    char** syms;
    lval** vals;
};

lenv* lenv_new();
lenv* lenv_copy(lenv* e);
void lenv_del(lenv* e);
lval* lenv_get(lenv* e, lval* k);
void lenv_put(lenv* e, lval* k, lval* v);
void lenv_def(lenv* e, lval* k, lval* v);
void lenv_add_builtin(lenv* e, char* name, lbuiltin func);
void lenv_add_default_builtins(lenv* e, lgrammar* g);

///////////////////////////////////////////////////////////////////////

struct lgrammar {
    mpc_parser_t* number;
    mpc_parser_t* symbol;
    mpc_parser_t* sexpr;
    mpc_parser_t* qexpr;
    mpc_parser_t* expr;
    mpc_parser_t* lispy;
};

lgrammar* lgrammar_new();
void lgrammar_del(lgrammar* grammar);

///////////////////////////////////////////////////////////////////////

lval* lval_eval(lenv* e, lval* v);
lval* lval_eval_sexpr(lenv* e, lval* v);

#endif