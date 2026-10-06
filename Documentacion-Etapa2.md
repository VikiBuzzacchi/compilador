# Documentación — Etapa 2: Tabla de Símbolos, AST y Análisis Semántico

Entregas:
- TS y AST: miércoles 30/09/2026.
- Análisis semántico: miércoles 07/10/2026.

## 28/09/2026 (Cami)
Se reescribieron `ast.h`, `ast.c`, y  para el lenguaje completo agregando los nuevos tipos de nodos y funciones para cada uno.

### Decisiones de diseño
- **Modificaciones de node de AST**:     
    1. Se cambió nombre de `node_type` por `node_kind` para evitar confusiones.
    2. Se agrego `third` para la rama else del IF-THEN-ELSE.
    3. Se agregó `line` para los errores semánticos.
    4. Se agregó `value` para los literales `ival` o `fval`.
    5. Se definiieron operadores propios `op_type`.

- **Listas encadenadas por `next`** en vez del nodo `NODE_SEQ` del pre-proyecto. Antes cada sentencia agregaba un nodo extra `NODE_SEQ`, con la sentencia en `left` y el resto de la lista en `right`. Se quitan los nodos intermedios que no representan nada del programa; las listas se recorren con un `for` en vez de recursión.

- **Un solo tipo de nodo para operadores binarios**, en vez de un tipo de nodo por operador. Generaliza el comportamiento de todos los operadores.
- **Operadores  propios en ast** en vez de los tokens de bison.

- **`new_id` y `new_call` resuelven el nombre al crearse** con `search_symbol`. Si no existe, `sym` queda en `NULL` y el error lo reporta el semántico.
    - *Motivo:* la búsqueda tiene que hacerse cuando el nodo se crea, mientras está abierto el nivel donde aparece el identificador. Después del parseo los niveles ya están cerrados y no se sabe qué `x` era cuál. Además, el parser no tiene que repetir la búsqueda en cada regla.

### Pendiente
- VER: como el bloque del método abre su propio nivel, hoy se podría declarar una variable local con el mismo nombre que un parámetro. ERROR?
- VER: ¿se puede llamar a un método declarado más abajo en el archivo?

- Tabla de símbolos y tests nuevos de TS y AST. Corrección de tarea pendiente etapa 1. actualizar readme  (Trini)
- `parser.y`: construir el AST y cargar la TS y main (Victoria)


## 29/09/2026 (Trini)
Se actualizaron los archivos `symbol_table.h` y `symbol_table.c`.

### Tabla de símbolos
Cada nivel guarda una lista de símbolos
- **open_level** apila un nivel
- **close_level** desapila un nivel 
- **insert_symbol** inserta en el nivel actual
- **search_symbol** busca desde nivel más interno al más global

-`struct symbol` tiene un campo nuevo `kind` (`SYM_VAR`, `SYM_PARAM`,`SYM_METHOD`). 
  `insert_symbol` recibe `kind` como tercer parámetro.
        -Para distinguir métodos de variables.
-Nuevos campos `nparams` y `param_types`: guardan cuántos parámetros tiene un método y de qué tipo es cada uno. Todavía no se cargan (valen `0` y `NULL`); se usarán al chequear las invocaciones.
-`insert_symbol` y `search_symbol` ya no imprimen nada: devuelven `NULL` si el nombre ya existe en el nivel actual / si no se encuentra.
    *Motivo:* el mensaje necesita la línea del error y solo la conocen quien crea el nodo (`new_id`, `new_call`) y el parser (`declare`).

### Tarea pendiente etapa 1
`1.2.3` no se detecta como error léxico: los flotantes se definen como en C. El lexer reconoce `1.2` y `.3` como dos `CONSTFLOAT` consecutivos, y cada uno es un token válido. Por eso el error lo detecta el parser y se reporta como **sintáctico**.
 
### Pendiente

- **`parser.y`(Victoria):** conectar con la tabla de símbolos: `declare()` con `kind`, y apertura/cierre de niveles (método, parámetros y bloques).
- **`ast.c` / `ast.h`:** `new_id` y `new_call` deben imprimir `'x' no declarado` con la línea y sumar a `semantic_errors` cuando `sym` es `NULL`. En `ast.h` falta `extern int semantic_errors;`.
- Completar `nparams` y `param_types` al declarar métodos (chequeo de argumentos).
- Verificar que exista `main` sin parámetros.

## Parser: tabla de símbolos y AST (Cami)

- **`%union`** con `int type` (tipo de dato), `struct symbol *sym` y `struct node *ast`. `type` es `int` y no `enum sym_type` porque `lexer.l` incluye `parser.tab.h` y ahí no está visible el enum.
- **`%type`** declara qué campo del `%union` guarda cada regla: `type` → `type`, `method_head` → `sym`, el resto de las reglas con nodos → `ast`.
- **Declaraciones:** `var_decl` declara cada ID con `declare()` usando el tipo de `type`. `id_list` solo crea nodos con el nombre (`name_node`), porque no conoce el tipo. `param_list_ne` declara los parámetros como `SYM_PARAM`.
- **Métodos:** `method_head` (`type ID` o `VOID ID`) declara el método en el nivel global como `SYM_METHOD` y abre el nivel del método. `method_decl` arma el nodo y cierra el nivel. Parámetros y variables del cuerpo comparten ese nivel.
- **Bloques:** `block` abre y cierra su propio nivel. Lo usan `if`, `else`, `while` y los bloques sueltos.
- **`new_method_decl`** ahora recibe los parámetros: `new_method_decl(sym, params, body, line)`. Los parámetros quedan en `right` del nodo.
- **`main.c`** llama a `init_symbol_table() y cuenta errores semanticos.
- **Errores semánticos:** se reporta `'x' ya declarado` con la línea y suma a `semantic_errors`. `semantic_errors` se define en `parser.y`.


### Pendiente
- Identificadores no declarados: `new_id` y `new_call` dejan `sym = NULL`, pero todavía no se reporta `'x' no declarado`. Hay que agregarlo en `ast.c` con `semantic_errors++`.
- `nparams` y `param_types`:*siguen en 0 y `NULL`. Hay que completarlos en `method_head`/`method_decl` para chequear llamadas.
- Llamadas a métodos declarados más abajo: con la implementación actual no se pueden hacer, porque el método se inserta al parsear su cabecera. Decidir si se hace una pasada previa o se chequea en el semántico.
- `print_ast`: está vacío. Sin él no se puede ver el árbol.
- Números de línea: `cur_line` es la línea del último token leído, que puede ser el token siguiente por lookahead. Para la línea exacta haría falta `%locations`.
- Recuperación de errores: si hay un error sintáctico dentro del cuerpo de un método, el nivel podría quedar sin cerrar.
- Tests:
- Actualizar Readme
- Archivos generados: `parser.tab.c`, `parser.tab.h` y `lex.yy.c` están versionados y quedaron desactualizados. Se regeneran con `make`.




