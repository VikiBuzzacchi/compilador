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

## Pendientes

- **`extern`**: palabra reservada, regla en el parser, saltear el cuerpo en el TAC y enlazar archivos C extra desde `main.c`.
- **Test de funciones C externas:** la normalización del resultado de `call` se probó a mano. Cuando exista `extern`, agregar un test que enlace una función C que devuelva un negativo.
- **Plegado y propagación de constantes.**
- **Eliminación de código inalcanzable** (después de `return`/`goto`, ramas de `if (false)`).
- **Reutilización de slots de temporales** (lista de offsets libres).
- **Tests de llamadas con parámetros que son expresiones.**
- **`float` no soportado** en el generador de assembly (los literales se cargan como 0).
