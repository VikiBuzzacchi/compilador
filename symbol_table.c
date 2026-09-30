#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol_table.h"

struct level {
    struct symbol *symbols;
    struct level *prev;
};

static struct level *top = NULL;

void init_symbol_table(void) {
    top = NULL;
    open_level();
}

void open_level(void) {
    struct level *lvl = malloc(sizeof(struct level));
    lvl->symbols = NULL;
    lvl->prev = top;
    top = lvl;
}

void close_level(void) {
    if (!top) return;
    struct level *lvl = top;
    top = lvl->prev;
    free(lvl);
}

struct symbol *insert_symbol(char *name, enum sym_type type, enum sym_kind kind) {
    if (!top) return NULL;
    for (struct symbol *s = top->symbols; s; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            return NULL;
        }
    }
    struct symbol *s = malloc(sizeof(struct symbol));
    s->flag = 0;
    s->name = strdup(name);
    s->type = type;
    s->kind = kind;
    s->nparams = 0;
    s->param_types = NULL;
    s->value = 0;
    s->next = top->symbols;
    top->symbols = s;
    return s;
}

struct symbol *search_symbol(char *name) {
    for (struct level *lvl = top; lvl; lvl = lvl->prev) {
        for (struct symbol *s = lvl->symbols; s; s = s->next) {
            if (strcmp(s->name, name) == 0) return s;
        }
    }
    return NULL;
}
