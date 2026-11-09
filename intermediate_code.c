#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "intermediate_code.h"
#include "ast.h"
#include "tac.h"
#include "symbol_table.h"

static int temp_count = 0;
static int label_count = 0;

static struct tac *head = NULL;
static struct tac *tail = NULL;

/* lista de instrucciones */

/* Crea una instrucción vacía, la agrega al final y la devuelve para completarla */
static struct tac *emit(enum tac_op op)
{
    struct tac *t = calloc(1, sizeof(struct tac));
    t->op = op;
    if (tail)
        tail->next = t;
    else
        head = t;
    tail = t;
    return t;
}

/* operandos */

static struct operand opd_none(void)
{
    struct operand o;
    memset(&o, 0, sizeof(o));
    return o;
}

static struct operand new_temp(void)
{
    struct operand o = opd_none();
    o.kind = OPD_TEMP;
    o.temp_id = temp_count++;
    return o;
}

static char *new_label(void)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "L%d", label_count++);
    return strdup(buf);
}

/* Nombre de una variable en el TAC. Dos símbolos distintos con el mismo nombre
   (ámbitos anidados) se distinguen con sufijo: x, x_1, x_2... */
#define MAX_VARS 1024
static struct symbol *seen_syms[MAX_VARS];
static char *seen_names[MAX_VARS];
static int nseen = 0;

static char *var_name(struct node *n)
{
    if (!n->sym)
        return n->id;
    for (int i = 0; i < nseen; i++)
        if (seen_syms[i] == n->sym)
            return seen_names[i];
    if (nseen >= MAX_VARS)
        return n->id;
    char buf[256];
    int suffix = 0;
    for (;;)
    {
        if (suffix)
            snprintf(buf, sizeof(buf), "%s_%d", n->sym->name, suffix);
        else
            snprintf(buf, sizeof(buf), "%s", n->sym->name);
        int used = 0;
        for (int i = 0; i < nseen; i++)
            if (strcmp(seen_names[i], buf) == 0) { used = 1; break; }
        if (!used)
            break;
        suffix++;
    }
    seen_syms[nseen] = n->sym;
    seen_names[nseen] = strdup(buf);
    return seen_names[nseen++];
}

static struct operand opd_var(struct node *n)
{
    struct operand o = opd_none();
    o.kind = OPD_VAR;
    o.sym = n->sym;
    o.name = var_name(n);
    return o;
}

/* generación */

static struct operand gen_expr(struct node *e);
static void gen_stmt(struct node *s);
static void gen_block(struct node *b);

static struct operand gen_expr(struct node *e)
{
    struct operand none = opd_none();
    if (!e)
        return none;

    switch (e->kind)
    {
    case NODE_INT:
    {
        struct operand o = opd_none();
        o.kind = OPD_INT;
        o.ival = e->value.ival;
        return o;
    }
    case NODE_FLOAT:
    {
        struct operand o = opd_none();
        o.kind = OPD_FLOAT;
        o.fval = e->value.fval;
        return o;
    }
    case NODE_BOOL:
    {
        struct operand o = opd_none();
        o.kind = OPD_BOOL;
        o.ival = e->value.ival ? 1 : 0;
        return o;
    }
    case NODE_ID:
        return opd_var(e);

    case NODE_BINOP:
    {
        if (e->op == OP_AND || e->op == OP_OR)
        {
            struct operand t = new_temp();
            char *l_short = new_label(); /* salto cuando a es false */
            char *l_end = new_label();

            struct operand l = gen_expr(e->left);
            struct tac *f = emit(TAC_IFFALSE);
            f->a = l;
            f->label = l_short;

            if (e->op == OP_AND)
            {
                /* a era true: el resultado es b */
                struct operand r = gen_expr(e->right);
                struct tac *as = emit(TAC_ASSIGN);
                as->a = r;
                as->res = t;
                emit(TAC_GOTO)->label = l_end;

                emit(TAC_LABEL)->label = l_short; /* a era false: resultado false */
                struct tac *fl = emit(TAC_ASSIGN);
                fl->a.kind = OPD_BOOL;
                fl->a.ival = 0;
                fl->res = t;
            }
            else
            {
                /* a era true: el resultado es true */
                struct tac *tr = emit(TAC_ASSIGN);
                tr->a.kind = OPD_BOOL;
                tr->a.ival = 1;
                tr->res = t;
                emit(TAC_GOTO)->label = l_end;

                emit(TAC_LABEL)->label = l_short; /* a era false: el resultado es b */
                struct operand r = gen_expr(e->right);
                struct tac *as = emit(TAC_ASSIGN);
                as->a = r;
                as->res = t;
            }
            emit(TAC_LABEL)->label = l_end;
            return t;
        }

        struct operand l = gen_expr(e->left);
        struct operand r = gen_expr(e->right);
        struct operand t = new_temp();
        struct tac *i = emit(TAC_BINOP);
        i->bop = e->op;
        i->a = l;
        i->b = r;
        i->res = t;
        return t;
    }
    case NODE_UNOP:
    {
        struct operand a = gen_expr(e->left);
        struct operand t = new_temp();
        struct tac *i = emit(TAC_UNOP);
        i->bop = e->op;
        i->a = a;
        i->res = t;
        return t;
    }
    case NODE_CALL:
    {
        /* Evaluar todos los argumentos primero y después emitir los param */
        int nargs = 0;
        struct operand args[128];
        for (struct node *arg = e->left; arg && nargs < 128; arg = arg->next)
        {
            args[nargs++] = gen_expr(arg);
        }
        for (int k = 0; k < nargs; k++)
        {
            struct tac *p = emit(TAC_PARAM);
            p->a = args[k];
        }
        struct tac *c = emit(TAC_CALL);
        c->label = strdup(e->id);
        c->func = e->sym;
        c->nargs = nargs;
        if (e->data_type != TYPE_VOID)
        {
            struct operand t = new_temp();
            c->has_result = 1;
            c->res = t;
            return t;
        }
        return none;
    }
    default:
        return none;
    }
}

static void gen_stmt(struct node *s)
{
    if (!s)
        return;

    switch (s->kind)
    {
    case NODE_ASSIGN:
    {
        struct operand val = gen_expr(s->right);
        struct tac *i = emit(TAC_ASSIGN);
        i->a = val;
        if (s->left)
            i->res = opd_var(s->left);
        break;
    }
    case NODE_CALL:
        gen_expr(s);
        break;

    case NODE_IF:
    {
        struct operand cond = gen_expr(s->left);
        char *l_end = new_label();
        if (s->third)
        {
            char *l_else = new_label();
            struct tac *f = emit(TAC_IFFALSE);
            f->a = cond;
            f->label = l_else;

            gen_block(s->right);
            emit(TAC_GOTO)->label = l_end;

            emit(TAC_LABEL)->label = l_else;
            gen_block(s->third);

            emit(TAC_LABEL)->label = l_end;
        }
        else
        {
            struct tac *f = emit(TAC_IFFALSE);
            f->a = cond;
            f->label = l_end;

            gen_block(s->right);

            emit(TAC_LABEL)->label = l_end;
        }
        break;
    }
    case NODE_WHILE:
    {
        char *l_start = new_label();
        char *l_end = new_label();

        emit(TAC_LABEL)->label = l_start;
        struct operand cond = gen_expr(s->left);
        struct tac *f = emit(TAC_IFFALSE);
        f->a = cond;
        f->label = l_end;

        gen_block(s->right);
        emit(TAC_GOTO)->label = l_start;

        emit(TAC_LABEL)->label = l_end;
        break;
    }
    case NODE_RETURN:
    {
        struct operand v = opd_none();
        if (s->left)
            v = gen_expr(s->left);        /* primero se evalúa la expresión */
        struct tac *r = emit(TAC_RETURN); /* y después se emite el return */
        r->a = v;
        break;
    }
    case NODE_BLOCK:
        gen_block(s);
        break;

    case NODE_VAR_DECL:
    default:
        break;
    }
}

static void gen_block(struct node *b)
{
    if (!b)
        return;
    struct node *items = (b->kind == NODE_BLOCK) ? b->left : b;
    for (struct node *it = items; it; it = it->next)
    {
        gen_stmt(it);
    }
}

static void gen_method(struct node *m)
{
    if (!m)
        return;
    char *name = m->sym ? m->sym->name : (m->id ? m->id : "func");
    nseen = 0;

    struct tac *b = emit(TAC_FUNC_BEGIN);
    b->label = strdup(name);
    b->func = m->sym;
    b->params = m->right; /* parámetros, en orden */

    gen_block(m->left);

    emit(TAC_FUNC_END)->label = strdup(name);
}

struct tac *build_tac(struct node *root)
{
    head = tail = NULL;
    temp_count = 0;
    label_count = 0;
    if (!root)
        return NULL;

    for (struct node *m = root->right; m; m = m->next)
    {
        if (m->kind == NODE_METHOD_DECL && m->left)   /* los extern no tienen cuerpo */
            gen_method(m);
    }
    return head;
}

/* impresión */

static const char *op_to_str(enum op_type op)
{
    switch (op) {
        case OP_ADD:    return "+";
        case OP_SUB:    return "-";
        case OP_MUL:    return "*";
        case OP_DIV:    return "/";
        case OP_MOD:    return "%";
        case OP_LT:     return "<";
        case OP_GT:     return ">";
        case OP_EQ:     return "==";
        case OP_AND:    return "&&";
        case OP_OR:     return "||";
        case OP_NEG:    return "-";
        case OP_NOT:    return "!";
        default:        return "?";
    }
}

static void print_opd(FILE *out, struct operand o)
{
    switch (o.kind)
    {
    case OPD_INT:   fprintf(out, "%d", o.ival);
                    break;
    case OPD_FLOAT: fprintf(out, "%g", o.fval);
                    break;
    case OPD_BOOL:  fprintf(out, "%s", o.ival ? "true" : "false");
                    break;
    case OPD_VAR:   fprintf(out, "%s", o.name);
                    break;
    case OPD_TEMP:  fprintf(out, "t%d", o.temp_id);
                    break;
    default:        break;
    }
}

void print_tac(struct tac *list, FILE *out)
{
    for (struct tac *i = list; i; i = i->next)
    {
        switch (i->op)
        {
        case TAC_ASSIGN:    fprintf(out, "  ");
                            print_opd(out, i->res);
                            fprintf(out, " = ");
                            print_opd(out, i->a);
                            fprintf(out, "\n");
                            break;
        case TAC_BINOP:     fprintf(out, "  ");
                            print_opd(out, i->res);
                            fprintf(out, " = ");
                            print_opd(out, i->a);
                            fprintf(out, " %s ", op_to_str(i->bop));
                            print_opd(out, i->b);
                            fprintf(out, "\n");
                            break;
        case TAC_UNOP:      fprintf(out, "  ");
                            print_opd(out, i->res);
                            fprintf(out, " = %s", op_to_str(i->bop));
                            print_opd(out, i->a);
                            fprintf(out, "\n");
                            break;
        case TAC_LABEL:     fprintf(out, "%s:\n", i->label);
                            break;
        case TAC_GOTO:      fprintf(out, "  goto %s\n", i->label);
                            break;
        case TAC_IFFALSE:   fprintf(out, "  ifFalse ");
                            print_opd(out, i->a);
                            fprintf(out, " goto %s\n", i->label);
                            break;
        case TAC_PARAM:     fprintf(out, "  param ");
                            print_opd(out, i->a);
                            fprintf(out, "\n");
                            break;
        case TAC_CALL:      fprintf(out, "  ");
                            if (i->has_result)
                            {
                                print_opd(out, i->res);
                                fprintf(out, " = ");
                            }
                            fprintf(out, "call %s, %d\n", i->label, i->nargs);
                            break;
        case TAC_RETURN:    fprintf(out, "  return");
                            if (i->a.kind != OPD_NONE)
                            {
                                fprintf(out, " ");
                                print_opd(out, i->a);
                            }
                            fprintf(out, "\n");
                            break;
        case TAC_FUNC_BEGIN:fprintf(out, "func begin %s\n", i->label);
                            break;
        case TAC_FUNC_END:  fprintf(out, "func end %s\n\n", i->label);
                            break;
        }
    }
}

/* Interfaz que ya usa main.c */
void gen_intermediate_code(struct node *root, FILE *out)
{
    if (!root)
        return;
    struct tac *list = build_tac(root);
    print_tac(list, out ? out : stdout);
}