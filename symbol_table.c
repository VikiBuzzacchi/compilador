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
    struct symbol *s = lvl->symbols;
    while (s) {
        struct symbol *next = s->next;
        free(s->name);
        free(s);
        s = next;
    }
    top = lvl->prev;
    free(lvl);
}

struct symbol *insert_symbol(char *name, int type) {
    if (!top) return NULL;
    for (struct symbol *s = top->symbols; s; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            printf("Error semántico: Variable '%s' ya declarada.\n", name);
            return NULL;
        }
    }
    struct symbol *s = malloc(sizeof(struct symbol));
    s->flag = 0;
    s->name = strdup(name);
    s->type = type;
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
    printf("Error semántico: Variable '%s' no declarada.\n", name);
    return NULL;
}
