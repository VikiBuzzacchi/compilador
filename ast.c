#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "symbol_table.h"

int semantic_errors = 0;

// Reporta un nombre usado sin haber sido declarado
static void report_undeclared(char *name, int line) {
    semantic_errors++;
    fprintf(stderr, "Error semántico en la línea %d: '%s' no declarado\n", line, name);
}

static struct node *new_node(enum node_kind kind, int line) {
    struct node *n = calloc(1, sizeof(struct node));
    n->kind = kind;
    n->line = line;
    n->op = OP_NONE;
    n->data_type = TYPE_VOID;
    return n;
}

struct node *append_node(struct node *list, struct node *n) {
    if (!list) return n;
    struct node *last = list;
    while (last->next) last = last->next;
    last->next = n;
    return list;
}

struct node *new_program(struct node *globals, struct node *methods) {
    struct node *n = new_node(NODE_PROGRAM, 1);
    n->left = globals;
    n->right = methods;
    return n;
}

struct node *new_var_decl(struct symbol *sym, int line) {
    struct node *n = new_node(NODE_VAR_DECL, line);
    n->sym = sym;
    if (sym) {
        n->id = sym->name;
        n->data_type = sym->type;
    }
    return n;
}

struct node *new_method_decl(struct symbol *sym, struct node *params, struct node *body, int line) {
    struct node *n = new_node(NODE_METHOD_DECL, line);
    n->sym = sym;
    n->left = body;
    n->right = params;
    if (sym) {
        n->id = sym->name;
        n->data_type = sym->type;
    }
    return n;
}

struct node *new_block(struct node *items, int line) {
    struct node *n = new_node(NODE_BLOCK, line);
    n->left = items;
    return n;
}

struct node *new_assign(struct node *target, struct node *expr, int line) {
    struct node *n = new_node(NODE_ASSIGN, line);
    n->left = target;
    n->right = expr;
    return n;
}

struct node *new_if(struct node *cond, struct node *then_block, struct node *else_block, int line) {
    struct node *n = new_node(NODE_IF, line);
    n->left = cond;
    n->right = then_block;
    n->third = else_block;
    return n;
}

struct node *new_while(struct node *cond, struct node *body, int line) {
    struct node *n = new_node(NODE_WHILE, line);
    n->left = cond;
    n->right = body;
    return n;
}

struct node *new_return(struct node *expr, int line) {
    struct node *n = new_node(NODE_RETURN, line);
    n->left = expr;
    return n;
}

struct node *new_binop(enum op_type op, struct node *left, struct node *right, int line) {
    struct node *n = new_node(NODE_BINOP, line);
    n->op = op;
    n->left = left;
    n->right = right;
    return n;
}

struct node *new_unop(enum op_type op, struct node *expr, int line) {
    struct node *n = new_node(NODE_UNOP, line);
    n->op = op;
    n->left = expr;
    return n;
}

struct node *new_int(int val, int line) {
    struct node *n = new_node(NODE_INT, line);
    n->value.ival = val;
    n->data_type = TYPE_INT;
    return n;
}

struct node *new_float(float val, int line) {
    struct node *n = new_node(NODE_FLOAT, line);
    n->value.fval = val;
    n->data_type = TYPE_FLOAT;
    return n;
}

struct node *new_bool(int val, int line) {
    struct node *n = new_node(NODE_BOOL, line);
    n->value.ival = val ? 1 : 0;
    n->data_type = TYPE_BOOL;
    return n;
}

struct node *new_id(char *name, int line) {
    struct node *n = new_node(NODE_ID, line);
    n->id = strdup(name);
    n->sym = search_symbol(name);
    if (n->sym) n->data_type = n->sym->type;
    else report_undeclared(name, line);
    return n;
}

struct node *new_call(char *name, struct node *args, int line) {
    struct node *n = new_node(NODE_CALL, line);
    n->id = strdup(name);
    n->sym = search_symbol(name);
    n->left = args;
    //if (n->sym) n->data_type = n->sym->type;
    if (!n->sym)
        report_undeclared(name, line);
    else if (n->sym->kind != SYM_METHOD) {
        semantic_errors++;
        fprintf(stderr, "Error semántico en la línea %d: '%s' no es un método\n", line, name);
    }
    else
        n->data_type = n->sym->type;
    //if (n->sym && n->sym->kind != SYM_METHOD) {
    //semantic_errors++;
    //fprintf(stderr, "Error semántico en la línea %d: '%s' no es un método\n", line, name);
    //}
    //else report_undeclared(name, line);
    return n;
}


void print_ast(struct node *t) {
    (void) t;
}
