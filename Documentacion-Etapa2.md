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

