#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "optimizer.h"

/* Nombres aceptados por -opt, en el orden de la estructura */
static const char *opt_names[] = { "constfold", "constprop", "deadcode", "tempslots" };
#define N_OPTS (int)(sizeof(opt_names) / sizeof(opt_names[0]))

static int *opt_field(struct opt_flags *o, int k) {
    switch (k) {
        case 0:  return &o->constfold;
        case 1:  return &o->constprop;
        case 2:  return &o->deadcode;
        default: return &o->tempslots;
    }
}

int parse_opt_list(const char *list, struct opt_flags *opts) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", list);
    int valid = 1;
    for (char *name = strtok(buf, ","); name; name = strtok(NULL, ",")) {
        if (strcmp(name, "all") == 0) {
            for (int k = 0; k < N_OPTS; k++) *opt_field(opts, k) = 1;
            continue;
        }
        int found = 0;
        for (int k = 0; k < N_OPTS; k++)
            if (strcmp(name, opt_names[k]) == 0) { *opt_field(opts, k) = 1; found = 1; }
        if (!found) {
            fprintf(stderr, "Optimización desconocida: '%s' (válidas: all", name);
            for (int k = 0; k < N_OPTS; k++) fprintf(stderr, ", %s", opt_names[k]);
            fprintf(stderr, ")\n");
            valid = 0;
        }
    }
    return valid;
}

int any_opt(const struct opt_flags *opts) {
    return opts->constfold || opts->constprop || opts->deadcode || opts->tempslots;
}

/* ---------- AST ---------- */

/* Plegado de constantes: reemplaza operaciones entre literales por su resultado.
   Solo int y boolean (float no tiene back-end). Los nodos se modifican en el lugar,
   así el padre no necesita actualizar su puntero y se conserva el 'next' de las listas. */

static int is_int(struct node *n)  { return n && n->kind == NODE_INT; }
static int is_bool(struct node *n) { return n && n->kind == NODE_BOOL; }

/* Convierte n en un literal, conservando su línea y su 'next' */
static void make_int(struct node *n, int v) {
    n->kind = NODE_INT;
    n->op = OP_NONE;
    n->value.ival = v;
    n->data_type = TYPE_INT;
    n->left = n->right = n->third = NULL;
}

static void make_bool(struct node *n, int v) {
    make_int(n, v ? 1 : 0);
    n->kind = NODE_BOOL;
    n->data_type = TYPE_BOOL;
}

/* Reemplaza n por su hijo c (para "true && e" → e), conservando el 'next' de n */
static void replace_with(struct node *n, struct node *c) {
    struct node *next = n->next;
    *n = *c;
    n->next = next;
}

/* Aritmética de 32 bits con wrap, igual que el código generado (movslq).
   Se calcula en unsigned porque el overflow de int con signo es comportamiento indefinido en C. */
static int wrap(unsigned v) { return (int) v; }

static void fold_binop(struct node *n) {
    struct node *l = n->left, *r = n->right;

    /* && y ||: alcanza con que el operando izquierdo sea literal (cortocircuito) */
    if (n->op == OP_AND || n->op == OP_OR) {
        int absorb = (n->op == OP_OR);        /* valor que decide el resultado: false para &&, true para || */
        if (is_bool(l)) {
            if (l->value.ival == absorb) make_bool(n, absorb);  /* false && e → false,  true || e → true */
            else replace_with(n, r);                             /* true && e → e,      false || e → e */
        } else if (is_bool(r) && r->value.ival != absorb) {
            replace_with(n, l);                                  /* e && true → e,      e || false → e */
        }
        /* e && false y e || true no se pliegan: e se evalúa igual y puede tener efectos (llamadas) */
        return;
    }

    if (n->op == OP_EQ && is_bool(l) && is_bool(r)) {
        make_bool(n, l->value.ival == r->value.ival);
        return;
    }

    if (!is_int(l) || !is_int(r)) return;
    int a = l->value.ival, b = r->value.ival;
    switch (n->op) {
        case OP_ADD: make_int(n, wrap((unsigned) a + (unsigned) b)); break;
        case OP_SUB: make_int(n, wrap((unsigned) a - (unsigned) b)); break;
        case OP_MUL: make_int(n, wrap((unsigned) a * (unsigned) b)); break;
        case OP_DIV:
        case OP_MOD:
            if (b == 0) return;                                 /* x / 0: no se pliega, queda el error en ejecución */
            if (a == INT_MIN && b == -1)                        /* en C es indefinido; el código generado da INT_MIN y 0 */
                make_int(n, n->op == OP_DIV ? INT_MIN : 0);
            else
                make_int(n, n->op == OP_DIV ? a / b : a % b);   /* C trunca hacia 0, igual que idivq */
            break;
        case OP_LT: make_bool(n, a < b);  break;
        case OP_GT: make_bool(n, a > b);  break;
        case OP_EQ: make_bool(n, a == b); break;
        default: break;
    }
}

static void fold_unop(struct node *n) {
    struct node *e = n->left;
    if (n->op == OP_NEG && is_int(e))
        make_int(n, wrap(0u - (unsigned) e->value.ival));
    else if (n->op == OP_NOT && is_bool(e))
        make_bool(n, !e->value.ival);
}

/* Recorre el AST en post-orden: primero los hijos, así "2 + 3 * 4" pliega 3 * 4 y después 2 + 12 */
static void fold_constants(struct node *n) {
    for (; n; n = n->next) {
        fold_constants(n->left);
        fold_constants(n->right);
        fold_constants(n->third);
        if (n->kind == NODE_BINOP) fold_binop(n);
        else if (n->kind == NODE_UNOP) fold_unop(n);
    }
}

void optimize_ast(struct node *root, const struct opt_flags *opts) {
    if (opts->constfold) fold_constants(root);
}

/* ---------- TAC ---------- */

/* Propagación de constantes: usa el valor conocido de una variable en lugar de leerla.
   TODO: implementar. */
static struct tac *propagate_constants(struct tac *code) {
    return code;
}

/* Eliminación de código inalcanzable: instrucciones después de return/goto hasta la próxima etiqueta.
   TODO: implementar. */
static struct tac *remove_dead_code(struct tac *code) {
    return code;
}

struct tac *optimize_tac(struct tac *code, const struct opt_flags *opts) {
    if (opts->constprop) code = propagate_constants(code);
    if (opts->deadcode)  code = remove_dead_code(code);
    return code;
}
