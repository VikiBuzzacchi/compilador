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