#ifndef INTERMEDIATE_CODE_H
#define INTERMEDIATE_CODE_H

#include <stdio.h>
#include "ast.h"

// Genera Código de Tres Direcciones (TAC) a partir del AST
void gen_intermediate_code(struct node *root, FILE *out);

#endif
