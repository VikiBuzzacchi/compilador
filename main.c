#include <stdio.h>
#include "ast.h"

extern int yyparse();
extern struct node *root;
extern int errores_lexicos;
extern int errores_sintacticos;

int main() {
    int ok = yyparse();

    if (ok == 0 && errores_lexicos == 0 && errores_sintacticos == 0) {
        printf("\n--- Análisis sintáctico exitoso ---\n");
        printf("\n--- Generando Código Pseudo-Assembly ---\n");
        gen_code(root);
        printf("\n--- Evaluando AST (Intérprete) ---\n");
        eval(root);
    } else {
        printf("\nFallo en el análisis sintáctico.\n");

        if (errores_lexicos > 0) {
            printf("Errores léxicos: %d\n", errores_lexicos);
        }
        if (errores_sintacticos > 0) {
            printf("Errores sintácticos: %d\n", errores_sintacticos);
        }

        int total = errores_lexicos + errores_sintacticos;
        printf("Se encontraron %d errores en total.\n", total);
    }

    return 0;
}