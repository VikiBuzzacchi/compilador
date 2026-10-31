#include <stdio.h>
#include "object_code.h"

void gen_object_code(struct node *root, FILE *out) {
    (void)root;
    if (!out) out = stdout;
    fprintf(out, "    .text\n");
    fprintf(out, "    .globl main\n");
    fprintf(out, "main:\n");
    fprintf(out, "    pushq %%rbp\n");
    fprintf(out, "    movq %%rsp, %%rbp\n");
    fprintf(out, "    movl $0, %%eax\n");
    fprintf(out, "    popq %%rbp\n");
    fprintf(out, "    ret\n");
    fprintf(out, "    .section .note.GNU-stack,\"\",@progbits\n");
}