/* Funciones C que usan los tests de extern (enlazadas con "// link:") */
#include <stdbool.h>

int neg(void) { return -1; }
int suma3(int a, int b, int c) { return a + b + c; }
bool esPar(int x) { return x % 2 == 0; }

/* Salida para los tests con "// expect:": un valor por línea */
#include <stdio.h>
void printInt(int x) { printf("%d\n", x); }
void printBool(bool b) { printf("%s\n", b ? "true" : "false"); }
