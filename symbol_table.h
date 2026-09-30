#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

enum sym_type {
    TYPE_VOID,
    TYPE_INT,
    TYPE_BOOL,
    TYPE_FLOAT
};

enum sym_kind {
    SYM_VAR,
    SYM_PARAM,
    SYM_METHOD
};

struct symbol {
    int flag;
    char *name;
    enum sym_type type;
    enum sym_kind kind;
    int nparams;
    enum sym_type *param_types;
    int value;
    struct symbol *next;
};

void init_symbol_table(void);
void open_level(void);
void close_level(void);
struct symbol *insert_symbol(char *name, enum sym_type type, enum sym_kind kind);
struct symbol *search_symbol(char *name);

#endif
