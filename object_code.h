#ifndef OBJECT_CODE_H
#define OBJECT_CODE_H

#include <stdio.h>
#include "tac.h"

/* Etapa 4: traduce la lista TAC a assembly x86-64 (AT&T).
   reuse: los temporales que no están vivos al mismo tiempo comparten slot (-opt tempslots).
   debug: lista en stderr el slot de cada variable y temporal (-debug). */
void gen_object_code(struct tac *list, FILE *out, int reuse, int debug);

#endif