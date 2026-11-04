#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "intermediate_code.h"
#include "tac.h"
#include "object_code.h"
#include "optimizer.h"

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
    char link_files[1024] = "";     /* archivos extra (.c, .o, .s) para enlazar: definen los extern */
    struct opt_flags opts = { 0 };  /* por defecto, ninguna optimización */

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-opt") == 0) {
            /* La lista es opcional: el siguiente argumento es la lista si no es una opción
               ni tiene '.' (los nombres de optimización no tienen; los archivos sí).
               "-opt" solo equivale a "-opt all". */
            if (i + 1 < argc && argv[i + 1][0] != '-' && !strchr(argv[i + 1], '.')) {
                if (!parse_opt_list(argv[++i], &opts)) return 1;
            } else {
                parse_opt_list("all", &opts);
            }
            continue;
        }
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
        } else if (argv[i][0] != '-') {
            strncat(link_files, " ", sizeof(link_files) - strlen(link_files) - 1);
            strncat(link_files, argv[i], sizeof(link_files) - strlen(link_files) - 1);
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
        /*Si el programa tiene un float y quiero generar ASSEMBLY notifico
        ASSEMBLY no soporta floats */
        if (target == T_ASSEMBLY || (target == T_NONE && input_filename)) {
            struct node *f = find_float(root);
            if (f) {
                fprintf(stderr, "Error en la línea %d: float no soportado en generación de código\n", f->line);
                return 1;
            }
        }

        /* Optimizaciones (solo las activadas con -opt): primero sobre el AST, después sobre el TAC */
        optimize_ast(root, &opts);
        struct tac *code = NULL;
        if (target == T_CODINTER || target == T_ASSEMBLY || (target == T_NONE && input_filename))
            code = optimize_tac(build_tac(root), &opts);

        if (target == T_CODINTER) {
            /* Con archivo de entrada se escribe <nombre>.ci, o <nombre>.opt si hay optimizaciones;
               el TAC va a consola solo con -debug, o si la entrada es stdin (no hay archivo de salida) */
            if (debug || !input_filename)
                print_tac(code, stdout);
            if (input_filename) {
                const char *ext = any_opt(&opts) ? ".opt" : ".ci";
                char ci_name[512];
                make_out_name(input_filename, ext, ci_name, sizeof(ci_name));
                FILE *ci = fopen(ci_name, "w");
                if (ci) {
                    print_tac(code, ci);
                    fclose(ci);
                } else {
                    fprintf(stderr, "No se pudo crear el archivo %s\n", ci_name);
                }
            }
        } else if (target == T_ASSEMBLY && !input_filename) {
            gen_object_code(code, stdout);
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
            gen_object_code(code, f);
            fclose(f);

            /* Sin -target: además se ensambla y linkea a <nombre>.out (o lo de -o) */
            if (target == T_NONE) {
                char out_name[512];
                if (exe_name) snprintf(out_name, sizeof(out_name), "%s", exe_name);
                else make_out_name(input_filename, ".out", out_name, sizeof(out_name));

                char cmd[4096];
                /* -x none: los archivos extra se compilan según su extensión */
                snprintf(cmd, sizeof(cmd), "gcc -o %s -x assembler %s -x none%s",
                         out_name, ass_name, link_files);
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