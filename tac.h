#ifndef TAC_H
#define TAC_H

#include <stdio.h>
#include "ast.h"

enum opd_kind { OPD_NONE, OPD_INT, OPD_BOOL, OPD_FLOAT, OPD_VAR, OPD_TEMP };

struct operand {
    enum opd_kind kind;
    int ival;               /* OPD_INT / OPD_BOOL */
    float fval;             /* OPD_FLOAT */
    int temp_id;            /* OPD_TEMP: número de tN */
    struct symbol *sym;     /* OPD_VAR: símbolo (clave única de la variable) */
    char *name;             /* OPD_VAR: nombre para imprimir (x, x_1...) */
};

enum tac_op {
    TAC_ASSIGN,      /* res = a            */
    TAC_BINOP,       /* res = a bop b      */
    TAC_UNOP,        /* res = bop a        */
    TAC_LABEL,       /* label:             */
    TAC_GOTO,        /* goto label         */
    TAC_IFFALSE,     /* ifFalse a goto label */
    TAC_PARAM,       /* param a            */
    TAC_CALL,        /* [res =] call label, nargs */
    TAC_RETURN,      /* return [a]         */
    TAC_FUNC_BEGIN,  /* func begin label   */
    TAC_FUNC_END     /* func end label     */
};

struct tac {
    enum tac_op op;
    enum op_type bop;            /* BINOP / UNOP */
    struct operand a, b, res;
    char *label;                 /* LABEL, GOTO, IFFALSE, CALL, FUNC_* (nombre) */
    int nargs;                   /* CALL */
    int has_result;              /* CALL: 1 si devuelve valor */
    struct symbol *func;         /* FUNC_BEGIN y CALL: símbolo del método */
    struct node *params;         /* FUNC_BEGIN: lista de parámetros (NODE_VAR_DECL) */
    struct tac *next;
};

/* Construye la lista de instrucciones TAC de todo el programa */
struct tac *build_tac(struct node *root);

/* Imprime la lista con el mismo formato de la etapa 3 */
void print_tac(struct tac *list, FILE *out);

#endif