/*
 * Intérprete del preproyecto
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- ast.h (preproyecto) ---------- */

enum node_type { NODE_INT, NODE_VAR, NODE_ADD, NODE_SUB, NODE_MUL, NODE_NEG, NODE_ASG, NODE_SEQ, NODE_RET };

struct node {
    enum node_type type;
    int val;
    char *id;
    struct node *left;
    struct node *right;
};

/* ---------- symbol_table.c (preproyecto) ---------- */

#define TYPE_INT 1
#define TYPE_BOOL 2
#define MAX_SYMBOLS 100

struct symbol {
    char *name;
    int type;
    int value;
};

struct symbol sym_table[MAX_SYMBOLS];
int sym_count = 0;

void install_symbol(char *name, int type) {
    for (int i = 0; i < sym_count; i++) {
        if (strcmp(sym_table[i].name, name) == 0) {
            printf("Error semántico: Variable '%s' ya declarada.\n", name);
            return;
        }
    }
    if (sym_count < MAX_SYMBOLS) {
        sym_table[sym_count].name = strdup(name);
        sym_table[sym_count].type = type;
        sym_table[sym_count].value = 0;
        sym_count++;
    } else {
        printf("Error: Tabla de símbolos llena.\n");
    }
}

int get_symbol_index(char *name) {
    for (int i = 0; i < sym_count; i++) {
        if (strcmp(sym_table[i].name, name) == 0) {
            return i;
        }
    }
    printf("Error semántico: Variable '%s' no declarada.\n", name);
    return -1;
}

int get_value_from_table(char *name) {
    int idx = get_symbol_index(name);
    if (idx != -1) return sym_table[idx].value;
    return 0;
}

void update_symbol_value(char *name, int value) {
    int idx = get_symbol_index(name);
    if (idx != -1) sym_table[idx].value = value;
}

/* ---------- ast.c (preproyecto): intérprete ---------- */

int eval(struct node *t) {
    if (!t) return 0;
    switch(t->type) {
        case NODE_INT: return t->val;
        case NODE_VAR: return get_value_from_table(t->id);
        case NODE_NEG: return -eval(t->left);
        case NODE_ADD: return eval(t->left) + eval(t->right);
        case NODE_SUB: return eval(t->left) - eval(t->right);
        case NODE_MUL: return eval(t->left) * eval(t->right);
        case NODE_ASG: {
            int val = eval(t->right);
            update_symbol_value(t->left->id, val);
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

/* En el preproyecto, main.c lo invocaba así después de yyparse():
 *
 *     printf("\n--- Evaluando AST (Intérprete) ---\n");
 *     eval(root);
 */
