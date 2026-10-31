#include <string.h>
#include "intermediate_code.h"
#include <stdio.h>
#include <stdlib.h>
#include "ast.h"
#include "object_code.h"

extern int yyparse();
extern struct node *root;
extern int lexical_errors;
extern int syntax_errors;
extern int semantic_errors;
extern FILE *yyin;


/* Arma "<base sin directorio ni extensión><ext>" a partir del archivo de entrada */
static void make_out_name(const char *input, const char *ext, char *dst, size_t n) {
    const char *base = strrchr(input, '/');
    base = base ? base + 1 : input;
    snprintf(dst, n, "%s", base);
    char *dot = strrchr(dst, '.');
    if (dot) *dot = '\0';
    strncat(dst, ext, n - strlen(dst) - 1);
}
/*
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
*/

int main(int argc, char **argv) {
    int target_codinter = 0;
    int target_assembly = 0;
    int debug = 0;
    char *input_filename = NULL;
    char *exe_name = NULL;          /* valor de -o */

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-target") == 0 && i + 1 < argc) {
            if (strcmp(argv[i + 1], "codinter") == 0) target_codinter = 1;
            else if (strcmp(argv[i + 1], "assembly") == 0) target_assembly = 1;
            i++;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            exe_name = argv[++i];
        } else if (strcmp(argv[i], "-debug") == 0) {
            debug = 1;
        } else if (argv[i][0] != '-' && !input_filename) {
            input_filename = argv[i];
        }
    }
    (void)debug;  /* se usará para controlar las impresiones */

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
            /* Si hay archivo de entrada, también se exporta a <nombre>.ci */
            if (input_filename) {
                char ci_name[512];
                make_out_name(input_filename, ".ci", ci_name, sizeof(ci_name));
                FILE *ci = fopen(ci_name, "w");
                if (ci) {
                    gen_intermediate_code(root, ci);
                    fclose(ci);
                } else {
                    perror("No se pudo crear el archivo .ci");
                }
            }
            /* ... tu bloque actual de codinter, sin cambios ... */
        } else if (target_assembly) {
            char ass_name[512] = "salida.ass";
            if (input_filename) make_out_name(input_filename, ".ass", ass_name, sizeof(ass_name));
            FILE *f = fopen(ass_name, "w");
            if (!f) {
                perror("No se pudo crear el archivo .ass");
                return 1;
            }
            gen_object_code(root, f);
            fclose(f);

            /* Ensamblar y linkear: nombre.out, o lo que venga en -o */
            char out_name[512];
            if (exe_name) snprintf(out_name, sizeof(out_name), "%s", exe_name);
            else if (input_filename) make_out_name(input_filename, ".out", out_name, sizeof(out_name));
            else snprintf(out_name, sizeof(out_name), "a.out");

            char cmd[2048];
            snprintf(cmd, sizeof(cmd), "gcc -x assembler -o %s %s", out_name, ass_name);
            if (system(cmd) != 0) {
                fprintf(stderr, "Error al ensamblar %s\n", ass_name);
                return 1;
            }
        } else {
            printf("\n--- Análisis sintáctico exitoso ---\n");
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
        /* ... tu bloque de errores, sin cambios ... */
    }

    return 0;
}