# Documentación — Etapa 2: Tabla de Símbolos, AST y Análisis Semántico

Entregas:
- TS y AST: miércoles 30/09/2026.
- Análisis semántico: miércoles 07/10/2026.

## 28/09/2026 (Cami)
Se reescribieron `ast.h`, `ast.c`, y  para el lenguaje completo agregando los nuevos tipos de nodos y funciones para cada uno. Cada nivel de la pila de niveles, es una lista de símbolos en orden de declaración: se agrego a  `symbol_table.h` las cabeceras de los metodos que ya estaban implentados en `symbol_table.c`

### Decisiones de diseño
- **Los errores no los imprime la Tabla de simbolos.**

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
- Consultar: como el bloque del método abre su propio nivel, hoy se podría declarar una variable local con el mismo nombre que un parámetro. ¿Tiene que ser error?
- Consultar: ¿se puede llamar a un método declarado más abajo en el archivo? Hoy `new_call` deja `sym` en `NULL` en ese caso.
- `print_symbol_table` desalinea las columnas con palabras acentuadas ("método", "parámetro").
- El `Makefile` no recompila cuando cambia un `.h`. Después de modificar un header, correr `make clean && make`.


## Reparto de tareas

| Quién | Tareas |
| Cami | Headers (`ast.h`, `symbol_table.h`), tabla de símbolos (`symbol_table.c`), constructores del AST (`ast.c`), correcciones de la Etapa 1 (test4error, test5error, test7error). **Hecho.** |
| Victoria | `parser.y`: construir el AST y cargar la TS |
| Trini | Tabla de símbolos: completar `print_symbol_table` (alinear columnas con acentos), `print_ast` en `ast.c`, tests nuevos de TS y AST. |

