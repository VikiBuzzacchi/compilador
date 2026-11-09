// Árbol Sintáctico Abstracto (AST)

#ifndef AST_H
#define AST_H

#include "symbol_table.h"

enum node_kind {
    NODE_PROGRAM,       // left: variables globales, right: métodos
    NODE_VAR_DECL,      // sym
    NODE_METHOD_DECL,   // sym, right: parámetros (NODE_VAR_DECL), left: cuerpo (NODE_BLOCK)
    NODE_BLOCK,         // left: lista de declaraciones y sentencias
    NODE_ASSIGN,        // left: NODE_ID, right: expresión
    NODE_CALL,          // id, sym, left: lista de argumentos
    NODE_IF,            // left: condición, right: then, third: else (NULL si no hay)
    NODE_WHILE,         // left: condición, right: cuerpo
    NODE_RETURN,        // left: expresión (NULL en "return;")
    NODE_BINOP,         // op, left, right
    NODE_UNOP,          // op, left
    NODE_INT,           // value.ival
    NODE_FLOAT,         // value.fval
    NODE_BOOL,          // value.ival (0 = false, 1 = true)
    NODE_ID             // id, sym
};

enum op_type {
    OP_NONE,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_LT, OP_GT, OP_EQ,
    OP_AND, OP_OR,
    OP_NEG, OP_NOT      // unarios: - y !
};

struct node {
    enum node_kind kind;
    int line;               // línea de la sentencia o expresión
    enum op_type op;            // NODE_BINOP / NODE_UNOP
    enum sym_type data_type;    // tipo de la expresión
    union {
        int ival;
        float fval;
    } value;                    // literales
    int is_int_min_mag;         // literal 2147483648 pendiente de resolver como -2147483648
    char *id;                   // NODE_ID 
    struct symbol *sym;         // NULL si el identificador no está declarado
    struct node *left;
    struct node *right;
    struct node *third;         // for else branch of NODE_IF
    struct node *next;          // siguiente elemento en una lista
};

struct node *append_node(struct node *list, struct node *n);

struct node *new_program(struct node *globals, struct node *methods);
struct node *new_var_decl(struct symbol *sym, int line);
struct node *new_method_decl(struct symbol *sym, struct node *params, struct node *body, int line);
struct node *new_block(struct node *items, int line);
struct node *new_assign(struct node *target, struct node *expr, int line);
struct node *new_if(struct node *cond, struct node *then_block, struct node *else_block, int line);
struct node *new_while(struct node *cond, struct node *body, int line);
struct node *new_return(struct node *expr, int line);
struct node *new_binop(enum op_type op, struct node *left, struct node *right, int line);
struct node *new_unop(enum op_type op, struct node *expr, int line);
struct node *new_int(int val, int line);
struct node *new_int_min_mag(int line);
struct node *new_float(float val, int line);
struct node *new_bool(int val, int line);

// Convierte -2147483648 en el literal INT_MIN y reporta los usos no negados de 2147483648.
void resolve_int_min(struct node *n);

// Contador de errores semánticos (se define en ast.c)
extern int semantic_errors;

// Resuelven el nombre con search_symbol al crearse (sym queda en NULL si no existe)
struct node *new_id(char *name, int line);
struct node *new_call(char *name, struct node *args, int line);

// Imprime el árbol indentado
void print_ast(struct node *t);

// Método que se está analizando (lo fija parser.y en method_head); lo usa new_return
extern struct symbol *current_method;

// Una llamada usada como expresión debe devolver valor (regla 5)
struct node *check_call_value(struct node *call);

// Primer nodo que usa float (declaración, parámetro, literal, expresión o llamada); NULL si no hay.
// El generador de código objeto solo soporta int y boolean.
struct node *find_float(struct node *n);

#endif
