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

/* ---------- Aritmética de constantes (compartida por el AST y el TAC) ---------- */

/* Valor constante: int o boolean */
struct cval {
    int is_bool;
    int v;
};

/* Aritmética de 32 bits con wrap, igual que el código generado (movslq).
   Se calcula en unsigned porque el overflow de int con signo es comportamiento indefinido en C. */
static int wrap(unsigned v) { return (int) v; }

static struct cval cint(int v)  { struct cval c = { 0, v }; return c; }
static struct cval cbool(int v) { struct cval c = { 1, v ? 1 : 0 }; return c; }

/* Calcula x op y. Devuelve 1 y deja el resultado en *out, o 0 si no se puede calcular
   (tipos que no corresponden al operador, o división por 0, que tiene que fallar al ejecutar). */
static int eval_binop(enum op_type op, struct cval x, struct cval y, struct cval *out) {
    if (op == OP_EQ && x.is_bool == y.is_bool) { *out = cbool(x.v == y.v); return 1; }
    if (op == OP_AND && x.is_bool && y.is_bool) { *out = cbool(x.v && y.v); return 1; }
    if (op == OP_OR  && x.is_bool && y.is_bool) { *out = cbool(x.v || y.v); return 1; }
    if (x.is_bool || y.is_bool) return 0;

    int a = x.v, b = y.v;
    switch (op) {
        case OP_ADD: *out = cint(wrap((unsigned) a + (unsigned) b)); return 1;
        case OP_SUB: *out = cint(wrap((unsigned) a - (unsigned) b)); return 1;
        case OP_MUL: *out = cint(wrap((unsigned) a * (unsigned) b)); return 1;
        case OP_DIV:
        case OP_MOD:
            if (b == 0) return 0;                               /* x / 0: no se calcula, queda el error en ejecución */
            if (a == INT_MIN && b == -1)                        /* en C es indefinido; el código generado da INT_MIN y 0 */
                *out = cint(op == OP_DIV ? INT_MIN : 0);
            else
                *out = cint(op == OP_DIV ? a / b : a % b);      /* C trunca hacia 0, igual que idivq */
            return 1;
        case OP_LT: *out = cbool(a < b); return 1;
        case OP_GT: *out = cbool(a > b); return 1;
        default:    return 0;
    }
}

static int eval_unop(enum op_type op, struct cval x, struct cval *out) {
    if (op == OP_NEG && !x.is_bool) { *out = cint(wrap(0u - (unsigned) x.v)); return 1; }
    if (op == OP_NOT && x.is_bool)  { *out = cbool(!x.v); return 1; }
    return 0;
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

/* Valor de un literal; 0 si el nodo no es un literal int o boolean */
static int node_cval(struct node *n, struct cval *c) {
    if (is_int(n))  { *c = cint(n->value.ival);  return 1; }
    if (is_bool(n)) { *c = cbool(n->value.ival); return 1; }
    return 0;
}

static void set_node(struct node *n, struct cval c) {
    if (c.is_bool) make_bool(n, c.v);
    else           make_int(n, c.v);
}

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

    struct cval x, y, out;
    if (node_cval(l, &x) && node_cval(r, &y) && eval_binop(n->op, x, y, &out))
        set_node(n, out);
}

static void fold_unop(struct node *n) {
    struct cval x, out;
    if (node_cval(n->left, &x) && eval_unop(n->op, x, &out))
        set_node(n, out);
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

/* Propagación de constantes: recorre el TAC con una tabla "variable o temporal → valor conocido".
   Reemplaza cada lectura por su valor y, si todos los operandos quedan constantes, calcula el resultado.
   Es local (por bloque básico): la tabla se vacía en cada etiqueta, porque a una etiqueta se llega
   por varios caminos (if, while), y en cada call se olvidan las globales, que la función puede modificar. */

#define MAX_KNOWN 1024

struct known {
    struct operand key;     /* OPD_VAR (por símbolo) u OPD_TEMP (por número) */
    struct cval val;
};

static struct known known[MAX_KNOWN];
static int nknown;

static int same_place(struct operand *a, struct operand *b) {
    if (a->kind != b->kind) return 0;
    if (a->kind == OPD_VAR)  return a->sym == b->sym;
    if (a->kind == OPD_TEMP) return a->temp_id == b->temp_id;
    return 0;
}

static struct known *lookup(struct operand *o) {
    for (int i = 0; i < nknown; i++)
        if (same_place(&known[i].key, o)) return &known[i];
    return NULL;
}

/* El destino recibió un valor desconocido: se olvida */
static void forget(struct operand *o) {
    for (int i = 0; i < nknown; i++)
        if (same_place(&known[i].key, o)) { known[i] = known[--nknown]; return; }
}

static void remember(struct operand *o, struct cval v) {
    struct known *k = lookup(o);
    if (k) { k->val = v; return; }
    if (nknown < MAX_KNOWN) { known[nknown].key = *o; known[nknown].val = v; nknown++; }
}

static void forget_globals(void) {
    for (int i = 0; i < nknown; )
        if (known[i].key.kind == OPD_VAR && known[i].key.sym && known[i].key.sym->is_global)
            known[i] = known[--nknown];
        else
            i++;
}

static int opd_cval(struct operand *o, struct cval *c) {
    if (o->kind == OPD_INT)  { *c = cint(o->ival);  return 1; }
    if (o->kind == OPD_BOOL) { *c = cbool(o->ival); return 1; }
    return 0;
}

static struct operand cval_opd(struct cval c) {
    struct operand o;
    memset(&o, 0, sizeof(o));
    o.kind = c.is_bool ? OPD_BOOL : OPD_INT;
    o.ival = c.v;
    return o;
}

/* 1 si alguna pasada sobre el TAC modificó una instrucción (para repetir hasta un punto fijo).
   Anotar u olvidar valores en la tabla no cuenta: solo cambios en el código. */
static int tac_changed;

/* Si el operando es una variable o temporal con valor conocido, lo reemplaza por la constante */
static void substitute(struct operand *o) {
    if (o->kind != OPD_VAR && o->kind != OPD_TEMP) return;
    struct known *k = lookup(o);
    if (k) {
        *o = cval_opd(k->val);
        tac_changed = 1;
    }
}

/* La instrucción i escribe una constante en su destino: se convierte en "res = c" y se anota */
static void becomes_const(struct tac *i, struct cval c) {
    tac_changed = 1;
    i->op = TAC_ASSIGN;
    i->bop = OP_NONE;
    i->a = cval_opd(c);
    memset(&i->b, 0, sizeof(i->b));
    remember(&i->res, c);
}

/* Borra las asignaciones a temporales que ninguna instrucción lee (quedan después de propagar:
   "t0 = 20" cuando todos los usos de t0 ya se reemplazaron por 20). Solo TAC_ASSIGN, que no tiene
   efectos; una operación como t = x / y no se borra porque puede fallar al ejecutar. */
static struct tac *remove_unused_temps(struct tac *code) {
    int max_temp = -1;
    for (struct tac *i = code; i; i = i->next) {
        if (i->a.kind == OPD_TEMP && i->a.temp_id > max_temp) max_temp = i->a.temp_id;
        if (i->b.kind == OPD_TEMP && i->b.temp_id > max_temp) max_temp = i->b.temp_id;
        if (i->res.kind == OPD_TEMP && i->res.temp_id > max_temp) max_temp = i->res.temp_id;
    }
    if (max_temp < 0) return code;

    char *read = calloc(max_temp + 1, 1);       /* read[n] = 1 si tn se lee en algún lado */
    for (struct tac *i = code; i; i = i->next) {
        if (i->a.kind == OPD_TEMP) read[i->a.temp_id] = 1;
        if (i->b.kind == OPD_TEMP) read[i->b.temp_id] = 1;
    }

    struct tac *head = code, *prev = NULL;
    for (struct tac *i = code; i; ) {
        struct tac *next = i->next;
        if (i->op == TAC_ASSIGN && i->res.kind == OPD_TEMP && !read[i->res.temp_id]) {
            if (prev) prev->next = next; else head = next;
            tac_changed = 1;
        } else {
            prev = i;
        }
        i = next;
    }
    free(read);
    return head;
}

static struct tac *propagate_constants(struct tac *code) {
    for (struct tac *i = code; i; i = i->next) {
        struct cval x, y, out;
        switch (i->op) {
            case TAC_FUNC_BEGIN:
            case TAC_FUNC_END:
            case TAC_LABEL:
                nknown = 0;                     /* nuevo bloque básico: no se sabe por dónde se llegó */
                break;
            case TAC_ASSIGN:
                substitute(&i->a);
                if (opd_cval(&i->a, &x)) remember(&i->res, x);
                else forget(&i->res);
                break;
            case TAC_BINOP:
                substitute(&i->a);
                substitute(&i->b);
                if (opd_cval(&i->a, &x) && opd_cval(&i->b, &y) && eval_binop(i->bop, x, y, &out))
                    becomes_const(i, out);
                else
                    forget(&i->res);
                break;
            case TAC_UNOP:
                substitute(&i->a);
                if (opd_cval(&i->a, &x) && eval_unop(i->bop, x, &out))
                    becomes_const(i, out);
                else
                    forget(&i->res);
                break;
            case TAC_IFFALSE:
            case TAC_PARAM:
            case TAC_RETURN:
                substitute(&i->a);
                break;
            case TAC_CALL:
                forget_globals();               /* la función llamada puede modificar cualquier global */
                if (i->has_result) forget(&i->res);
                break;
            default:
                break;
        }
    }
    return remove_unused_temps(code);
}

/* Eliminación de código inalcanzable. Reglas:
     ifFalse true goto L      → se borra (nunca salta)
     ifFalse false goto L     → goto L (siempre salta)
     después de goto / return → se borra todo hasta la próxima etiqueta (no se puede llegar)
     goto L seguido de L:     → se borra el goto
     L: que ningún salto usa  → se borra
   Se repite hasta que no cambie nada, porque una regla habilita a otra (borrar un goto deja una
   etiqueta sin uso, y sin la etiqueta el código que sigue puede quedar inalcanzable).
   Siempre termina: cada vuelta que cambia algo borra al menos una instrucción. */

/* ¿Algún salto (goto o ifFalse) va a la etiqueta l? */
static int label_used(struct tac *code, const char *l) {
    for (struct tac *i = code; i; i = i->next)
        if ((i->op == TAC_GOTO || i->op == TAC_IFFALSE) && strcmp(i->label, l) == 0)
            return 1;
    return 0;
}

static struct tac *remove_dead_code(struct tac *code) {
    int changed = 1;
    while (changed) {
        changed = 0;
        int dead = 0;                   /* 1 después de goto o return: no se puede llegar hasta una etiqueta */
        struct tac **pp = &code;        /* puntero al 'next' anterior, para desenganchar instrucciones */
        while (*pp) {
            struct tac *i = *pp;
            int remove = 0;

            if (i->op == TAC_LABEL) {
                if (!label_used(code, i->label)) remove = 1;    /* sin saltos: no corta el código inalcanzable */
                else dead = 0;                                  /* se puede llegar saltando */
            } else if (i->op == TAC_FUNC_BEGIN || i->op == TAC_FUNC_END) {
                dead = 0;
            } else if (dead) {
                remove = 1;
            } else if (i->op == TAC_IFFALSE && i->a.kind == OPD_BOOL) {
                if (i->a.ival) {
                    remove = 1;                                 /* ifFalse true: nunca salta */
                } else {
                    i->op = TAC_GOTO;                           /* ifFalse false: siempre salta */
                    memset(&i->a, 0, sizeof(i->a));
                    changed = 1;
                }
            } else if (i->op == TAC_GOTO && i->next && i->next->op == TAC_LABEL
                       && strcmp(i->label, i->next->label) == 0) {
                remove = 1;                                     /* salta a la instrucción siguiente */
            }

            if (remove) {
                *pp = i->next;
                changed = 1;
                continue;
            }
            if (i->op == TAC_GOTO || i->op == TAC_RETURN) dead = 1;
            pp = &i->next;
        }
        if (changed) tac_changed = 1;
    }
    return code;
}

/* Termina porque ninguna deshace lo que hace la otra, pero se define MAX_ROUNDS como seguridad 
    Si llega el código esta menos optimizado. */
#define MAX_ROUNDS 100

struct tac *optimize_tac(struct tac *code, const struct opt_flags *opts) {
    int rounds = 0;
    do {
        tac_changed = 0;
        if (opts->constprop) code = propagate_constants(code);
        if (opts->deadcode)  code = remove_dead_code(code);
    } while (tac_changed && ++rounds < MAX_ROUNDS);
    return code;
}
