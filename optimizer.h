// Optimizador: optimizaciones opcionales, activadas con -opt
// Por defecto (sin -opt) no se realiza ninguna.

#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "ast.h"
#include "tac.h"

// Optimizaciones activas (1 = activada)
struct opt_flags {
    int constfold;      // plegado de constantes (AST)
    int constprop;      // propagación de constantes (TAC)
    int deadcode;       // eliminación de código inalcanzable (TAC)
    int tempslots;      // reutilización de slots de temporales (código objeto)
};

// Activa las optimizaciones de una lista separada por comas ("all" = todas).
// Devuelve 0 si algún nombre no existe (y lo reporta), 1 si la lista es válida.
int parse_opt_list(const char *list, struct opt_flags *opts);

// 1 si hay al menos una optimización activa
int any_opt(const struct opt_flags *opts);

// Pasadas sobre el AST (después del análisis semántico)
void optimize_ast(struct node *root, const struct opt_flags *opts);

// Pasadas sobre la lista TAC; devuelven la lista resultante
struct tac *optimize_tac(struct tac *code, const struct opt_flags *opts);

#endif
