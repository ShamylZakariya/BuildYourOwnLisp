#include <stdio.h>
#include <stdlib.h>

#include "libclisp.h"

static lval* builtin_op(lval* a, char* op)
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

lval* lval_num(long x)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

lval* lval_sym(char* s)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

lval* lval_sexpr()
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* lval_err(char* message)
{
    lval* v = malloc(sizeof(lval));
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

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) {
            continue;
        }
        if (strcmp(t->children[i]->contents, ")") == 0) {
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

void lval_del(lval* v)
{
    switch (v->type) {
    case LVAL_NUM:
        break; // no heap allocs
    case LVAL_ERR:
        free(v->err);
        break;
    case LVAL_SYM:
        free(v->sym);
        break;
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
    case LVAL_SEXPR:
        lval_expr_print(v, '(', ')');
        break;
    }
}

void lval_println(lval* v)
{
    lval_print(v);
    putchar('\n');
}

Grammar grammar_create()
{
    // define polish notation grammar
    Grammar grammar;
    grammar.Number = mpc_new("number");
    grammar.Symbol = mpc_new("symbol");
    grammar.Sexpr = mpc_new("sexpr");
    grammar.Expr = mpc_new("expr");
    grammar.Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                  \
        number   : /-?[0-9]+/;                             \
        symbol   : '+' | '-' | '*' | '/' | '%';            \
        sexpr    : '(' <expr>* ')';                        \
        expr     : <number> | <symbol> | <sexpr> ;         \
        lispy    : /^/ <expr>* /$/;             \
    ",
        grammar.Number, grammar.Symbol, grammar.Sexpr, grammar.Expr, grammar.Lispy);
    return grammar;
}

void grammar_cleanup(Grammar* grammar)
{
    mpc_cleanup(5, grammar->Number, grammar->Symbol, grammar->Sexpr, grammar->Expr, grammar->Lispy);
    grammar->Number = NULL;
    grammar->Symbol = NULL;
    grammar->Sexpr = NULL;
    grammar->Expr = NULL;
    grammar->Lispy = NULL;
}

lval* lval_eval(lval* v)
{
    // eval sexprs, otherwise pass on
    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(v);
    }
    return v;
}

lval* lval_eval_sexpr(lval* v)
{
    // eval children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
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

    // otherwise ensure first element is a symbol
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f);
        lval_del(v);
        return lval_err("S-expression must start with a symbol");
    }

    // call builtin
    lval* result = builtin_op(v, f->sym);
    lval_del(f);
    return result;
}
