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

### Métodos `extern`

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

### Tests de valores con `printInt` / `printBool`

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
| `constprop` | Propagación de constantes (`x = 3; y = x + 1` → `y = 4`) | TAC | **implementada** |
| `deadcode` | Eliminación de código inalcanzable | TAC | **implementada** |
| `tempslots` | Reutilización de slots de temporales | código objeto | **implementada** |

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
optimize_tac(code, &opts)     constprop y deadcode, repetidas hasta que no cambien nada
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

**Cada test de TAC prueba solo sus optimizaciones.** La primera línea del test indica cuáles (sin esa línea se usa `all`):

```c
// opt: constfold
```
El runner compila con `-target codinter -opt <lista>` y compara con `X.opt.expected`. Así, agregar una optimización nueva (con su propio test) no cambia los esperados de las anteriores: `test32` usa `// opt: constfold` y `test33` usa `// opt: constprop`.

## Plegado de constantes (`-opt constfold`)

**Qué hace.** Calcula en tiempo de compilación las operaciones cuyos operandos son literales y las reemplaza por el resultado. Así el programa ya no las calcula cada vez que se ejecuta.

```c
x = 2 + 3 * 4;        →   x = 14
b = !(1 == 2);        →   b = true
x = x * (2 + 2);      →   x = x * 4         (se pliega solo la parte constante)
```

En el TAC de `tests/cases/test32.txt`, `main` pasa de 53 a 24 instrucciones (sin contar etiquetas) (`test32.ci` vs `test32.opt`):

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

## Propagación de constantes (`-opt constprop`)

**Qué hace.** Si una variable tiene un valor conocido en un punto del programa, la reemplaza por ese valor, y calcula las operaciones que quedan con todos sus operandos constantes. Las constantes se encadenan:

```c
a = 10;
b = a * 2;      →  b = 20
c = b + a;      →  c = 30
```

**Dónde: en el TAC.** Necesita seguir las instrucciones en el orden en que se ejecutan y ver los saltos (etiquetas, `goto`, `ifFalse`), que en el TAC están explícitos. Como el plegado del AST ya pasó, la propagación también tiene que calcular.

- Se centraliza la logica en `eval_binop` / `eval_unop` (`optimizer.c`), que usan las dos optimizaciones
- En cada instrucción la propagación necesita saber si una variable tiene un valor conocido en ese punto. Para eso mantiene una tabla, `known[]`, de pares `(variable o temporal, valor)`. Las variables se identifican por símbolo.
- Después de propagar quedan instrucciones como `t0 = 20` que nadie lee. Además de sobrar, le ocupan un slot de pila al generador de assembly. La función marca en un arreglo `read[]` cada temporal que aparece como operando en alguna instrucción, y desengancha de la lista las asignaciones a temporales no marcados.

LIMITACIÓN: Al ser local, pierde información que una versión global conservaría. Después de `if (c) { x = 1; } else { x = 1; }`, `x` vale 1 por los dos caminos, pero la tabla se vacía en la etiqueta y `x` no se propaga. Resolverlo requiere análisis de flujo de datos.


| Regla rota | Resultado |
|---|---|
| No vaciar la tabla en las etiquetas | 4 FAIL; `propagacion` y `valores` con `-opt all` **no terminan** (el `while` queda con `ifFalse true`) y el runner los corta por tiempo |
| No olvidar las globales en los `call` | 4 FAIL; `propagacion` imprime `5` y `11` en vez de `6` y `12` |

**Runner.** `run_tests.sh` tiene tiempo límite: 10 s para compilar y 5 s para ejecutar cada programa. Un ciclo infinito (en el compilador o en el programa generado) da `FAIL ... no terminó en 5 s` en vez de colgar todos los tests. `BIN` se puede cambiar para correr los tests contra otro compilador.

## Eliminación de código inalcanzable (`-opt deadcode`)

**Qué hace.** Borra del TAC las instrucciones a las que la ejecución nunca puede llegar. Aprovecha las condiciones constantes que dejan el plegado y la propagación (`if (1 > 2)` → `ifFalse false goto L`), y el código escrito después de un `return`.

| Regla | Antes | Después |
| Condición siempre verdadera | `ifFalse true goto L` | se borra (nunca salta) |
| Condición siempre falsa | `ifFalse false goto L` | `goto L` (siempre salta) |
| Código inalcanzable | todo lo que sigue a un `goto` / `return` hasta la próxima etiqueta usada | se borra |
| Salto a la instrucción siguiente | `goto L` seguido de `L:` | se borra el `goto` |
| Etiqueta sin uso | `L:` que ningún `goto`/`ifFalse` usa | se borra |

- `remove_dead_code` (`optimizer.c`) recorre la lista con un puntero al `next` anterior (`struct tac **pp`), así borrar una instrucción es `*pp = i->next`. Un indicador `dead` se activa después de `goto`/`return` y se apaga en una etiqueta **usada** (se puede llegar saltando) o al empezar/terminar una función.
- **Se repite hasta que no cambie nada**
- Una etiqueta sin uso no corta el código inalcanzable: si nadie salta a ella, solo se llega desde arriba.
- `false || e` → queda solo `e`, y `e` se sigue evaluando (si tiene una llamada, se hace).

**Tests.** `tests/cases/test34.txt` 

## Reutilización de slots de temporales (`-opt tempslots`)

**Qué hace.** Sin la optimización, cada temporal tiene su propio slot de 8 bytes en la pila durante toda la función. Pero un temporal vive muy poco (desde que se escribe hasta su último uso). Entonces, un temporal que ya no se usa devuelve su slot a una lista de libres y el siguiente lo reutiliza. 
optmización la memoria porque el frame queda del tamaño del máximo de temporales vivos a la vez.
Importa sobre todo en la recursión, donde cada llamada activa tiene su propio frame.

**Dónde: en el generador de assembly** Se realiza recorriendo el TAC antes de generar el assembly porque es ahí donde se decide la dirección de cada temporal.

**Como.**
1. Vida de cada temporal:desde su primera aparición hasta su último uso 
2. Asignación en orden: en cada instrucción, primero se liberan los slots de los temporales que se usan ahí por última vez, y después los temporales que aparecen por primera vez toman un slot libre (o uno nuevo). Se ibera antes de asignar.                                            

**Casos Especiales**
- **`param t`** el valor se lee recién en el `call` por eso `t` vive hasta el `call`. NO PUEDE PARAR: La generacion de codigo intermedio no lo permite
- **El temporal de `&&`/`||`** se escribe en las dos ramas: solo la primera aparición le asigna slot.
- **Ciclos:** si un salto hacia atrás vuelve a un punto donde un temporal está vivo por los ciclos anidados. NO PUEDE PARAR: La generacion de codigo intermedio no lo permite

**`-debug` muestra los slots.** Con `-target assembly -debug` (o sin `-target`), se lista en `stderr` el slot de cada parámetro, variable y temporal.

**Tests.**
- `tests/asm/tempslots.txt`: muchos temporales vivos a la vez, `param` con llamadas anidadas, el temporal de `&&`/`||`, un ciclo y recursión (`fib(15)`). Corre sin optimizar y con `-opt all`: los valores tienen que dar igual.
- **Marca nueva `// frame: función bytes`:** el runner compila solo con `-opt tempslots` y compara el `enter $bytes` de esa función. Los tests de ejecución verifican que no se rompa nada; este verifica que la pila 

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



## Orden de `constprop` y `deadcode` repetir hasta un punto fijo

**Problema.** Las dos pasadas se habilitan mutuamente: la propagación pierde información en las etiquetas que `deadcode` después borra, y `deadcode` necesita las condiciones constantes que descubre la propagación.
 Ningún orden fijo genera la mejor optimización.

**Solución.** `optimize_tac` repite `constprop` y `deadcode` hasta que una vuelta no cambia nada. Cada pasada avisa con `tac_changed` si modificó una instrucción (reemplazar un operando, calcular una instrucción o borrar una)

**Termina siempre:** ninguna pasada deshace lo que hace la otra (la propagación solo reemplaza variables por constantes; `deadcode` solo borra instrucciones). MAX_ROUNDS (100) es solo una red de seguridad: si se alcanzara, el código igual es correcto, solo menos optimizado.

**Alternativa descartada:** eliminar código muerto también en el AST, después del plegado (`if (true)`, `while (false)`, sentencias después de `return`), para que esas etiquetas nunca lleguen al TAC. Con el punto fijo, solo ahorraría vueltas sin mejorar el resultado, y requiere cuidar las declaraciones dentro de los bloques que se eliminan.

## diferencias con la especificación 

Encontradas al comparar el compilador con `00-TDS-proyecto.pdf` y `01-TDS-spec-lenguaje.pdf`.

- [x] **`_` en identificadores.** La gramática permite `⟨alpha_num⟩ → ⟨alpha⟩ | ⟨digit⟩ | _`, pero el lexer usa `[a-zA-Z][a-zA-Z0-9]*` y `mi_var` da error léxico. Cambiar a `[a-zA-Z][a-zA-Z0-9_]*`.
- [x] **Métodos con retorno que llegan al final sin `return`.** La spec dice que un método que retorna un valor "no puede alcanzar el fin del método". Hoy `int f() { x = 1; }` compila sin error y devuelve 0. Falta un chequeo semántico: que todo camino del cuerpo termine en `return` (un `if` cuenta solo si tiene `else` y las dos ramas retornan).
- [x] **Opción `-opt [optimización]`.** Implementada; ver "Optimizaciones opcionales: opción `-opt`".
- [x] **Salidas `.lex` / `.sint` y extensión `.ctds`.** Con `-target scan`/`parse` la spec pide generar un archivo con extensión según la etapa, y el archivo de entrada debe ser `.ctds`. Hoy no se generan esos archivos y los tests son `.txt`. Consultar con la cátedra si se evalúan.
- [x] **Literales float.** La spec define `digit+ . digit+`, pero el lexer también acepta `1.`, `.5` y exponentes (`1e5`). Documentada como extensión; ver "Extensiones realizadas al lenguaje".
- [x] **Error claro para `float` en la generación de código.** Implementado; ver "`float` en la generación de código: error en lugar de código incorrecto".
- [x] **Literal `-2147483648`.** `2147483648` se rechazaba en el lexer antes de aplicar el `-` unario, así que el mínimo no se podía escribir. Implementado; ver "Literal `-2147483648` (`INT_MIN`)".
- [ ] **`extern` como extensión.**  un programa que use `extern` como identificador deja de compilar.
- [ ] **Documentación entrega final.** 



## Extensiones realizadas al lenguaje

Además de lo que define la gramática de la especificación, el compilador acepta las siguientes construcciones.

### Literales `float` con formato de C

La producción formal de la especificación es `⟨float literal⟩ → ⟨digit⟩ ⟨digit⟩* . ⟨digit⟩ ⟨digit⟩*` (parte entera y fraccionaria obligatorias). Sin embargo, el texto de la spec también dice que "los literales reales son iguales a los utilizados en C". El lexer (`lexer.l`) sigue esa segunda definición y acepta, además de `1.5`:

| Forma | Ejemplo | Regla del lexer |
|---|---|---|
| Sin parte fraccionaria | `86.` | `[0-9]+"."[0-9]*([eE][+-]?[0-9]+)?` |
| Sin parte entera | `.05` | `[0-9]*"."[0-9]+([eE][+-]?[0-9]+)?` |
| Notación científica | `5e10`, `3.6e-8` | sufijo `([eE][+-]?[0-9]+)?` / `[0-9]+[eE][+-]?[0-9]+` |

Es una extensión respecto de la gramática formal, no del análisis semántico: `CONSTFLOAT` se trata igual en todas las etapas.

**Consecuencia conocida.** Al aceptar floats como en C, `1.2.3` no es un error léxico: el lexer reconoce `1.2` y `.3` como dos `CONSTFLOAT` y el error lo reporta el parser como sintáctico (ver `Documentacion-Etapa2.md`). `run_tests.sh` compara esa salida en `test6error`.

**No afecta al back-end.** La generación de código rechaza cualquier programa con `float` con un error explícito (ver "`float` en la generación de código: error en lugar de código incorrecto"), así que la extensión solo se observa en el análisis y en el código intermedio.



## Literal `-2147483648` (`INT_MIN`)

**Problema.** La spec define enteros de 32 bits con signo, así que `-2147483648` es un valor válido. Pero `2147483648` excede `INT_MAX` y el lexer lo rechazaba como "entero fuera de rango" antes de que el parser aplicara el `-` unario, así que el mínimo no se podía escribir.

**Solución.** Se distingue la magnitud `2147483648` y se valida según el contexto:

- **`lexer.l`:** el literal `2147483648` devuelve el token `CONSTINT_MIN` en lugar de ser un error léxico. Las magnitudes mayores siguen dando error léxico (así `test4error` no cambia).
- **`parser.y`:** `literal: CONSTINT_MIN` crea un `NODE_INT` marcado con `is_int_min_mag` (`new_int_min_mag`, `ast.c`).
- **`ast.c`: `resolve_int_min`** recorre el AST después del parseo. Si el nodo marcado es el operando directo del `-` unario, lo convierte en el literal `INT_MIN`. Funciona también con paréntesis (`-(2147483648)`), porque no crean nodo. Cualquier otro uso (`x = 2147483648;`, `5 - 2147483648`) es un error semántico:
  ```
  Error semántico en la línea 3: entero '2147483648' fuera de rango (solo se admite como -2147483648)
  ```
- **`main.c`:** llama a `resolve_int_min(root)` después de `yyparse()` y antes de las optimizaciones y la generación de código.

**Por qué después del parseo.** El lexer no puede saber si un `-` es unario: en `5 - 2147483648` es una resta binaria y el literal sigue fuera de rango, mientras que en `-2147483648` es el signo del mínimo. La decisión necesita el árbol.

**Tests.**

| Archivo | Qué verifica |
|---|---|
| `tests/asm/intmin.txt` | `-2147483648` compila y vale exactamente `INT_MIN` (`x + 2147483647 == -1`). Corre sin optimizar y con `-opt all` |
| `tests/cases/test36error.txt` | `2147483648` suelto y en una resta binaria dan dos errores semánticos |
| `tests/cases/test4error.txt` | Sin cambios: `99999999999` sigue siendo error léxico |



## Correcciones a etapas anteriores y salidas de `scan` y `parse`
 
Se cerraron tres diferencias entre el compilador y la especificación (gramática del lenguaje y tabla de opciones de línea de comandos).
 
### 1. Guion bajo en identificadores (lexer, etapa 1)
 
**Problema.** La gramática define `⟨alpha num⟩ → ⟨alpha⟩ | ⟨digit⟩ | _`, pero el lexer aceptaba solo `[a-zA-Z][a-zA-Z0-9]*`. Un programa válido con `mi_var` o `es_par` daba error léxico en el `_` y, a continuación, un error sintáctico por el nombre partido en dos.
 
**Solución (`lexer.l`).** La regla de identificadores pasa a ser:
```
[a-zA-Z][a-zA-Z0-9_]*
```

### 2. Métodos con valor que alcanzan el fin sin `return` (semántico, etapa 2)
 
**Problema.** La especificación dice que un método que retorna un valor no puede alcanzar el fin de su cuerpo: solo retorna con una sentencia `return`.
 
**Solución (`ast.c`).** `new_method_decl` verifica que el cuerpo retorne en todos los caminos y, si no, reporta con `sem_error`:
 
```
Error semántico en la línea N: el método 'f' debe retornar un valor en todos los caminos
```
 
La decisión la toman `block_returns` y `list_returns`, que recorren las sentencias de un bloque en orden y devuelven 1 en cuanto encuentran una que garantiza el retorno:
 
```c
int a() { int x; x = 1; }                                      // error
int b(boolean c) { if (c) { return 1; } else { return 2; } }   // ok
int d(boolean c) { if (c) { return 1; } }                      // error
int f() { { return 1; } }                                      // ok
```
 
**Decisiones y casos borde.**
- **Los métodos `void` y los `extern` no se chequean.** Un `void` puede terminar sin `return`. Un `extern` no tiene cuerpo (el puntero llega en `NULL`), por lo que la condición incluye `body` y `!sym->is_extern`. 

### 3. Salidas de `-target scan` y `-target parse` 
 
**Problema.** La especificación indica que, sin `-target`, la compilación genera el archivo de la etapa o el ejecutable, y que `-target <etapa>` corta la compilación en esa etapa. `scan` y `parse` no generaban ningún archivo: compartían el destino `T_FRONT` y solo analizaban.
 
**Solución (`main.c`).** `T_FRONT` se separa en `T_SCAN` y `T_PARSE`:

| `-target scan` | 
Solo análisis léxico: recorre los tokens con `yylex()`, sin pasar por el parser 
Salida -> `nombre.lex`: un lexema por línea, `línea N: texto` |

| `-target parse` | 
Análisis sintáctico y semántico completos 
Salida -> `nombre.sint`: una línea, `Análisis sintáctico exitoso` |
 
- **`.gitignore`:** se agregaron `*.lex` y `*.sint`.

**Decisiones.**
- **No se valida la extensión `.ctds`.** El compilador acepta cualquier nombre de entrada y nombra las salidas con el archivo sin su extensión (`x.ctds` produce `x.lex`, `x.sint`, `x.ci`, `x.ass`, `x.out`). Validarla rompería los tests existentes, que usan `.txt`, y los archivos `.ctds` funcionan igual.
- **`.lex` lista el lexema y su línea, no el nombre del token.** La especificación no define el contenido. El lexema alcanza para ver qué reconoció el scanner, y el nombre del token exigiría tocar cada regla de `lexer.l`.
- **`.sint` solo informa el resultado.** La etapa 1 permite que la salida del parser sea el árbol "o simplemente si la entrada es correcta o no". Volcar el árbol exige implementar `print_ast`, que hoy está vacío.
