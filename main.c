#include <string.h>
#include "intermediate_code.h"

extern int yyparse();
extern struct node *root;
extern int lexical_errors;
extern int syntax_errors;
extern int semantic_errors;
extern FILE *yyin;

int main(int argc, char **argv) {
    int target_codinter = 0;
    char *input_filename = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-target") == 0 && i + 1 < argc) {
            if (strcmp(argv[i + 1], "codinter") == 0) {
                target_codinter = 1;
            }
            i++;
        } else if (!input_filename) {
            input_filename = argv[i];
        }
    }

    if (input_filename) {
        yyin = fopen(input_filename, "r");
        if (!yyin) {
            perror("No se pudo abrir el archivo");
            return 1;
        }
    }

    init_symbol_table();
    int ok = yyparse();

    if (ok == 0 && lexical_errors == 0 && syntax_errors == 0 && semantic_errors == 0) {
        if (target_codinter) {
            gen_intermediate_code(root, stdout);
            // Si hay archivo de entrada, también se exporta a <nombre>.ci
            if (input_filename) {
                char *base = strrchr(input_filename, '/');
                base = base ? base + 1 : input_filename;
                char ci_name[512];
                snprintf(ci_name, sizeof(ci_name), "%s", base);
                char *dot = strrchr(ci_name, '.');
                if (dot) *dot = '\0';
                strncat(ci_name, ".ci", sizeof(ci_name) - strlen(ci_name) - 1);
                FILE *ci = fopen(ci_name, "w");
                if (ci) {
                    gen_intermediate_code(root, ci);
                    fclose(ci);
                } else {
                    perror("No se pudo crear el archivo .ci");
                }
            }
        } else {
            printf("\n--- Análisis sintáctico exitoso ---\n");
            //printf("\n--- Generando Código Pseudo-Assembly ---\n");
            //gen_code(root);
            //printf("\n--- Evaluando AST (Intérprete) ---\n");
            //eval(root);
        }
    } else {
        printf("\nFallo en el análisis sintáctico.\n");

        if (lexical_errors > 0) {
            printf("Errores léxicos: %d\n", lexical_errors);
        }
        if (syntax_errors > 0) {
            printf("Errores sintácticos: %d\n", syntax_errors);
        }

        if (semantic_errors > 0) {
            printf("Errores semánticos: %d\n", semantic_errors);
        }

        int total = lexical_errors + syntax_errors + semantic_errors;
        printf("Se encontraron %d errores en total.\n", total);
    }

    return 0;
}