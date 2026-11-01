#ifndef OBJECT_CODE_H
#define OBJECT_CODE_H

#include <stdio.h>
#include "tac.h"

/* Etapa 4: traduce la lista TAC a assembly x86-64 (AT&T). */
void gen_object_code(struct tac *list, FILE *out);

#endif