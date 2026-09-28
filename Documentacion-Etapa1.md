# Documentación — Etapa 1: Análisis Léxico y Sintáctico

## 16/09/2026 (Cami)

Se reescribió la gramática del lenguaje C-TDS en `parser.y`. Se quitó el código de generación del AST porque no forma parte de esta etapa.

### Decisiones de diseño
- **Construcción while con ( )** se define WHILE '(' expr ')' block. (Consultado en clase).
- **Renombres de tokens** `CONST` → `CONSTINT`, `CONSTF` → `CONSTFLOAT`.
- **Regla de recuperación de errores** (`| error ';' { yyerrok; }` en `statement`): no
  está en la especificación, pero se mantiene de la especificación anterior para poder reportar más de un error sintáctico por corrida.

### Conflictos conocidos (bison)
Al compilar (`bison -d parser.y`), aparecen 36 conflictos shift/reduce por la ambiguedad de `expr`.

### Pendiente
- Agregar `%left`/`%right`/`%prec` para desambiguar `expr`.
- Resolver comentarios // y /* */. (`lexer.y`)
- Modificar `lexer.y`.


## 18/09/2026 (Victoria)

Se resolvió la precedencia y asociatividad de operadores en `parser.y`.

### Problema
La gramática definía `expr: expr bin_op expr` usando las reglas intermedias `bin_op`, `arith_op`, `rel_op` y `cond_op`. Bison no puede aplicar declaraciones de precedencia a no-terminales, solo a tokens. Esto generaba conflictos shift/reduce porque la gramática era ambigua para expresiones como `1 + 2 * 3`.

### Solución aplicada
1. **Se eliminaron** las reglas `bin_op`, `arith_op`, `rel_op` y `cond_op`.
2. **Se reescribió** la regla `expr` con cada operador inline (`expr '+' expr`, `expr '*' expr`, etc.).
3. **Se agregaron** declaraciones de precedencia y asociatividad antes de `%%`, en orden ascendente de prioridad.
4. Los operadores unarios `-` y `!` usan `%prec UMINUS` / `%prec UNOT` para tener mayor prioridad que los binarios.


## 18/09/2026 (Trini)

Se modifica el archivo `lexer.y`.

### Modificaciones

Se agregan palabras reservadas y operadores nuevos en las reglas del lexer.
Se quita la palabra reservada `main` que no aparece en palabras reservadas.
Se agregar operadores, y nuevos tipos de comentarios.
Se hace chequeo en los límites de literales enteros.
Distintas opciones de literales flotantes. Se aceptan como literales flotantes expresiones como "2.15" ".05" "86." "5e10" "3.6e-8"

Modifico y comento lineas del main para poder usar con los cambios realizados 

### Pendiente

Consultar si es necesario chequear los limites en literales flotantes.
Crear nuevos test que contemplen las nuevas reglas.
Crear archivo para correr los test juntos
Consultar si hacemos print de los tokens en el lexer (para comprobar que reconocemos los tokens correctos)

## 23/09/2026 -en clase-

Resolución de conflictos con bison

### Test con errores sintácticos
Se resuelven conflictos de bison. Se modifica el orden en la recursiones para evitar ambigüedad y conflictos shift/reduce.

## 23/09/2026 (Cami)
- Se agrego impresion de numero de linea en errores. 
- Se reemplazó por una lista de ítems que pueden ser declaración o sentencia, en cualquier orden. Antes se exigía al menos una declaración por bloque y todas las declaraciones antes de las sentencias, por lo que `void main() { x = 1; }` o `void main() { x = 1; int y; }` daban error.
- Se agregó %define parse.error verbose en `parser.y`. Ahora el mensaje indica token no esperado.
- Se automatizaron los tests: los archivos de prueba pasaron a `tests/cases/` (entradas) y `tests/expected/` (salidas esperadas).
 `test4error`, `test5error` y `test7error` guardan la salida correcta pero fallan porque falta corregir cada caso. `test6error` **ver**

### Pendiente
- Recuperación de errores en cascada tras un `;` faltante. Caso (test7error.txt).
- Ver errores Lexicos:
1. **Entero fuera de rango** (test4error.txt, x = 99999999999; en la línea 3). imprime Error: entero fuera de rango: 99999999999 y "Análisis sintáctico exitoso".
2. **Comentario `/*` sin cerrar** (test5error.txt) no se detecta como error léxico.
3. **Literales mal formados** (test6error.txt). `1.2.3`no se detecta como error lexico. Esta bien?


## 28/09/2026 (Cami)
Se corrigieron los pendientes 1 y 2. 

- **Entero fuera de rango** (test4error): ahora suma lexical_errors++.
- **Comentario `/*` sin cerrar** (test5error): se agregó una regla `"/*"` en `lexer.l`. `"/*"` solo matchea cuando no hay cierre.
- **; faltante** (test7error): la regla `error ';'` descartaba tokens hasta el próximo `;`.

### Pendiente
1. **Literales mal formados** (test6error.txt). `1.2.3`no se detecta como error lexico. Deberia dar error lexico?

