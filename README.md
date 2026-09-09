# Compilador — Pre-Proyecto (Taller de Diseño de Software, Cod. 3306)

Compilador para un lenguaje de expresiones extendido con declaración de
variables (`int`/`bool`), asignaciones, constantes lógicas y una función
`main` con sentencias `return`. Implementado con `lex` (Flex) y `bison`.

## Requisitos

- `gcc`
- `flex`
- `bison`

## Build

```sh
make        # compila
make clean  # limpia binarios
```

## Uso

```sh
./compilador < test1.txt
```

Si no hay errores léxicos/sintácticos, imprime el pseudo-assembly generado
y el resultado de interpretar el AST (incluyendo cada `return`). Si hay
errores, se listan con su número de línea y no se genera código.

### Archivos de prueba incluidos

| `test1.txt` | Programa `void main` sin `return`, válido |
| `test2.txt` | Programa `int main` con varios `return`, válido |
| `test2error.txt` | Símbolos inválidos (`@`, `!`), para probar reporte de errores |

## Estructura del proyecto

1. Analizador léxico (lexer.l)
2. Parser, lex + bison (parser.y)
3. AST (ast.h, ast.c)
4. tabla de símbolos (symbol_table.c)
5. Intérprete (ast.c)
6. Generador de pseudo-assembly (ast.c)

## Tabla de símbolos

Pila de niveles, cada uno una lista enlazada de símbolos.

- `init_symbol_table()` — inicializa la pila y abre el nivel global.
- `open_level()` / `close_level()` — apilan/desapilan un nivel (la gramática
  actual no tiene bloques anidados, así que solo se usa el nivel global).
- `insert_symbol(name, type)` — inserta en el nivel tope; falla si el
  nombre ya existe en ese nivel.
- `search_symbol(name)` — busca desde el nivel tope hacia afuera.

Cada `NODE_VAR` resuelve su símbolo una sola vez, al crearse, y guarda el
puntero en vez de rebuscarlo por nombre en cada evaluación.
  
