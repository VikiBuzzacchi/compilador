# Documentación — Etapa 3: Generador de Código Intermedio

Entrada: AST validado semánticamente y tabla de símbolos (etapa 2). Salida: Código de Tres Direcciones (TAC).

## 19/10/2026 (Victoria)

Se agregó el generador de código intermedio en `intermediate_code.h` y `intermediate_code.c`. No se modificaron el scanner ni el parser (`lexer.l`, `parser.y`): la fase solo se invoca desde `main.c`.

### Decisiones de diseño
- **Un recorrido recursivo del AST que emite TAC directo a un `FILE *`**, sin lista intermedia de instrucciones. Alcanza para generar la secuencia lineal que pide la consigna y evita una estructura de datos extra.
    - `gen_expr` devuelve el operando donde queda el resultado (literal, variable o temporal nuevo). `gen_stmt` y `gen_block` recorren las sentencias; `gen_method` delimita cada método.
    - Las listas (bloques, argumentos, métodos) se recorren con `next`, como en la etapa 2.
- **Temporales y etiquetas**: contadores globales `temp_count` y `label_count` que generan `t0, t1, ...` y `L0, L1, ...`. Se reinician al comenzar a generar, y los temporales siguen numerándose entre métodos.
- **Formato del TAC**
    - Binaria: `t0 = a + b`. Unaria: `t1 = -a`, `t2 = !b`. Asignación: `x = t0`.
    - `if`: `ifFalse c goto Lelse/Lend`; con `else` se agrega `goto Lend` al final del `then`.
    - `while`: `Linicio:`, condición, `ifFalse c goto Lfin`, cuerpo, `goto Linicio`, `Lfin:`.
    - Llamada: un `param` por argumento (en orden, después de evaluar todos) y `call f, n`. Si el método devuelve valor: `t = call f, n`.
    - `return e` / `return`. Cada método va entre `func begin f` y `func end f`.
    - Solo se usa `ifFalse`; `ifTrue` no hace falta.
- **No hay cortocircuito** en `&&` y `||`: se evalúan los dos operandos y se emite una operación binaria, igual que el resto de los operadores.
- **Variables homónimas de ámbitos anidados** (`test21`): el TAC usa solo el nombre, y dos variables distintas se confundirían. `var_name` identifica cada variable por su `struct symbol` (que el AST ya guarda en `sym`) y le agrega sufijo a la segunda con el mismo nombre dentro del método (`x`, `x_1`). La tabla se reinicia en cada método.
- **Declaraciones** (`NODE_VAR_DECL`) y variables globales no emiten instrucciones: en TAC básico solo importan las sentencias ejecutables.
- **Sin optimizaciones**: por ejemplo `-2147483646` genera `t0 = -2147483646` (unario sobre el literal), y `4 * 2` no se pliega.

### Integración (`main.c` y `Makefile`)
- `./compilador -target codinter archivo.txt` imprime el TAC en consola y además lo exporta a `<nombre>.ci` en el directorio actual. Con entrada por stdin (`< archivo.txt`) solo imprime en consola.
- El TAC se genera solo si no hubo errores léxicos, sintácticos ni semánticos. Sin `-target codinter` el comportamiento es el de las etapas anteriores.
- `Makefile`: se agregó `intermediate_code.c` a la compilación. `.gitignore`: se ignoran los `*.ci` generados.

### Tests
- `run_tests.sh` ahora corre también `./compilador -target codinter < tests/cases/<nombre>.txt` y compara con `tests/expected/<nombre>.ci.expected` (un caso por cada test válido: 1, 2, 3, 12, 17, 21, 25, 27 y 28).
- `test28.txt` (nuevo): `if-else`, `while`, llamada `void` como sentencia, `return;` y llamadas anidadas como argumentos.
- Cubren el resto: `test27` (aritmética, relacionales, lógicos, unarios, `while`, `if`), `test17` (recursión, `param`/`call`/`return`), `test21` (ámbitos anidados).
- Resultado: `make test` → `PASS: 38  FAIL: 0` (28 de etapas 1-2 + 9 de TAC + `test28`).

### Pendiente
- Chequeo de tipos (heredado de la etapa 2): no hay conversión explícita int→float en el TAC (`y = x` con `y` float se emite tal cual).
- Etapas posteriores: generación de código objeto y optimizaciones (fuera de esta entrega).
