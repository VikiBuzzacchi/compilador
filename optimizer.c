#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "optimizer.h"

/* Nombres aceptados por -opt, en el orden de la estructura */
static const char *opt_names[] = { "constfold", "constprop", "deadcode", "tempslots" };
#define N_OPTS (int)(sizeof(opt_names) / sizeof(opt_names[0]))

static int *opt_field(struct opt_flags *o, int k) {
    switch (k) {
        case 0:  return &o->constfold;
        case 1:  return &o->constprop;
        case 2:  return &o->deadcode;
        default: return &o->tempslots;
    }
}

int parse_opt_list(const char *list, struct opt_flags *opts) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", list);
    int valid = 1;
    for (char *name = strtok(buf, ","); name; name = strtok(NULL, ",")) {
        if (strcmp(name, "all") == 0) {
            for (int k = 0; k < N_OPTS; k++) *opt_field(opts, k) = 1;
            continue;
        }
        int found = 0;
        for (int k = 0; k < N_OPTS; k++)
            if (strcmp(name, opt_names[k]) == 0) { *opt_field(opts, k) = 1; found = 1; }
        if (!found) {
            fprintf(stderr, "Optimización desconocida: '%s' (válidas: all", name);
            for (int k = 0; k < N_OPTS; k++) fprintf(stderr, ", %s", opt_names[k]);
            fprintf(stderr, ")\n");
            valid = 0;
        }
    }
    return valid;
}

int any_opt(const struct opt_flags *opts) {
    return opts->constfold || opts->constprop || opts->deadcode || opts->tempslots;
}

/* ---------- AST ---------- */

/* Plegado de constantes: reemplaza operaciones entre literales por su resultado.
   TODO: implementar. */
static void fold_constants(struct node *root) {
    (void) root;
}

void optimize_ast(struct node *root, const struct opt_flags *opts) {
    if (opts->constfold) fold_constants(root);
}

/* ---------- TAC ---------- */

/* Propagación de constantes: usa el valor conocido de una variable en lugar de leerla.
   TODO: implementar. */
static struct tac *propagate_constants(struct tac *code) {
    return code;
}

/* Eliminación de código inalcanzable: instrucciones después de return/goto hasta la próxima etiqueta.
   TODO: implementar. */
static struct tac *remove_dead_code(struct tac *code) {
    return code;
}

struct tac *optimize_tac(struct tac *code, const struct opt_flags *opts) {
    if (opts->constprop) code = propagate_constants(code);
    if (opts->deadcode)  code = remove_dead_code(code);
    return code;
}
