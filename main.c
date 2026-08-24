#include <stdio.h>
#include "ast.h"

extern int yyparse();
extern struct node *root;
extern int lexical_errors;
extern int syntax_errors;

int main() {
    int ok = yyparse();

    if (ok == 0 && lexical_errors == 0 && syntax_errors == 0) {
        printf("\n--- Análisis sintáctico exitoso ---\n");
        printf("\n--- Generando Código Pseudo-Assembly ---\n");
        gen_code(root);
        printf("\n--- Evaluando AST (Intérprete) ---\n");
        eval(root);
    } else {
        printf("\nFallo en el análisis sintáctico.\n");

        if (lexical_errors > 0) {
            printf("Errores léxicos: %d\n", lexical_errors);
        }
        if (syntax_errors > 0) {
            printf("Errores sintácticos: %d\n", syntax_errors);
        }

        int total = lexical_errors + syntax_errors;
        printf("Se encontraron %d errores en total.\n", total);
    }

    return 0;
}