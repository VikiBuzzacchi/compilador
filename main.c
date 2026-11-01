#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "intermediate_code.h"
#include "tac.h"
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


enum target { T_NONE, T_FRONT, T_CODINTER, T_ASSEMBLY };

int main(int argc, char **argv) {
    enum target target = T_NONE;
    int debug = 0;
    char *input_filename = NULL;
    char *exe_name = NULL;          /* valor de -o */

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-target") == 0 && i + 1 < argc) {
            const char *t = argv[++i];
            if (strcmp(t, "codinter") == 0) target = T_CODINTER;
            else if (strcmp(t, "assembly") == 0) target = T_ASSEMBLY;
            else if (strcmp(t, "scan") == 0 || strcmp(t, "parse") == 0) target = T_FRONT;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            exe_name = argv[++i];
        } else if (strcmp(argv[i], "-debug") == 0) {
            debug = 1;
        } else if (argv[i][0] != '-' && !input_filename) {
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
        if (target == T_CODINTER) {
            /* Con archivo de entrada se escribe <nombre>.ci; el TAC va a consola
               solo con -debug, o si la entrada es stdin (no hay archivo de salida) */
            if (debug || !input_filename)
                gen_intermediate_code(root, stdout);
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
        } else if (target == T_ASSEMBLY && !input_filename) {
            gen_object_code(build_tac(root), stdout);
        } else if (target == T_FRONT || (target == T_NONE && !input_filename)) {
            /* Solo análisis (entrada por stdin sin target, o scan/parse) */
            if (debug) printf("\n--- Análisis sintáctico exitoso ---\n");
        } else {
            /* T_ASSEMBLY o T_NONE con archivo: se genera <nombre>.ass */
            char ass_name[512];
            make_out_name(input_filename, ".ass", ass_name, sizeof(ass_name));
            FILE *f = fopen(ass_name, "w");
            if (!f) {
                perror("No se pudo crear el archivo .ass");
                return 1;
            }
            gen_object_code(build_tac(root), f);
            fclose(f);

            /* Sin -target: además se ensambla y linkea a <nombre>.out (o lo de -o) */
            if (target == T_NONE) {
                char out_name[512];
                if (exe_name) snprintf(out_name, sizeof(out_name), "%s", exe_name);
                else make_out_name(input_filename, ".out", out_name, sizeof(out_name));

                char cmd[2048];
                snprintf(cmd, sizeof(cmd), "gcc -x assembler -o %s %s", out_name, ass_name);
                if (system(cmd) != 0) {
                    fprintf(stderr, "Error al ensamblar %s\n", ass_name);
                    return 1;
                }
            }
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
        return 1;
    }

    return 0;
}