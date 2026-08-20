#include <stdio.h>
#include "ast.h"

extern int yyparse();
extern struct node *root;

int main() {
    if (yyparse() == 0) {
        printf("\n--- Análisis sintáctico exitoso ---\n");
        printf("\n--- Generando Código Pseudo-Assembly ---\n");
        gen_code(root);
        printf("\n--- Evaluando AST (Intérprete) ---\n");
        eval(root);
    } else {
        printf("\nFallo en el análisis sintáctico.\n");
    }
    return 0;
}