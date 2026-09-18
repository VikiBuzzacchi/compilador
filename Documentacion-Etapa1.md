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

