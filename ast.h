// Árbol Sintáctico Abstracto (AST)

#ifndef AST_H
#define AST_H

enum node_type { NODE_INT, NODE_VAR, NODE_ADD, NODE_SUB, NODE_MUL, NODE_NEG, NODE_ASG, NODE_SEQ, NODE_RET };

struct node {
    enum node_type type;
    int val;
    char *id;
    struct node *left;
    struct node *right;
};

struct node *create_node(enum node_type type, struct node *left, struct node *right);
struct node *create_var_node(char *id);
struct node *create_const_node(int val);
int eval(struct node *t);
void gen_code(struct node *t);

#endif