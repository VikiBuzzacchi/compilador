#include <stdio.h>
#include "ast.h"

extern int yyparse();
extern struct node *root;
extern int lexical_errors;
extern int syntax_errors;
extern FILE *yyin;

int main(int argc, char **argv) {
     if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            perror("No se pudo abrir el archivo");
            return 1;
        }
    }

    int ok = yyparse();

    if (ok == 0 && lexical_errors == 0 && syntax_errors == 0) {
        printf("\n--- Análisis sintáctico exitoso ---\n");
        //printf("\n--- Generando Código Pseudo-Assembly ---\n");
        //gen_code(root);
        //printf("\n--- Evaluando AST (Intérprete) ---\n");
        //eval(root);
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