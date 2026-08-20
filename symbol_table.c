#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol_table.h"

#define MAX_SYMBOLS 100

struct symbol {
    char *name;
    int type;
    int value;
};

struct symbol sym_table[MAX_SYMBOLS];
int sym_count = 0;

void install_symbol(char *name, int type) {
    for (int i = 0; i < sym_count; i++) {
        if (strcmp(sym_table[i].name, name) == 0) {
            printf("Error semántico: Variable '%s' ya declarada.\n", name);
            return;
        }
    }
    if (sym_count < MAX_SYMBOLS) {
        sym_table[sym_count].name = strdup(name);
        sym_table[sym_count].type = type;
        sym_table[sym_count].value = 0;
        sym_count++;
    } else {
        printf("Error: Tabla de símbolos llena.\n");
    }
}

int get_symbol_index(char *name) {
    for (int i = 0; i < sym_count; i++) {
        if (strcmp(sym_table[i].name, name) == 0) {
            return i;
        }
    }
    printf("Error semántico: Variable '%s' no declarada.\n", name);
    return -1;
}

int get_value_from_table(char *name) {
    int idx = get_symbol_index(name);
    if (idx != -1) return sym_table[idx].value;
    return 0;
}

void update_symbol_value(char *name, int value) {
    int idx = get_symbol_index(name);
    if (idx != -1) sym_table[idx].value = value;
}