#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

enum sym_type {
    TYPE_VOID,
    TYPE_INT,
    TYPE_BOOL,
    TYPE_FLOAT
};

struct symbol {
    int flag;
    char *name;
    enum sym_type type;
    int value;
    struct symbol *next;
};

void init_symbol_table(void);
void open_level(void);
void close_level(void);
struct symbol *insert_symbol(char *name, enum sym_type type);
struct symbol *search_symbol(char *name);

#endif
