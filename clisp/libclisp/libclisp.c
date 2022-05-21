#include <stdio.h>
#include <stdlib.h>

#include "libclisp.h"

#define LASSERT(args, cond, err) \
    if (!(cond)) {               \
        lval_del(args);          \
        return lval_err(err);    \
    }

static lval* builtin_head(lenv* e, lval* a)
{
    // error check
    LASSERT(a, a->count == 1, "Function 'head' takes one arg");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'head' takes qexpr only");
    LASSERT(a, a->cell[0]->count != 0, "Function 'head' received empty qexpr");

    // we're good, take first arg
    lval* v = lval_take(a, 0);

    // delete elements which aren't head
    while (v->count > 1) {
        lval_del(lval_pop(v, 1));
    }
    return v;
}

static lval* builtin_tail(lenv* e, lval* a)
{
    // error check
    LASSERT(a, a->count == 1, "Function 'tail' takes one arg");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'tail' takes qexpr only");
    LASSERT(a, a->cell[0]->count != 0, "Function 'tail' received empty qexpr");

    // take first arg
    lval* v = lval_take(a, 0);

    // delete frist arg, and return rest
    lval_del(lval_pop(v, 0));

    return v;
}

static lval* builtin_list(lenv* e, lval* a)
{
    a->type = LVAL_QEXPR;
    return a;
}

static lval* builtin_eval(lenv* e, lval* a)
{
    LASSERT(a, a->count == 1, "Function 'eval' takes one arg");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'eval' takes qexpr only.");

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

static lval* builtin_join(lenv* e, lval* a)
{
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR, "Function 'join' only accepts qexprs");
    }
    lval* x = lval_pop(a, 0);
    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }
    lval_del(a);
    return x;
}

static lval* builtin_op(lenv* e, lval* a, char* op)
{
    // ensure all args are numeric
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot perform numeric operations on non-numbers");
        }
    }

    // pop the first element
    lval* x = lval_pop(a, 0);

    // if no arguments and op is sub, perform unary negation
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    // while elements remain
    while (a->count > 0) {
        lval* y = lval_pop(a, 0);
        if (strcmp(op, "+") == 0) {
            x->num += y->num;
        }
        if (strcmp(op, "-") == 0) {
            x->num -= y->num;
        }
        if (strcmp(op, "%") == 0) {
            x->num %= y->num;
        }
        if (strcmp(op, "*") == 0) {
            x->num *= y->num;
        }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x);
                lval_del(y);
                x = lval_err("Division by zero.");
                break;
            } else {
                x->num /= y->num;
            }
        }
        lval_del(y);
    }
    lval_del(a);
    return x;
}

static lval* builtin_add(lenv* e, lval* a)
{
    return builtin_op(e, a, "+");
}

static lval* builtin_sub(lenv* e, lval* a)
{
    return builtin_op(e, a, "-");
}

static lval* builtin_mul(lenv* e, lval* a)
{
    return builtin_op(e, a, "*");
}

static lval* builtin_div(lenv* e, lval* a)
{
    return builtin_op(e, a, "/");
}

static lval* builtin_mod(lenv* e, lval* a)
{
    return builtin_op(e, a, "%");
}

static void lval_expr_print(lval* v, char open, char close)
{
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);
        if (i != (v->count - 1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

///////////////////////////////////////////////////////////////////////

static lval* lval_new()
{
    lval* v = malloc(sizeof(lval));
    v->type = 0;
    v->num = 0;
    v->err = NULL;
    v->sym = NULL;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* lval_copy(lval* v)
{
    lval* x = lval_new();
    x->type = v->type;
    switch (v->type) {
    case LVAL_FUN:
        x->fun = v->fun;
        break;
    case LVAL_NUM:
        x->num = v->num;
        break;

    case LVAL_ERR:
        x->err = malloc(strlen(v->err) + 1);
        strcpy(x->err, v->err);
        break;

    case LVAL_SYM:
        x->sym = malloc(strlen(v->sym) + 1);
        strcpy(x->sym, v->sym);
        break;

    case LVAL_SEXPR:
    case LVAL_QEXPR:
        x->count = v->count;
        x->cell = malloc(sizeof(lval*) * x->count);
        for (int i = 0; i < x->count; i++) {
            x->cell[i] = lval_copy(v->cell[i]);
        }
        break;
    }

    return x;
}

lval* lval_num(long x)
{
    lval* v = lval_new();
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

lval* lval_sym(char* s)
{
    lval* v = lval_new();
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

lval* lval_sexpr()
{
    lval* v = lval_new();
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* lval_qexpr()
{
    lval* v = lval_new();
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* lval_fun(lbuiltin fun)
{
    lval* v = lval_new();
    v->type = LVAL_FUN;
    v->fun = fun;
    return v;
}

lval* lval_err(char* message)
{
    lval* v = lval_new();
    v->type = LVAL_ERR;
    v->err = malloc(strlen(message) + 1);
    strcpy(v->err, message);
    return v;
}

lval* lval_add(lval* v, lval* x)
{
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

lval* lval_read_num(mpc_ast_t* t)
{
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t)
{
    if (strstr(t->tag, "number")) {
        return lval_read_num(t);
    }
    if (strstr(t->tag, "symbol")) {
        return lval_sym(t->contents);
    }

    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) {
        // root node of ast is marked with >
        x = lval_sexpr();
    }
    if (strstr(t->tag, "sexpr")) {
        // root node of ast is marked with >
        x = lval_sexpr();
    }
    if (strstr(t->tag, "qexpr")) {
        x = lval_qexpr();
    }

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) {
            continue;
        }
        if (strcmp(t->children[i]->contents, ")") == 0) {
            continue;
        }
        if (strcmp(t->children[i]->contents, "{") == 0) {
            continue;
        }
        if (strcmp(t->children[i]->contents, "}") == 0) {
            continue;
        }
        if (strcmp(t->children[i]->tag, "regex") == 0) {
            continue;
        }
        x = lval_add(x, lval_read(t->children[i]));
    }
    return x;
}

lval* lval_pop(lval* v, int i)
{
    // find element at i
    lval* x = v->cell[i];

    // shift memory over to left
    memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval*) * (v->count - i - 1));
    v->count--;

    // realloc
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval* lval_take(lval* v, int i)
{
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* lval_join(lval* x, lval* y)
{
    // for each cell in y, add it to x
    while (y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }
    lval_del(y);
    return x;
}

void lval_del(lval* v)
{
    switch (v->type) {
    case LVAL_NUM:
    case LVAL_FUN:
        break; // no heap allocs
    case LVAL_ERR:
        free(v->err);
        break;
    case LVAL_SYM:
        free(v->sym);
        break;
    case LVAL_QEXPR:
    case LVAL_SEXPR: {
        for (int i = 0; i < v->count; i++) {
            lval_del(v->cell[i]);
        }
        free(v->cell);
        break;
    }
    }
    free(v);
}

void lval_print(lval* v)
{
    switch (v->type) {
    case LVAL_NUM:
        printf("%li", v->num);
        break;
    case LVAL_ERR:
        printf("Error: %s", v->err);
        break;
    case LVAL_SYM:
        printf("%s", v->sym);
        break;
    case LVAL_FUN:
        printf("<function>");
        break;
    case LVAL_SEXPR:
        lval_expr_print(v, '(', ')');
        break;
    case LVAL_QEXPR:
        lval_expr_print(v, '{', '}');
        break;
    }
}

void lval_println(lval* v)
{
    lval_print(v);
    putchar('\n');
}

///////////////////////////////////////////////////////////////////////

lenv* lenv_new()
{
    lenv* e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv* e)
{
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval* lenv_get(lenv* e, lval* k)
{
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }
    return lval_err("Unbound symbol");
}

void lenv_put(lenv* e, lval* k, lval* v)
{
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);

    e->vals[e->count - 1] = lval_copy(v);
    e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count - 1], k->sym);
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func)
{
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}

void lenv_add_default_builtins(lenv* e)
{
    // List Functions
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);

    // Mathematical Functions
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
    lenv_add_builtin(e, "%", builtin_mod);
}

///////////////////////////////////////////////////////////////////////

lgrammar* lgrammar_new()
{
    lgrammar* g = malloc(sizeof(lgrammar));
    g->number = mpc_new("number");
    g->symbol = mpc_new("symbol");
    g->sexpr = mpc_new("sexpr");
    g->qexpr = mpc_new("qexpr");
    g->expr = mpc_new("expr");
    g->lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                               \
        number   : /-?[0-9]+/ ;                                         \
        symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&%]+/ ;                  \
        sexpr    : '(' <expr>* ')' ;                                    \
        qexpr    : '{' <expr>* '}' ;                                    \
        expr     : <number> | <symbol> | <sexpr> | <qexpr> ;            \
        lispy    : /^/ <expr>* /$/ ;                                    \
    ",
        g->number, g->symbol, g->sexpr, g->qexpr, g->expr, g->lispy);
    return g;
}

void lgrammar_del(lgrammar* g)
{
    mpc_cleanup(6, g->number, g->symbol, g->sexpr, g->qexpr, g->expr, g->lispy);
    free(g);
}

///////////////////////////////////////////////////////////////////////

lval* lval_eval(lenv* e, lval* v)
{
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }

    // eval sexprs, otherwise pass on
    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(e, v);
    }
    return v;
}

lval* lval_eval_sexpr(lenv* e, lval* v)
{
    // eval children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    // error checking
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) {
            return lval_take(v, i);
        }
    }

    // empty epxression
    if (v->count == 0) {
        return v;
    }

    // single expression
    if (v->count == 1) {
        return lval_take(v, 0);
    }

    // first element must be a function after evaluation
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval_del(v);
        lval_del(f);
        return lval_err("S-expression must start with a function");
    }

    lval* result = f->fun(e, v);
    lval_del(f);
    return result;
}
