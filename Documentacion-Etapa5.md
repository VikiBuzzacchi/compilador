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

## Pendientes

- **Plegado y propagación de constantes.**
- **Eliminación de código inalcanzable** (después de `return`/`goto`, ramas de `if (false)`).
- **Reutilización de slots de temporales** (lista de offsets libres).
- **`float` no soportado** en el generador de assembly (los literales se cargan como 0).
