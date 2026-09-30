# Compilador — Proyecto (Taller de Diseño de Software, Cod. 3306)

Compilador para un lenguaje de expresiones extendido con declaración de
variables (`int`/`float`/`bool`), métodos con parámetros, `if`/`else`, `while`,
`return`, llamadas y expresiones aritméticas, relacionales y lógicas.
Implementado con Flex y Bison.

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
./compilador < tests/cases/test1.txt
```


## Tests automáticos

```sh
make test          # o ./run_tests.sh
```

Corre cada `tests/cases/*.txt` y compara la salida (stdout y stderr) con `tests/expected/<nombre>.expected`.
Cada test pasa (`PASS`) o falla (`FAIL`).

### Archivos de prueba incluidos

| Archivo                 | Descripción   |
| `tests/cases/test1.txt` | Programa `void main` sin `return`, válido |
| `tests/cases/test2.txt` | Programa `int main` con varios `return`, válido |
| `tests/cases/test3.txt` | Programa `int main` con constante entera negativa cercana al límite, válido |
| `tests/cases/test2error.txt` | Símbolos inválidos (`@`, `!`), para probar reporte de errores |
| `tests/cases/test4error.txt` | Entero fuera de rango (`99999999999`). Esperado: error léxico. |
| `tests/cases/test5error.txt` | Comentario `/*` sin cerrar. Esperado: error léxico. |
| `tests/cases/test6error.txt` | Literales mal formados (`1.2.3`, `3abc`). | 
| `tests/cases/test7error.txt` | `;` faltante antes de un `if` con bloque. Esperado: 1 error |


## Estructura del proyecto

1. Analizador léxico (lexer.l)
2. Parser, lex + bison (parser.y)
3. AST (ast.h, ast.c)
4. tabla de símbolos (symbol_table.c, symbol_table.h)
5. Intérprete (ast.c)


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
  
