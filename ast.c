#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "ast.h"
#include "symbol_table.h"
#include <stdarg.h>

int semantic_errors = 0;

struct symbol *current_method = NULL;

static const char *type_name(enum sym_type t) {
    switch (t) {
        case TYPE_INT:   return "int";
        case TYPE_BOOL:  return "boolean";
        case TYPE_FLOAT: return "float";
        case TYPE_VOID:  return "void";
        default:         return "?";
    }
}

static int is_num(enum sym_type t) { return t == TYPE_INT || t == TYPE_FLOAT; }

// Compatibles: mismo tipo, o int/float entre sí (coerción, regla 14)
static int compatible(enum sym_type a, enum sym_type b) {
    return a == b || (is_num(a) && is_num(b));
}

static const char *op_name(enum op_type op) {
    switch (op) {
        case OP_ADD: return "+";  case OP_SUB: return "-";
        case OP_MUL: return "*";  case OP_DIV: return "/";
        case OP_MOD: return "%";  case OP_LT:  return "<";
        case OP_GT:  return ">";  case OP_EQ:  return "==";
        case OP_AND: return "&&"; case OP_OR:  return "||";
        case OP_NEG: return "-";  case OP_NOT: return "!";
        default:     return "?";
    }
}

static void sem_error(int line, const char *fmt, ...) {
    va_list ap;
    semantic_errors++;
    fprintf(stderr, "Error semántico en la línea %d: ", line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

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

// Todo camino de la lista de sentencias termina en return
static int list_returns(struct node *list);

static int block_returns(struct node *b) {
    if (!b) return 0;
    return list_returns(b->kind == NODE_BLOCK ? b->left : b);
}

static int list_returns(struct node *list) {
    for (struct node *s = list; s; s = s->next) {
        switch (s->kind) {
        case NODE_RETURN:
            return 1;
        case NODE_BLOCK:
            if (block_returns(s)) return 1;
            break;
        case NODE_IF:
            if (s->third && block_returns(s->right) && block_returns(s->third)) return 1;
            break;
        default:
            break;
        }
    }
    return 0;
}

struct node *new_method_decl(struct symbol *sym, struct node *params, struct node *body, int line) {
    struct node *n = new_node(NODE_METHOD_DECL, line);
    n->sym = sym;
    n->left = body;
    n->right = params;
    if (sym) {
        n->id = sym->name;
        n->data_type = sym->type;
        // un método con valor de retorno no puede alcanzar el fin del cuerpo
        if (sym->type != TYPE_VOID && body && !sym->is_extern && !block_returns(body))
            sem_error(line, "el método '%s' debe retornar un valor en todos los caminos",
                      sym->name);
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
    if (target && expr) {
        enum sym_type tt = target->data_type, et = expr->data_type;
        n->data_type = tt;
        // La ⟨location⟩ y la ⟨expr⟩ en una asignaci ́on, ⟨location⟩ = ⟨expr⟩, deben tener el mismo tipo.
        if (tt != TYPE_ERROR && et != TYPE_ERROR && !compatible(tt, et))
            sem_error(target->line, "no se puede asignar %s a '%s' de tipo %s",
                      type_name(et), target->id, type_name(tt));
    }
    return n;
}

// regla 9: la condición de if y while debe ser boolean
static void check_cond(struct node *cond, const char *what) {
    if (cond && cond->data_type != TYPE_BOOL && cond->data_type != TYPE_ERROR)
        sem_error(cond->line, "la condición del %s debe ser boolean y es %s",
                  what, type_name(cond->data_type));
}

struct node *new_if(struct node *cond, struct node *then_block, struct node *else_block, int line) {
    struct node *n = new_node(NODE_IF, line);
    n->left = cond;
    n->right = then_block;
    n->third = else_block;
    check_cond(cond, "if");
    return n;
}

struct node *new_while(struct node *cond, struct node *body, int line) {
    struct node *n = new_node(NODE_WHILE, line);
    n->left = cond;
    n->right = body;
    check_cond(cond, "while");
    return n;
}

struct node *new_return(struct node *expr, int line) {
    struct node *n = new_node(NODE_RETURN, line);
    n->left = expr;
    if (current_method) {
        enum sym_type rt = current_method->type;
        if (rt == TYPE_VOID && expr)                       // en metodo void la sentencia return no puede tener asociada ninguna expresión
            sem_error(expr->line, "return con expresión en el método void '%s'",
                      current_method->name);
        else if (rt != TYPE_VOID && !expr)                 // si es un método que retorna algo, la sentencia return debe tener asociada una expresión
            sem_error(line, "return sin expresión en el método '%s', que retorna %s",
                      current_method->name, type_name(rt));
        else if (rt != TYPE_VOID && expr && expr->data_type != TYPE_ERROR
                 && !compatible(rt, expr->data_type))      // La expresion en una sentencia return debe ser igual al tipo de retorno declarado para el metodo.
            sem_error(expr->line, "return de tipo %s en el método '%s', que retorna %s",
                      type_name(expr->data_type), current_method->name, type_name(rt));
    }
    return n;
}

struct node *new_binop(enum op_type op, struct node *left, struct node *right, int line) {
    struct node *n = new_node(NODE_BINOP, line);
    n->op = op;
    n->left = left;
    n->right = right;

    enum sym_type lt = left  ? left->data_type  : TYPE_ERROR;
    enum sym_type rt = right ? right->data_type : TYPE_ERROR;
    int bad = (lt == TYPE_ERROR || rt == TYPE_ERROR); 
    switch (op) {
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        n->data_type = TYPE_ERROR;
        if (bad) break;
        if (!is_num(lt) || !is_num(rt))                     // operandos de +, -, *, / y % deben ser int o float 
            sem_error(line, "los operandos de '%s' deben ser int o float (son %s y %s)",
                      op_name(op), type_name(lt), type_name(rt));
        else
            n->data_type = (lt == TYPE_FLOAT || rt == TYPE_FLOAT) ? TYPE_FLOAT : TYPE_INT;
        break;
    case OP_LT: case OP_GT:
        n->data_type = TYPE_BOOL;
        if (!bad && (!is_num(lt) || !is_num(rt)))           // operandos de < y > deben ser int o float
            sem_error(line, "los operandos de '%s' deben ser int o float (son %s y %s)",
                      op_name(op), type_name(lt), type_name(rt));
        break;
    case OP_EQ:
        n->data_type = TYPE_BOOL;
        if (!bad && !compatible(lt, rt))                    // los operandos de == deben tener el mismo tipo (o int/float entre sí)
            sem_error(line, "los operandos de '==' deben tener el mismo tipo (son %s y %s)",
                      type_name(lt), type_name(rt));
        break;
    case OP_AND: case OP_OR:
        n->data_type = TYPE_BOOL;
        if (!bad && (lt != TYPE_BOOL || rt != TYPE_BOOL))   // los operandos de && y || deben ser boolean
            sem_error(line, "los operandos de '%s' deben ser boolean (son %s y %s)",
                      op_name(op), type_name(lt), type_name(rt));
        break;
    default:
        break;
    }
    return n;
}

struct node *new_unop(enum op_type op, struct node *expr, int line) {
    struct node *n = new_node(NODE_UNOP, line);
    n->op = op;
    n->left = expr;
    enum sym_type t = expr ? expr->data_type : TYPE_ERROR;

    if (op == OP_NOT) {
        n->data_type = TYPE_BOOL;
        if (t != TYPE_ERROR && t != TYPE_BOOL)              // operando de ! debe ser boolean
            sem_error(line, "el operando de '!' debe ser boolean (es %s)", type_name(t));
    } else {
        n->data_type = TYPE_ERROR;
        if (t != TYPE_ERROR) {
            if (is_num(t)) n->data_type = t;
            else sem_error(line, "el operando de '-' debe ser int o float (es %s)", type_name(t));
        }
    }
    return n;
}

struct node *new_int(int val, int line) {
    struct node *n = new_node(NODE_INT, line);
    n->value.ival = val;
    n->data_type = TYPE_INT;
    return n;
}

/* 2147483648 no entra en un int: se marca y solo se acepta como operando de '-' unario. */
struct node *new_int_min_mag(int line) {
    struct node *n = new_int(INT_MIN, line);
    n->is_int_min_mag = 1;
    return n;
}

/* Recorre el AST: -2147483648 se convierte en el literal INT_MIN. Cualquier otro uso
   de 2147483648 (suelto, en una resta binaria, etc.) es un error semántico. */
void resolve_int_min(struct node *n) {
    for (; n; n = n->next) {
        if (n->kind == NODE_UNOP && n->op == OP_NEG &&
            n->left && n->left->kind == NODE_INT && n->left->is_int_min_mag) {
            n->kind = NODE_INT;
            n->op = OP_NONE;
            n->value.ival = INT_MIN;
            n->data_type = TYPE_INT;
            n->is_int_min_mag = 0;
            n->left = n->right = n->third = NULL;
        } else {
            resolve_int_min(n->left);
            resolve_int_min(n->right);
            resolve_int_min(n->third);
            if (n->kind == NODE_INT && n->is_int_min_mag)
                sem_error(n->line, "entero '2147483648' fuera de rango (solo se admite como -2147483648)");
        }
    }
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
    n->data_type = TYPE_ERROR;                  
    if (!n->sym)
        report_undeclared(name, line);
    else if (n->sym->kind == SYM_METHOD) {
        semantic_errors++;
        fprintf(stderr, "Error semántico en la línea %d: '%s' no es una variable\n", line, name);
    }
    else
        n->data_type = n->sym->type;
    return n;
}

struct node *new_call(char *name, struct node *args, int line) {
    struct node *n = new_node(NODE_CALL, line);
    n->id = strdup(name);
    n->sym = search_symbol(name);
    n->left = args;
    n->data_type = TYPE_ERROR;
    if (!n->sym)
        report_undeclared(name, line);
    else if (n->sym->kind != SYM_METHOD) {
        semantic_errors++;
        fprintf(stderr, "Error semántico en la línea %d: '%s' no es un método\n", line, name);
    }
    else {
        n->data_type = n->sym->type;
        int nargs = 0;
        for (struct node *a = args; a; a = a->next) nargs++;
        if (nargs != n->sym->nparams) {
            semantic_errors++;
            fprintf(stderr, "Error semántico en la línea %d: '%s' espera %d argumentos y recibe %d\n",
                    line, name, n->sym->nparams, nargs);
        } else if (n->sym->param_types) {
            // tipo de cada argumento debe ser compatible
            int i = 0;
            for (struct node *a = args; a; a = a->next, i++) {
                if (a->data_type != TYPE_ERROR && !compatible(n->sym->param_types[i], a->data_type))
                    sem_error(line, "argumento %d de '%s': se esperaba %s y se recibe %s",
                              i + 1, name, type_name(n->sym->param_types[i]),
                              type_name(a->data_type));
            }
        }
    }
    return n;
}

// un método void no puede usarse como expresión
struct node *check_call_value(struct node *call) {
    if (call && call->data_type == TYPE_VOID) {
        sem_error(call->line, "el método '%s' es void y no puede usarse como expresión", call->id);
        call->data_type = TYPE_ERROR;
    }
    return call;
}


struct node *find_float(struct node *n) {
    for (; n; n = n->next) {
        if (n->data_type == TYPE_FLOAT || n->kind == NODE_FLOAT)
            return n;
        struct node *f;
        if ((f = find_float(n->left)) || (f = find_float(n->right)) || (f = find_float(n->third)))
            return f;
    }
    return NULL;
}

/* ---------- Impresión del AST ---------- */

static void print_node(struct node *n, int depth, const char *role, FILE *out);

/* Imprime cada elemento de una lista enlazada por 'next' con el mismo nivel y rol */
static void print_list(struct node *list, int depth, const char *role, FILE *out) {
    for (struct node *n = list; n; n = n->next)
        print_node(n, depth, role, out);
}

/* Una línea por nodo: sangría, rol en el padre (cond, then, ...), clase y datos.
   Los hijos van en las líneas siguientes con un nivel más de sangría. */
static void print_node(struct node *n, int depth, const char *role, FILE *out) {
    fprintf(out, "%*s", depth * 2, "");
    if (role) fprintf(out, "%s: ", role);

    switch (n->kind) {
    case NODE_PROGRAM:
        fprintf(out, "PROGRAM\n");
        print_list(n->left, depth + 1, "global", out);
        print_list(n->right, depth + 1, NULL, out);
        break;
    case NODE_VAR_DECL:
        fprintf(out, "VAR_DECL %s : %s\n", n->id, type_name(n->data_type));
        break;
    case NODE_METHOD_DECL:
        fprintf(out, "METHOD_DECL %s : %s%s\n", n->id, type_name(n->data_type),
                n->left ? "" : " extern");
        print_list(n->right, depth + 1, "param", out);
        if (n->left) print_node(n->left, depth + 1, "body", out);
        break;
    case NODE_BLOCK:
        fprintf(out, "BLOCK\n");
        print_list(n->left, depth + 1, NULL, out);
        break;
    case NODE_ASSIGN:
        fprintf(out, "ASSIGN\n");
        print_node(n->left, depth + 1, NULL, out);
        print_node(n->right, depth + 1, NULL, out);
        break;
    case NODE_CALL:
        fprintf(out, "CALL %s : %s\n", n->id, type_name(n->data_type));
        print_list(n->left, depth + 1, "arg", out);
        break;
    case NODE_IF:
        fprintf(out, "IF\n");
        print_node(n->left, depth + 1, "cond", out);
        print_node(n->right, depth + 1, "then", out);
        if (n->third) print_node(n->third, depth + 1, "else", out);
        break;
    case NODE_WHILE:
        fprintf(out, "WHILE\n");
        print_node(n->left, depth + 1, "cond", out);
        print_node(n->right, depth + 1, "body", out);
        break;
    case NODE_RETURN:
        fprintf(out, "RETURN\n");
        if (n->left) print_node(n->left, depth + 1, NULL, out);
        break;
    case NODE_BINOP:
        fprintf(out, "BINOP %s : %s\n", op_name(n->op), type_name(n->data_type));
        print_node(n->left, depth + 1, NULL, out);
        print_node(n->right, depth + 1, NULL, out);
        break;
    case NODE_UNOP:
        fprintf(out, "UNOP %s : %s\n", op_name(n->op), type_name(n->data_type));
        print_node(n->left, depth + 1, NULL, out);
        break;
    case NODE_INT:
        fprintf(out, "INT %d\n", n->value.ival);
        break;
    case NODE_FLOAT:
        fprintf(out, "FLOAT %g\n", n->value.fval);
        break;
    case NODE_BOOL:
        fprintf(out, "BOOL %s\n", n->value.ival ? "true" : "false");
        break;
    case NODE_ID:
        fprintf(out, "ID %s : %s\n", n->id, type_name(n->data_type));
        break;
    }
}

void print_ast(struct node *t, FILE *out) {
    if (t) print_node(t, 0, NULL, out ? out : stdout);
}
