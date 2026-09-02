#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#define TYPE_INT 1
#define TYPE_BOOL 2

struct symbol {
    int flag;
    char *name;
    int type;
    int value;
    struct symbol *next;
};

void init_symbol_table(void);
void open_level(void);
void close_level(void);
struct symbol *insert_symbol(char *name, int type);
struct symbol *search_symbol(char *name);

#endif
