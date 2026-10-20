#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "intermediate_code.h"
#include "ast.h"
#include "symbol_table.h"

static int temp_count = 0;
static int label_count = 0;
static FILE *tac_out = NULL;

static char *new_temp(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "t%d", temp_count++);
    return strdup(buf);
}

static char *new_label(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "L%d", label_count++);
    return strdup(buf);
}

// Nombre de una variable en el TAC. Dos símbolos distintos con el mismo nombre
// (ámbitos anidados) se distinguen con sufijo: x, x_1, x_2...
#define MAX_VARS 1024
static struct symbol *seen_syms[MAX_VARS];
static char *seen_names[MAX_VARS];
static int nseen = 0;

static char *var_name(struct node *n) {
    if (!n->sym) return n->id;
    int dup = 0;
    for (int i = 0; i < nseen; i++) {
        if (seen_syms[i] == n->sym) return seen_names[i];
        if (strcmp(seen_syms[i]->name, n->sym->name) == 0) dup++;
    }
    if (nseen >= MAX_VARS) return n->id;
    char buf[256];
    if (dup) snprintf(buf, sizeof(buf), "%s_%d", n->sym->name, dup);
    else snprintf(buf, sizeof(buf), "%s", n->sym->name);
    seen_syms[nseen] = n->sym;
    seen_names[nseen] = strdup(buf);
    return seen_names[nseen++];
}

static char *gen_expr(struct node *e);
static void gen_stmt(struct node *s);
static void gen_block(struct node *b);

static const char *op_to_str(enum op_type op) {
    switch (op) {
        case OP_ADD: return "+";
        case OP_SUB: return "-";
        case OP_MUL: return "*";
        case OP_DIV: return "/";
        case OP_MOD: return "%";
        case OP_LT:  return "<";
        case OP_GT:  return ">";
        case OP_EQ:  return "==";
        case OP_AND: return "&&";
        case OP_OR:  return "||";
        case OP_NEG: return "-";
        case OP_NOT: return "!";
        default:     return "?";
    }
}

static char *gen_expr(struct node *e) {
    if (!e) return NULL;

    switch (e->kind) {
        case NODE_INT: {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", e->value.ival);
            return strdup(buf);
        }
        case NODE_FLOAT: {
            char buf[32];
            snprintf(buf, sizeof(buf), "%g", e->value.fval);
            return strdup(buf);
        }
        case NODE_BOOL: {
            return strdup(e->value.ival ? "true" : "false");
        }
        case NODE_ID: {
            return strdup(var_name(e));
        }
        case NODE_BINOP: {
            char *left = gen_expr(e->left);
            char *right = gen_expr(e->right);
            char *t = new_temp();
            fprintf(tac_out, "  %s = %s %s %s\n", t, left, op_to_str(e->op), right);
            free(left);
            free(right);
            return t;
        }
        case NODE_UNOP: {
            char *operand = gen_expr(e->left);
            char *t = new_temp();
            fprintf(tac_out, "  %s = %s%s\n", t, op_to_str(e->op), operand);
            free(operand);
            return t;
        }
        case NODE_CALL: {
            // Evaluar argumentos y recopilar temporales / identificadores
            int nargs = 0;
            char *arg_addrs[128];
            for (struct node *arg = e->left; arg && nargs < 128; arg = arg->next) {
                arg_addrs[nargs++] = gen_expr(arg);
            }
            // Emitir param en orden
            for (int i = 0; i < nargs; i++) {
                fprintf(tac_out, "  param %s\n", arg_addrs[i]);
                free(arg_addrs[i]);
            }
            if (e->data_type != TYPE_VOID) {
                char *t = new_temp();
                fprintf(tac_out, "  %s = call %s, %d\n", t, e->id, nargs);
                return t;
            } else {
                fprintf(tac_out, "  call %s, %d\n", e->id, nargs);
                return NULL;
            }
        }
        default:
            return NULL;
    }
}

static void gen_stmt(struct node *s) {
    if (!s) return;

    switch (s->kind) {
        case NODE_ASSIGN: {
            char *val = gen_expr(s->right);
            char *target = s->left ? var_name(s->left) : "unknown";
            fprintf(tac_out, "  %s = %s\n", target, val);
            free(val);
            break;
        }
        case NODE_CALL: {
            char *res = gen_expr(s);
            if (res) free(res);
            break;
        }
        case NODE_IF: {
            char *cond = gen_expr(s->left);
            char *l_end = new_label();
            if (s->third) {
                // if-then-else
                char *l_else = new_label();
                fprintf(tac_out, "  ifFalse %s goto %s\n", cond, l_else);
                free(cond);

                gen_block(s->right);
                fprintf(tac_out, "  goto %s\n", l_end);

                fprintf(tac_out, "%s:\n", l_else);
                gen_block(s->third);

                fprintf(tac_out, "%s:\n", l_end);
                free(l_else);
            } else {
                // solo if-then
                fprintf(tac_out, "  ifFalse %s goto %s\n", cond, l_end);
                free(cond);

                gen_block(s->right);

                fprintf(tac_out, "%s:\n", l_end);
            }
            free(l_end);
            break;
        }
        case NODE_WHILE: {
            char *l_start = new_label();
            char *l_end = new_label();

            fprintf(tac_out, "%s:\n", l_start);
            char *cond = gen_expr(s->left);
            fprintf(tac_out, "  ifFalse %s goto %s\n", cond, l_end);
            free(cond);

            gen_block(s->right);
            fprintf(tac_out, "  goto %s\n", l_start);

            fprintf(tac_out, "%s:\n", l_end);
            free(l_start);
            free(l_end);
            break;
        }
        case NODE_RETURN: {
            if (s->left) {
                char *ret_val = gen_expr(s->left);
                fprintf(tac_out, "  return %s\n", ret_val);
                free(ret_val);
            } else {
                fprintf(tac_out, "  return\n");
            }
            break;
        }
        case NODE_BLOCK: {
            gen_block(s);
            break;
        }
        case NODE_VAR_DECL:
            // Las declaraciones locales no emiten código ejecutable en TAC básico
            break;
        default:
            break;
    }
}

static void gen_block(struct node *b) {
    if (!b) return;
    struct node *items = (b->kind == NODE_BLOCK) ? b->left : b;
    for (struct node *it = items; it; it = it->next) {
        gen_stmt(it);
    }
}

static void gen_method(struct node *m) {
    if (!m) return;
    char *name = m->sym ? m->sym->name : (m->id ? m->id : "func");
    fprintf(tac_out, "func begin %s\n", name);
    nseen = 0;
    gen_block(m->left);
    fprintf(tac_out, "func end %s\n\n", name);
}

void gen_intermediate_code(struct node *root, FILE *out) {
    if (!root) return;
    tac_out = out ? out : stdout;
    temp_count = 0;
    label_count = 0;

    // Métodos
    for (struct node *m = root->right; m; m = m->next) {
        if (m->kind == NODE_METHOD_DECL) {
            gen_method(m);
        }
    }
}
