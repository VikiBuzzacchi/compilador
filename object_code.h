#ifndef OBJECT_CODE_H
#define OBJECT_CODE_H

#include <stdio.h>
#include "ast.h"

/* Genera assembly x86-64 (AT&T).  */
void gen_object_code(struct node *root, FILE *out);

#endif