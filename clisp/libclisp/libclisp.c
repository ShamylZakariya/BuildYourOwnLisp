#include <stdio.h>
#include <stdlib.h>

#include "libclisp.h"

#define LASSERT(args, cond, fmt, ...)             \
    if (!(cond)) {                                \
        lval* err = lval_err(fmt, ##__VA_ARGS__); \
        lval_del(args);                           \
        return err;                               \
    }

#define LASSERT_TYPE(func, args, index, expect)                 \
    LASSERT(args, args->cell[index]->type == expect,            \
        "Function '%s' passed incorrect type for argument %i. " \
        "Got %s, Expected %s.",                                 \
        func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_NUM(func, args, num)                           \
    LASSERT(args, args->count == num,                          \
        "Function '%s' passed incorrect number of arguments. " \
        "Got %i, Expected %i.",                                \
        func, args->count, num)

#define LASSERT_NOT_EMPTY(func, args, index)     \
    LASSERT(args, args->cell[index]->count != 0, \
        "Function '%s' passed {} for argument %i.", func, index);

static lval* builtin_head(lenv* e, lval* a)
{
    // error check
    LASSERT(a, a->count == 1, "Function 'head' passed too many arguments. Got %i, epxected %i", a->count, 1);
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'head' passed inccorect type for argument 0. Got %s, Expected %s", ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
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
    LASSERT(a, a->count == 1, "Function 'tail' passed too many arguments. Got %i, epxected %i", a->count, 1);
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'tail' passed inccorect type for argument 0. Got %s, Expected %s", ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
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
    LASSERT(a, a->count == 1, "Function 'eval' passed too many arguments. Got %i, expected %i", a->count, 1);
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'eval' passed incorrect type for argument 0. Got %s expected %s", ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

static lval* builtin_join(lenv* e, lval* a)
{
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR, "Incorrect argument type passed to 'join'. Argument %i was type %s, expected %s", i, ltype_name(a->cell[i]->type), ltype_name(LVAL_QEXPR));
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
            lval* err = lval_err("Numeric operator %s passed incorrect type for argument %i. Got %s, expected: %s", op, i, ltype_name(a->cell[i]->type), ltype_name(LVAL_NUM));
            lval_del(a);
            return err;
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

static lval* builtin_var(lenv* e, lval* a, char* func)
{
    LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

    lval* syms = a->cell[0];
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
            "Function '%s' cannot define non-symbol. Got %s expected %s",
            func, ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
    }

    LASSERT(a, (syms->count == a->count - 1),
        "Function '%s' passed wrong argument count for symbol. Got %i expected %i",
        func, syms->count, a->count - 1);

    for (int i = 0; i < syms->count; i++) {
        // if "def" define globalls. If "put" define locally
        if (strcmp(func, "def") == 0) {
            lenv_def(e, syms->cell[i], a->cell[i + 1]);
        }
        if (strcmp(func, "put") == 0) {
            lenv_put(e, syms->cell[i], a->cell[i + 1]);
        }
    }
    lval_del(a);
    return lval_sexpr();
}

static lval* builtin_def(lenv* e, lval* a)
{
    return builtin_var(e, a, "def");
}

static lval* builtin_put(lenv* e, lval* a)
{
    return builtin_var(e, a, "=");
}

static lval* builtin_lambda(lenv* e, lval* a)
{
    // requires two args, both QEXPR
    LASSERT_NUM("\\", a, 2);
    LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
    LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

    // first QEXPR may only contain symbols
    for (int i = 0; i < a->cell[0]->count; i++) {
        LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
            "Cannot define non-symbol. Got %s expected %s",
            ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
    }

    // pop first two args and pass them to lval_lambda
    lval* formals = lval_pop(a, 0);
    lval* body = lval_pop(a, 0);
    lval_del(a);
    return lval_lambda(formals, body);
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
    v->builtin = NULL;
    v->env = NULL;
    v->formals = NULL;
    v->body = NULL;
    v->count = 0;
    v->cell = NULL;
    return v;
}

char* ltype_name(int t)
{
    switch (t) {
    case LVAL_FUN:
        return "Function";
    case LVAL_NUM:
        return "Number";
    case LVAL_ERR:
        return "Error";
    case LVAL_SYM:
        return "Symbol";
    case LVAL_SEXPR:
        return "S-Expression";
    case LVAL_QEXPR:
        return "Q-Expression";
    default:
        return "Unknown";
    }
}

lval* lval_copy(lval* v)
{
    lval* x = lval_new();
    x->type = v->type;
    switch (v->type) {
    case LVAL_FUN:
        if (v->builtin) {
            x->builtin = v->builtin;
        } else {
            x->builtin = NULL;
            x->env = lenv_copy(v->env);
            x->formals = lval_copy(v->formals);
            x->body = lval_copy(v->body);
        }
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
    v->builtin = fun;
    return v;
}

lval* lval_lambda(lval* formals, lval* body)
{
    lval* v = lval_new();
    v->type = LVAL_FUN;

    v->env = lenv_new();
    v->formals = formals;
    v->body = body;
    return v;
}

lval* lval_err(char* fmt, ...)
{
    lval* v = lval_new();
    v->type = LVAL_ERR;

    va_list va;
    va_start(va, fmt);

    v->err = malloc(512);
    vsnprintf(v->err, 511, fmt, va);
    v->err = realloc(v->err, strlen(v->err) + 1);

    va_end(va);

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
    return errno != ERANGE ? lval_num(x) : lval_err("Unable to parse \"%s\" to a number", t->contents);
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

lval* lval_call(lenv* e, lval* f, lval* a)
{
    // if is builtin, dispatch
    if (f->builtin) {
        return f->builtin(e, a);
    }

    // record argument counts
    int given = a->count;
    int total = f->formals->count;

    while (a->count) {
        // if we're run out of formal arguments to bind
        if (f->formals->count == 0) {
            lval_del(a);
            return lval_err("Function was passed too many arguments. Got %i expected %i",
                given, total);
        }

        // pop first symbol from the formals
        lval* sym = lval_pop(f->formals, 0);

        // handle &
        if (strcmp(sym->sym, "&") == 0) {
            // ensure & is followed by a symbol
            if (f->formals->count != 1) {
                lval_del(a);
                return lval_err("Function format invalid. Symbol '&' not followed by single symbol");
            }

            // nex format should be bound to remaining arguments
            lval* nsym = lval_pop(f->formals, 0);
            lenv_put(f->env, nsym, builtin_list(e, a));
            lval_del(sym);
            lval_del(nsym);
            break;
        }

        // pop next argument from the list
        lval* val = lval_pop(a, 0);

        // bind a copy into the functions environment
        lenv_put(f->env, sym, val);

        lval_del(sym);
        lval_del(val);
    }

    // argument list is bound so it can be freed
    lval_del(a);

    // if & remains in formal list bind to empty list
    if (f->formals->count > 0 && strcmp(f->formals->cell[0]->sym, "&") == 0) {
        if (f->formals->count != 2) {
            return lval_err("Function format invalid. Symbol '&' not followed by single symbol");
        }

        // pop and delete & symbol
        lval_del(lval_pop(f->formals, 0));

        // pop next symbol and create empty list
        lval* sym = lval_pop(f->formals, 0);
        lval* val = lval_qexpr();

        // bind to env and delete
        lenv_put(f->env, sym, val);
        lval_del(sym);
        lval_del(val);
    }

    // if all formals have been bound we can eval
    if (f->formals->count == 0) {
        // set env parent to evaluation env
        f->env->parent = e;

        // evaluate and return
        return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
    } else {
        // return partially evaluated function
        return lval_copy(f);
    }
}

void lval_del(lval* v)
{
    switch (v->type) {
    case LVAL_NUM:
        break;
    case LVAL_FUN:
        if (!v->builtin) {
            lenv_del(v->env);
            lval_del(v->formals);
            lval_del(v->body);
        }
        break;
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
        if (v->builtin) {
            printf("<function>");
        } else {
            printf("(\\ "); // we're using \ as lambda symbol
            lval_print(v->formals);
            putchar(' ');
            lval_print(v->body);
            putchar(')');
        }
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
    e->parent = NULL;
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

lenv* lenv_copy(lenv* e)
{
    lenv* n = lenv_new();
    n->parent = e->parent;
    n->count = e->count;
    n->syms = malloc(sizeof(char*) * n->count);
    n->vals = malloc(sizeof(lval*) * n->count);
    for (int i = 0; i < e->count; i++) {
        n->syms[i] = malloc(strlen(e->syms[i]) + 1);
        strcpy(n->syms[i], e->syms[i]);
        n->vals[i] = lval_copy(e->vals[i]);
    }
    return n;
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
    if (e->parent) {
        return lenv_get(e->parent, k);
    } else {
        return lval_err("Unbound symbol '%s'", k->sym);
    }
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

void lenv_def(lenv* e, lval* k, lval* v)
{
    while (e->parent) {
        e = e->parent;
    }
    lenv_put(e, k, v);
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func)
{
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}

void lenv_add_default_builtins(lenv* e, lgrammar* g)
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

    // user definitions
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "\\", builtin_lambda);
    lenv_add_builtin(e, "=", builtin_put);

    // builtin definitions
    const int num_definitions = 5;
    const char* definitions[num_definitions];

    // `fun` definition, which allows for, e.g., "fun {add-together x y} {+ x y}"
    definitions[0] = "def {fun} (\\ {args body} {def (head args) (\\ (tail args) body)})";

    // the mechanisms currying will use
    definitions[1] = "fun {unpack f xs} {eval (join (list f) xs)}";
    definitions[2] = "fun {pack f & xs} {f xs}";

    // currying
    definitions[3] = "def {uncurry} pack";
    definitions[4] = "def {curry} unpack";

    for (int i = 0; i < num_definitions; i++) {
        mpc_result_t r;
        if (mpc_parse("<stdin>", definitions[i], g->lispy, &r)) {
            lval* x = lval_read(r.output);
            x = lval_eval(e, x);
            lval_del(x);
            mpc_ast_delete(r.output);
        }
    }
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
        lval* err = lval_err("S-expression must start with a function. Got '%s', expected: '%s'", ltype_name(f->type), ltype_name(LVAL_FUN));
        lval_del(v);
        lval_del(f);
        return err;
    }

    lval* result = lval_call(e, f, v);
    lval_del(f);
    return result;
}
