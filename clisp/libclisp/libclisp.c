#include <stdio.h>
#include <stdlib.h>

#include "libclisp.h"

#define LASSERT(args, cond, err) \
    if (!(cond)) {               \
        lval_del(args);          \
        return lval_err(err);    \
    }

static lval* builtin_head(lval* a)
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

static lval* builtin_tail(lval* a)
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

static lval* builtin_list(lval* a)
{
    a->type = LVAL_QEXPR;
    return a;
}

static lval* builtin_eval(lval* a)
{
    LASSERT(a, a->count == 1, "Function 'eval' takes one arg");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'eval' takes qexpr only.");

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

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

static lval* builtin_join(lval* a)
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

static lval* builtin(lval* a, char* func)
{
    if (strcmp("list", func) == 0) {
        return builtin_list(a);
    }
    if (strcmp("head", func) == 0) {
        return builtin_head(a);
    }
    if (strcmp("tail", func) == 0) {
        return builtin_tail(a);
    }
    if (strcmp("join", func) == 0) {
        return builtin_join(a);
    }
    if (strcmp("eval", func) == 0) {
        return builtin_eval(a);
    }
    if (strstr("+-/*%", func)) {
        return builtin_op(a, func);
    }
    lval_del(a);
    return lval_err("Unrecognized function");
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

Grammar grammar_create()
{
    // define polish notation grammar
    Grammar grammar;
    grammar.Number = mpc_new("number");
    grammar.Symbol = mpc_new("symbol");
    grammar.Sexpr = mpc_new("sexpr");
    grammar.Qexpr = mpc_new("qexpr");
    grammar.Expr = mpc_new("expr");
    grammar.Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                               \
        number   : /-?[0-9]+/ ;                                         \
        symbol   : \"list\" | \"head\" | \"tail\" | \"join\" | \"eval\" \
        | '+' | '-' | '*' | '/' | '%' ;                                 \
        sexpr    : '(' <expr>* ')' ;                                    \
        qexpr    : '{' <expr>* '}' ;                                    \
        expr     : <number> | <symbol> | <sexpr> | <qexpr> ;            \
        lispy    : /^/ <expr>* /$/ ;                                    \
    ",
        grammar.Number, grammar.Symbol, grammar.Sexpr, grammar.Qexpr, grammar.Expr, grammar.Lispy);
    return grammar;
}

void grammar_cleanup(Grammar* grammar)
{
    mpc_cleanup(6, grammar->Number, grammar->Symbol, grammar->Sexpr, grammar->Qexpr, grammar->Expr, grammar->Lispy);
    grammar->Number = NULL;
    grammar->Symbol = NULL;
    grammar->Sexpr = NULL;
    grammar->Qexpr = NULL;
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
    lval* result = builtin(v, f->sym);
    lval_del(f);
    return result;
}
