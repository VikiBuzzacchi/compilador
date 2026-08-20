#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#define TYPE_INT 1
#define TYPE_BOOL 2

void install_symbol(char *name, int type);
int get_value_from_table(char *name);
void update_symbol_value(char *name, int value);

#endif