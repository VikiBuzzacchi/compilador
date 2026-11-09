# Compilador — Proyecto (Taller de Diseño de Software, Cod. 3306)

Compilador para un lenguaje de expresiones extendido con declaración de
variables (`int`/`float`/`boolean`), métodos con parámetros, `if`/`else`,
`while`, `return`, llamadas y expresiones aritméticas, relacionales y lógicas.
Implementado con Flex y Bison.

## Requisitos

- `gcc`
- `flex`
- `bison`

## Build

```sh
make        # compila
make clean  # limpia el binario y los archivos generados por flex/bison
```

## Uso

```sh
./compilador < tests/cases/test1.txt
./compilador -target scan  prog.ctds                   # análisis léxico -> prog.lex
./compilador -target parse prog.ctds                   # análisis sintáctico/semántico -> prog.sint
./compilador -target codinter prog.ctds                # TAC -> prog.ci
./compilador -target codinter -debug < prog.ctds       # TAC por consola (sin archivo de entrada)
./compilador -opt all prog.ctds                        # compila y enlaza con optimizaciones
./compilador -target codinter -opt all prog.ctds       # TAC optimizado -> prog.opt
./compilador tests/asm/extern.txt tests/asm/lib.c -o prog  # enlaza lib.c, que define los extern
```

Con un archivo de entrada se genera `<nombre>.ass`. Sin `-target` además se
enlaza el ejecutable `<nombre>.out` (o lo indicado con `-o`); con `-target
assembly` solo se genera el `.ass`. Con entrada por stdin y sin `-target` solo
se analiza; con `-target assembly` el assembly va a la consola.

## Tests automáticos

```sh
make test          # o ./run_tests.sh
```

`run_tests.sh` ejecuta:

- cada `tests/cases/*.txt` y compara stdout+stderr con `tests/expected/<nombre>.expected`;
- cada `tests/expected/*.ci.expected` con `-target codinter`;
- cada `tests/expected/*.opt.expected` con `-target codinter -opt <lista>`;
- cada `tests/asm/*.txt`, sin optimizar y con `-opt all`, verificando `// exit:`,
  `// expect:`, `// error:` y `// frame:` cuando corresponda.

Cada test pasa (`PASS`) o falla (`FAIL`).

### Archivos de prueba incluidos

| Archivo | Descripción |
|---|---|
| `tests/cases/test1.txt` | Programa `void main` sin `return`, válido |
| `tests/cases/test2.txt` | Programa `int main` con varios `return`, válido |
| `tests/cases/test3.txt` | Programa `int main` con constante entera negativa cercana al límite, válido |
| `tests/cases/test2error.txt` | Símbolos inválidos (`@`, `!`), para probar reporte de errores |
| `tests/cases/test4error.txt` | Entero fuera de rango (`99999999999`). Esperado: error léxico |
| `tests/cases/test5error.txt` | Comentario `/*` sin cerrar. Esperado: error léxico |
| `tests/cases/test6error.txt` | Literales mal formados (`1.2.3`, `3abc`) |
| `tests/cases/test7error.txt` | `;` faltante antes de un `if` con bloque. Esperado: 1 error |
| `tests/asm/extern.txt` + `tests/asm/lib.c` | Llamadas a métodos externos definidos en C |
| `tests/asm/intmin.txt` | `-2147483648` como `INT_MIN` |
| `tests/asm/int32.txt` | Aritmética de 32 bits con wrap |
| `tests/asm/tempslots.txt` | Reutilización de slots de temporales (`-opt tempslots`) |

## Estructura del proyecto

1. Analizador léxico (`lexer.l`)
2. Parser (`parser.y`) y construcción del AST (`ast.h`, `ast.c`)
3. Tabla de símbolos (`symbol_table.h`, `symbol_table.c`)
4. Código intermedio TAC (`intermediate_code.h`, `intermediate_code.c`, `tac.h`)
5. Optimizador opcional (`optimizer.h`, `optimizer.c`)
6. Generador de código objeto x86-64 (`object_code.h`, `object_code.c`)
7. Driver y CLI (`main.c`)

## Tabla de símbolos

Pila de niveles, cada uno una lista enlazada de símbolos.

- `init_symbol_table()` — inicializa la pila y abre el nivel global.
- `open_level()` / `close_level()` — apilan/desapilan niveles para métodos y bloques anidados.
- `insert_symbol(name, type, kind)` — inserta en el nivel tope; falla si el nombre ya existe ahí.
- `search_symbol(name)` — busca desde el nivel tope hacia afuera, de modo que un bloque interno puede ocultar un nombre externo.

Cada nodo de identificador resuelve su símbolo una sola vez, al construirse el AST,
y guarda el puntero en `sym`. El símbolo incluye tipo, clase (`variable`, `parámetro`
o `método`), cantidad y tipos de parámetros, si es global y si es `extern`.
