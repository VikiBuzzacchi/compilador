#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "symbol_table.h"

struct node *create_node(enum node_type type, struct node *left, struct node *right) {
    struct node *n = malloc(sizeof(struct node));
    n->type = type;
    n->left = left;
    n->right = right;
    n->id = NULL;
    n->sym = NULL;
    n->val = 0;
    return n;
}

struct node *create_var_node(char *id) {
    struct node *n = malloc(sizeof(struct node));
    n->type = NODE_VAR;
    n->id = strdup(id);
    n->sym = search_symbol(id);
    n->left = NULL;
    n->right = NULL;
    return n;
}

struct node *create_const_node(int val) {
    struct node *n = malloc(sizeof(struct node));
    n->type = NODE_INT;
    n->val = val;
    n->sym = NULL;
    n->left = NULL;
    n->right = NULL;
    return n;
}

int eval(struct node *t) {
    if (!t) return 0;
    switch(t->type) {
        case NODE_INT: return t->val;
        case NODE_VAR: {
            return t->sym ? t->sym->value : 0;
        }
        case NODE_NEG: return -eval(t->left);
        case NODE_ADD: return eval(t->left) + eval(t->right);
        case NODE_SUB: return eval(t->left) - eval(t->right);
        case NODE_MUL: return eval(t->left) * eval(t->right);
        case NODE_ASG: {
            int val = eval(t->right);
            if (t->left->sym) t->left->sym->value = val;
            return val;
        }
        case NODE_SEQ:
            eval(t->left);
            return eval(t->right);
        case NODE_RET: {
            int ret_val = t->left ? eval(t->left) : 0;
            printf(">> Return ejecutado con valor: %d\n", ret_val);
            return ret_val;
        }
        default: return 0;
    }
}

void gen_code(struct node *t) {
    if (!t) return;
    if (t->type == NODE_INT) {
        printf("PUSHC %d\n", t->val);
    } else if (t->type == NODE_VAR) {
        printf("PUSHA %s\n", t->id);
        printf("LOAD\n");
    } else if (t->type == NODE_ASG) {
        printf("PUSHA %s\n", t->left->id);
        gen_code(t->right);
        printf("STORE\n");
    } else if (t->type == NODE_SEQ) {
        gen_code(t->left);
        if (t->left && t->left->type != NODE_ASG && t->left->type != NODE_RET) printf("POP\n");
        gen_code(t->right);
    } else if (t->type == NODE_RET) {
        if (t->left) gen_code(t->left);
        printf("RET\n");
    } else {
        gen_code(t->left);
        if (t->right) gen_code(t->right);
        switch(t->type) {
            case NODE_ADD: printf("ADD\n"); break;
            case NODE_SUB: printf("NEG\nADD\n"); break;
            case NODE_MUL: printf("MUL\n"); break;
            case NODE_NEG: printf("NEG\n"); break;
            default: break;
        }
    }
}