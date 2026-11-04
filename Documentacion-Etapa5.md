# Documentación — Etapa 5: Optimizaciones

Entrada: AST validado, TAC (etapa 3) y generador de código objeto (etapa 4). Salida: código intermedio y assembly optimizados.

## Correcciones previas a las optimizaciones

Antes de optimizar se corrigieron problemas de la etapa 4. Las optimizaciones cambian el código generado, y para verificar que no cambien el resultado de los programas hace falta que el compilador se reconstruya bien, que la semántica de `int` sea una sola y que haya tests que ejecuten el assembly.

### 1. El Makefile no recompilaba al modificar `object_code.c` ni los headers

**Solución (`Makefile`):** las fuentes y los headers están en `SRCS`/`HDRS` y son dependencias de `compilador`. Además, `lex.yy.c` depende de `parser.tab.h`, porque el lexer usa los códigos de token que genera bison.

### 2. Aritmética de 64 bits en lugar del `int` de 32 bits

El lexer limita los literales a 32 bits (`2147483647`), pero el assembly calculaba con 64 (`addq`, `imulq`, ...). Eso causaba tres problemas:

- **Overflow distinto al de C.** `x = 2147483647; x = x + 1;` daba `2147483648` (positivo) en vez de `-2147483648`, así que `x < 0` era falso.
- **Funciones externas en C.** Una función C que devuelve `int` deja el resultado solo en `%eax`, y la mitad alta de `%rax` queda en 0. Un `-1` llegaba como `4294967295`, positivo. Se verificó enlazando el assembly generado con `int neg(void) { return -1; }` en C: antes `neg() < 0` daba falso, ahora da verdadero. Lo mismo pasa con `bool` de C, que viene solo en `%al`.
- **Optimizaciones.** El plegado de constantes calcula en C con 32 bits mientras compila. Si el runtime usa 64 bits, el mismo programa daría resultados distintos según se pliegue o no una expresión.

**Solución (`object_code.c`):** se mantienen los slots de 8 bytes y las instrucciones de 64 bits, pero cada valor nuevo se normaliza a 32 bits con signo:

| Dónde | Instrucción agregada |
|---|---|
| Después de `+ - * / %` | `movslq %r10d, %r10` |
| Después del `-` unario | `movslq %r10d, %r10` |
| Después de un `call` con resultado `int` | `movslq %eax, %rax` |
| Después de un `call` con resultado `boolean` | `movzbq %al, %rax` |

`movslq` toma los 32 bits bajos y copia el bit de signo en la mitad alta. 

### 3. Test para assembly
**Solución (`run_tests.sh`):** ahora también recorre `tests/asm/*.txt`. Por cada uno compila, ejecuta y compara el exit code con el valor de la primera línea `// exit: N`. Los tests sin esa línea se saltean. Se agregó `// exit:` a los tests existentes, con valores verificados contra gcc, y un test nuevo: int32.
Antes de la corrección 2, test int32 fallaba con `exit 1`. Ahora pasan los 45 tests.

## Métodos `extern`

Un método declarado con `extern` en lugar de cuerpo está definido afuera en C, y se enlaza al generar el ejecutable:

```c
int suma3(int a, int b, int c) extern;
boolean esPar(int x) extern;
int abs(int x) extern;          // también funciones de la biblioteca estándar
```

```sh
./compilador programa.txt lib.c -o programa    # lib.c (o .o, .s) define los extern
```

**Cambios:**
- **`lexer.l`:** palabra reservada `extern` (token `EXTERN`).
- **`parser.y`:** nueva regla `method_body`: `'{' block_item_list '}'` o `EXTERN ';'`. En el caso `extern` el método queda con cuerpo `NULL` y se marca `is_extern` en su símbolo. Los parámetros se declaran igual que en un método normal, así que los chequeos de cantidad y tipo de argumentos en las llamadas no cambian.
- **`symbol_table.h/.c`:** campo `is_extern` en `struct symbol`.
- **Semántica:** `main` no puede ser `extern` (`'main' no puede ser extern`).
- **`intermediate_code.c`:** `build_tac` saltea los métodos sin cuerpo. Los `extern` no generan `func begin`/`func end` y solo aparecen en los `call`.
- **`object_code.c`:** sin cambios.
- **`main.c`:** los argumentos que no son opciones después del archivo de entrada se guardan y se pasan al `gcc` que enlaza: `gcc -o out -x assembler prog.ass -x none lib.c`. El `-x none` es necesario porque `-x assembler` se aplica a todos los archivos que siguen. Se agrandó el buffer del comando a 4096.
- **`run_tests.sh`:** la línea `// link: archivos` de un test de `tests/asm/` indica qué archivos enlazar.

**Tests:**

| Archivo | Qué prueba |
|---|---|
| `tests/asm/extern.txt` + `lib.c` | `int` negativo devuelto por C, `bool` de C, argumentos que son expresiones y llamadas (`suma3(doble(2), neg(), 1 + 1)`), `abs` de libc. Exit esperado 0 |
| `tests/cases/test29error.txt` | `int main() extern;` → error semántico |
| `tests/cases/test30.txt` | Programa válido con `extern`; su `.ci` no tiene `func begin` para los extern |
| `tests/cases/test31error.txt` | Llamada a un `extern` con cantidad de argumentos incorrecta |

Para comprobar que `extern.txt` detecta el problema de la corrección 2, se sacaron del assembly los `movslq %eax`/`movzbq %al`: el test falla con `exit 1`. 

## Tests de valores con `printInt` / `printBool`

El exit code permite chequear un solo valor por test, entre 0 y 255. Con `extern`, un test puede imprimir todos los resultados intermedios con funciones C y comparar la salida completa.

- **`tests/asm/lib.c`:** agrega `void printInt(int)` y `void printBool(bool)`.
```c
printInt(resta(y * 2, y - 3));              // expect: 13
printBool(!(x == 3) || x < 4 && true);      // expect: true
```

## Optimizaciones opcionales: opción `-opt`

La spec pide la opción `-opt [optimización]` ("`all` realiza todas las optimizaciones soportadas"). s

### Uso

```sh
c-tds prog.ctds                                   # sin optimizar
c-tds -opt all prog.ctds                          # todas
c-tds -opt prog.ctds                              # igual que -opt all
c-tds -opt constfold,deadcode prog.ctds           # solo esas dos
c-tds -target codinter -opt all prog.ctds         # escribe prog.opt (TAC optimizado)
```

| Nombre | Optimización | Sobre | Estado |
|---|---|---|---|
| `constfold` | Plegado de constantes (`2 * 3` → `6`) | AST | **implementada** |
| `constprop` | Propagación de constantes (`x = 3; y = x + 1` → `y = 4`) | TAC | pendiente |
| `deadcode` | Eliminación de código inalcanzable | TAC | pendiente |
| `tempslots` | Reutilización de slots de temporales | código objeto | pendiente |

Un nombre inexistente es un error y el compilador termina con código 1:

```
Optimización desconocida: 'constflod' (válidas: all, constfold, constprop, deadcode, tempslots)
```

### Cómo funciona

**1. Lectura de la opción (`main.c`).** `-opt` llena una `struct opt_flags` (`optimizer.h`) con un campo por optimización, todos en 0 por defecto. `parse_opt_list` separa la lista por comas y activa cada nombre (`all` activa todos).

La lista es opcional (`-opt [optimización]`), y eso genera una ambigüedad: en `c-tds -opt prog.ctds`, ¿`prog.ctds` es el archivo o el nombre de una optimización? **Decisión:** el siguiente argumento se toma como lista si no empieza con `-` y no tiene `.`. Los nombres de optimización nunca tienen punto y los archivos de entrada sí (`.ctds`). Si no, `-opt` solo equivale a `-opt all`. Así, un nombre mal escrito (`-opt constflod`) da error en vez de tomarse como archivo de entrada.

**2. Pipeline (`main.c`).** Cada optimización es una pasada independiente, y se ejecuta solo si está activada:

```
yyparse()                     AST + análisis semántico (sin cambios)
   ↓
optimize_ast(root, &opts)     constfold
   ↓
build_tac(root)               TAC (sin cambios)
   ↓
optimize_tac(code, &opts)     constprop, después deadcode
   ↓
print_tac → .ci / .opt        o   gen_object_code → .ass → .out
```

Antes, `main.c` llamaba a `gen_intermediate_code` y a `build_tac` por separado en cada rama. Ahora el TAC se construye y optimiza **una sola vez**, y todas las salidas (`.ci`/`.opt`, `.ass`, `.out`) usan esa misma lista.

**Decisión: las optimizaciones son pasadas separadas y no se hacen dentro de `new_binop`/`gen_expr`.**
- Se pueden activar y desactivar con `-opt`.
- El análisis semántico trabaja sobre el árbol original, así que los mensajes de error y sus líneas no dependen de las optimizaciones.
- Cada optimización está en una función propia de `optimizer.c` y se puede probar por separado.
- El orden importa y queda explícito en un solo lugar: el plegado va antes de la propagación, y las dos antes de eliminar código muerto.

**3. Salida `.opt`.** Con `-target codinter`, si hay al menos una optimización activa se escribe `<nombre>.opt` (el TAC optimizado) en lugar de `<nombre>.ci`. Así se puede comparar lado a lado el código antes y después de optimizar. La spec no dice qué contiene `.opt`; esta es una decisión de diseño a confirmar con la cátedra. Sin `-target`, `-opt` afecta al `.ass` y al `.out`, pero los nombres de archivo no cambian.

### Agregar una optimización nueva

1. Agregar el campo en `struct opt_flags` (`optimizer.h`) y el nombre en `opt_names` (`optimizer.c`), en la misma posición.
2. Implementar la pasada como función `static` en `optimizer.c` y llamarla desde `optimize_ast` o `optimize_tac`, según sobre qué trabaje.
3. Para `tempslots`, que trabaja en el generador de código objeto, hay que pasarle los flags a `gen_object_code`. 

### Tests

`run_tests.sh` ejecuta cada test de `tests/asm/` **dos veces**: sin optimizar (`asm`) y con `-opt all` (`asm -opt all`). Los dos tienen que dar el exit code y la salida esperados. 

```
PASS   valores (asm)
PASS   valores (asm -opt all)
```

## Plegado de constantes (`-opt constfold`)

**Qué hace.** Calcula en tiempo de compilación las operaciones cuyos operandos son literales y las reemplaza por el resultado. Así el programa ya no las calcula cada vez que se ejecuta.

```c
x = 2 + 3 * 4;        →   x = 14
b = !(1 == 2);        →   b = true
x = x * (2 + 2);      →   x = x * 4         (se pliega solo la parte constante)
```

En el TAC de `tests/cases/test32.txt`, `main` pasa de 62 a 24 instrucciones (`test32.ci` vs `test32.opt`):

```
sin optimizar           con -opt all
  t0 = 3 * 4              x = 14
  t1 = 2 + t0
  x = t1
```

### Cómo funciona

`fold_constants` (`optimizer.c`) es una pasada sobre el AST que se ejecuta después del análisis semántico y antes de generar el TAC.

**1. Recorrido** Para cada nodo, primero pliega sus hijos (`left`, `right`, `third`) y después el nodo; las listas (sentencias, argumentos) se recorren por `next`. Por eso una expresión anidada se pliega de abajo hacia arriba en una sola pasada:

```
      +                 +              14
     / \               / \
    2   *      →      2   12     →
       / \
      3   4


**2. Modificación** Un nodo plegado no se reemplaza por uno nuevo: el mismo nodo se convierte en literal (`make_int`/`make_bool` cambian `kind`, `value` y `data_type` y borran los hijos). Así el padre sigue apuntando al mismo nodo y no hace falta actualizar punteros, y se conservan su `line` y su `next`.

**3. Reglas** (solo `int` y `boolean`, porque `float` no tiene back-end)

| Operación | Se pliega cuando | Resultado |
|---|---|---|
| `+ - *` | los dos operandos son literales `int` | con wrap de 32 bits |
| `/ %` | los dos son literales y el divisor no es 0 | trunca hacia 0, como `idivq` |
| `< > ==` | los dos son literales `int` | literal `boolean` |
| `==` | los dos son literales `boolean` | literal `boolean` |
| `-e` | `e` es literal `int` | con wrap |
| `!e` | `e` es literal `boolean` | literal `boolean` |
| `false && e`, `true \|\| e` | el izquierdo es literal | `false` / `true` |
| `true && e`, `false \|\| e` | el izquierdo es literal | `e` |
| `e && true`, `e \|\| false` | el derecho es literal | `e` |

### Decisiones y casos borde

- **Mismo resultado que sin optimizar.** Plegar no puede cambiar lo que calcula el programa. Por eso el plegado usa la misma aritmética que el código generado: 32 bits con wrap. En C, el overflow de `int` con signo es comportamiento indefinido, así que las cuentas se hacen en `unsigned` (optimizer.c) y se convierten a `int` (`2147483647 + 1` = `-2147483648`).
- **`x / 0` y `x % 0` no se pliegan.** ERROR
- **CASO PARTICULAR `INT_MIN / -1`.** El resultado real (`+2147483648`) no entra en un `int`. da `INT_MIN`, y `INT_MIN % -1` da `0`. No es el valor matemático, pero la regla es la misma que para `2147483647 + 1`. Para que coincida con el resultado sin optimizar

- Plegar condiciones deja `if (1 > 2)` como `ifFalse false goto L`. Eliminar esa rama le corresponde a `deadcode`, que por eso se ejecuta después.

### Tests

| Test | Qué verifica |
|---|---|
| `tests/cases/test32.txt` + `test32.opt.expected` | El TAC con `-opt all`: qué se pliega y qué no (`7 / 0`, `x > 2 && false`, plegado parcial de `x * (2 + 2)`). `run_tests.sh` tiene un loop nuevo que compara `-target codinter -opt all` con `<nombre>.opt.expected` |
| `tests/asm/plegado.txt` | Los resultados al ejecutar: wrap, `INT_MIN`, `INT_MIN / -1`, división con negativos, lógica entre literales y cortocircuito con llamadas (cuenta las llamadas a `inc()`). Corre sin optimizar y con `-opt all` y los dos modos tienen que coincidir |
| `valores`, `corto`, `int32`, ... (existentes) | Corren también con `-opt all` y siguen dando lo mismo |


## `float` en la generación de código: error en lugar de código incorrecto

**Problema.** El generador de assembly no soporta `float`: los literales se cargaban como 0 y las operaciones se hacían con instrucciones enteras. Tampoco funcionaba llamar a funciones C con `float`. Un programa con `float` compilaba sin avisar y generaba un ejecutable que calculaba mal.
Un programa con `float` **no se compila a assembly y se reporta el motivo**: es mejor un error explícito que un ejecutable con resultados incorrectos.

```
$ c-tds prog.ctds
Error en la línea 4: float no soportado en generación de código
```

El compilador termina con código 1 y no genera `.ass` ni `.out`. El análisis (`-target parse`) y el código intermedio (`-target codinter`) siguen aceptando `float`, porque esas etapas sí lo manejan.

**Implementación.**
- **`ast.c`: `find_float(root)`** recorre el AST (`left`, `right`, `third` y las listas por `next`) y devuelve el primer nodo que usa `float`: un literal (`NODE_FLOAT`) o cualquier nodo con `data_type == TYPE_FLOAT`. Así detecta declaraciones de variables (globales o locales), parámetros, métodos que devuelven `float` (incluidos los `extern`), expresiones y llamadas. La línea del nodo se usa en el mensaje.
- **`main.c`:** después del análisis sin errores, si el destino es assembly (`-target assembly` o sin `-target`) y `find_float` encuentra un nodo, reporta el error y termina antes de optimizar y generar código.

**Por qué en el AST y no en el generador.** En el TAC los temporales no tienen tipo, así que un `float` sin literales (por ejemplo, un parámetro `float` recibido de una llamada con `int`) no se podría detectar ahí. En el AST, el análisis semántico ya dejó el tipo en cada nodo. Además, el chequeo ocurre antes de abrir el `.ass`, así que no queda un archivo a medio escribir.

**Tests.** Usan la línea `// error: mensaje` de `run_tests.sh`: la compilación tiene que fallar, mostrar ese mensaje y no dejar el `.ass`.

| Archivo | Caso |
|---|---|
| `tests/asm/floatvar.txt` | Variable local y literal `float` |
| `tests/asm/floatparam.txt` | `float` solo como parámetro, llamado con un `int`, sin literales: el chequeo no puede mirar solo los literales |
| `test27 (codinter)` (existente) | Un programa con `float` sigue generando su `.ci` |



## Pendientes

- **Propagación de constantes.**
- **Eliminación de código inalcanzable** (después de `return`/`goto`, ramas de `if (false)`).
- **Reutilización de slots de temporales** (lista de offsets libres).

## diferencias con la especificación 

Encontradas al comparar el compilador con `00-TDS-proyecto.pdf` y `01-TDS-spec-lenguaje.pdf`.

- [ ] **`_` en identificadores.** La gramática permite `⟨alpha_num⟩ → ⟨alpha⟩ | ⟨digit⟩ | _`, pero el lexer usa `[a-zA-Z][a-zA-Z0-9]*` y `mi_var` da error léxico. Cambiar a `[a-zA-Z][a-zA-Z0-9_]*`.
- [ ] **Métodos con retorno que llegan al final sin `return`.** La spec dice que un método que retorna un valor "no puede alcanzar el fin del método". Hoy `int f() { x = 1; }` compila sin error y devuelve 0. Falta un chequeo semántico: que todo camino del cuerpo termine en `return` (un `if` cuenta solo si tiene `else` y las dos ramas retornan).
- [x] **Opción `-opt [optimización]`.** Implementada; ver "Optimizaciones opcionales: opción `-opt`".
- [ ] **Salidas `.lex` / `.sint` y extensión `.ctds`.** Con `-target scan`/`parse` la spec pide generar un archivo con extensión según la etapa, y el archivo de entrada debe ser `.ctds`. Hoy no se generan esos archivos y los tests son `.txt`. Consultar con la cátedra si se evalúan.
- [ ] **Literales float.** La spec define `digit+ . digit+`. El lexer también acepta `1.`, `.5` y exponentes (`1e5`). Documentar como extensión o restringir.
- [x] **Error claro para `float` en la generación de código.** Implementado; ver "`float` en la generación de código: error en lugar de código incorrecto".
- [ ] **Literal `-2147483648`.** `2147483648` se rechaza en el lexer antes de aplicar el `-` unario, así que el mínimo no se puede escribir !!!!!
- [ ] **`extern` como extensión.**  un programa que use `extern` como identificador deja de compilar.
- [ ] **Documentación entrega final.** 
