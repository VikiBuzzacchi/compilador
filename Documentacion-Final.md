# Documentación Final — Compilador C-TDS

Taller de Diseño de Software (Cod. 3306) — 2026.

Resumen del proyecto completo. Cada punto remite a la documentación de su etapa, donde están el detalle, los ejemplos y los tests:

| Etapa | Archivo |
|---|---|
| 1. Análisis léxico y sintáctico | [Documentacion-Etapa1.md](Documentacion-Etapa1.md) |
| 2. Tabla de símbolos, AST y análisis semántico | [Documentacion-Etapa2.md](Documentacion-Etapa2.md) |
| 3. Código intermedio (TAC) | [Documentacion-Etapa3.md](Documentacion-Etapa3.md) |
| 4. Código objeto x86-64 | [Documentacion-Etapa4.md](Documentacion-Etapa4.md) |
| 5. Optimizaciones y correcciones finales | [Documentacion-Etapa5.md](Documentacion-Etapa5.md) |

Uso, build y tests: [README.md](README.md).

## Arquitectura

```
fuente .ctds
   │  lexer.l              tokens (Flex)
   ▼
parser.y                   gramática (Bison) + construcción del AST
   │                       + tabla de símbolos + chequeos semánticos
   ▼
AST tipado ──► optimize_ast (constfold)                    optimizer.c
   ▼
build_tac ──► lista de TAC ──► optimize_tac (constprop + deadcode)
   │                               │
   │                               └─► .ci / .opt          -target codinter
   ▼
gen_object_code (tempslots) ──► .ass ──► gcc ──► .out      object_code.c
```

| Archivo | Responsabilidad |
|---|---|
| `lexer.l` | Tokens, comentarios, rango de enteros, símbolos inválidos |
| `parser.y` | Gramática, precedencia, recuperación de errores, declaración de símbolos y apertura/cierre de niveles |
| `symbol_table.c/.h` | Pila de niveles; cada nivel es una lista de símbolos |
| `ast.c/.h` | Nodos del AST y chequeos semánticos al construir cada nodo |
| `intermediate_code.c`, `tac.h` | AST → lista de instrucciones TAC; impresión del `.ci` |
| `optimizer.c/.h` | Opción `-opt` y las pasadas de optimización |
| `object_code.c/.h` | TAC → assembly x86-64 (AT&T, System V) |
| `main.c` | CLI (`-target`, `-o`, `-opt`, `-debug`) y orquestación de las fases |

## 1. División del trabajo

| Etapa | Integrantes |
|---|---|
| 1 | Camila: gramática en `parser.y`, números de línea en errores, tests automáticos. Victoria: precedencia y asociatividad. Trinidad: lexer. |
| 2 | Camila: AST y conexión del parser con la tabla de símbolos. Trinidad: tabla de símbolos, chequeo de `kind`, cantidad de argumentos, `main`, corrección de tests. (Doc. Etapa 2) |
| 3 | Victoria |
| 4 | Trinidad |
| 5 | Camila: correcciones previas (32 bits, Makefile, tests de assembly), `extern` y optimizaciones. Trinidad: corrección guion bajo, retorno en todos los caminos, salidas `scan`/`parse`. Victoria: literal `INT_MIN`, documentación de extensiones, nombres únicos en el TAC |

## 2. Decisiones, asunciones y extensiones

**Decisiones y asunciones**

- **`while` con paréntesis** (`while (expr) block`), consultado en clase (Doc. Etapa 1).
- **Declaraciones y sentencias en cualquier orden** dentro de un bloque (Doc. Etapa 1).
- **Recuperación de errores sintácticos** (`error ';'`, `type id_list error`) para reportar más de un error por corrida (Doc. Etapa 1 y 2).
- **Parámetros y variables del cuerpo comparten el nivel del método**: redeclarar un parámetro en el cuerpo es error (Doc. Etapa 2).
- **Variables y métodos comparten el espacio de nombres**: una variable local puede ocultar un método y llamarla da `'f' no es un método` (Doc. Etapa 2).
- **`int` ↔ `float`** (regla 14) en asignaciones, argumentos, `return` y `==`. Una operación aritmética con un operando `float` da `float`.
- **Un método con valor debe retornar en todos los caminos**: un `if` cuenta solo si tiene `else` y las dos ramas retornan (Doc. Etapa 5).
- **`int` de 32 bits con wrap**, igual que C, tanto al ejecutar como al plegar constantes (Doc. Etapa 5).
- **`float` no tiene back-end**: se analiza y llega al TAC, pero generar assembly da un error explícito en vez de código incorrecto (Doc. Etapa 5).
- **No se valida la extensión `.ctds`**, para mantener los tests en `.txt` (Doc. Etapa 5).

**Extensiones al lenguaje** (Doc. Etapa 5)

- **Literales `float` como en C**: `86.`, `.05`, `5e10`. Consecuencia: `1.2.3` es un error sintáctico y no léxico.
- **Métodos `extern`**: `int f(int x) extern;` declara una función definida en C, que se enlaza con `c-tds prog.ctds lib.c`. `extern` pasa a ser palabra reservada.
- **Literal `-2147483648`**: `2147483648` solo se acepta como operando del `-` unario (Doc. Etapa 5).

## 3. Diseño y decisiones clave

**Análisis semántico durante el parseo.** Cada nodo se crea con su tipo ya calculado y cada identificador resuelve su símbolo en el momento en que se crea el nodo, mientras su nivel está abierto. Después del parseo los niveles ya se cerraron y no se podría saber a qué `x` se refiere cada uso. Los errores de tipo se propagan como `TYPE_ERROR` para no reportar en cascada (Doc. Etapa 2).

**AST con listas por `next`** en lugar de los nodos `NODE_SEQ` del preproyecto, y un único `NODE_BINOP` con un `op_type` propio para todos los operadores (Doc. Etapa 2).

**TAC como lista de instrucciones.** En la etapa 3 el TAC se escribía como texto. En la etapa 4 pasó a `struct tac` con operandos tipados, que guardan el `struct symbol *`, porque el generador de assembly necesita saber si una variable es global, local o parámetro, y extraerlo del texto era frágil. Las optimizaciones trabajan sobre esa misma lista (Doc. Etapa 4).

**Cortocircuito en el TAC.** La etapa 3 evaluaba los dos operandos de `&&`/`||`. Se corrigió en la etapa 4 con `ifFalse`, `goto` y etiquetas, así el generador de assembly no necesita tratarlos aparte (Doc. Etapa 4).

**Código objeto sin asignación de registros.** Cada variable y cada temporal tiene un slot de 8 bytes en la pila, y cada instrucción TAC se traduce con una plantilla fija. Se eligió por corrección antes que por eficiencia, como recomienda la consigna (Doc. Etapa 4).

**Optimizaciones como pasadas independientes**, no dentro de `new_binop` ni de `gen_expr`. Así se activan por separado con `-opt`, el análisis semántico y sus mensajes no dependen de ellas, y el orden entre pasadas queda explícito en un solo lugar (Doc. Etapa 5).

| Optimización | Sobre | Referencia |
|---|---|---|
| `constfold`: plegado de constantes | AST | Doc. Etapa 5 |
| `constprop`: propagación de constantes | TAC | Doc. Etapa 5 |
| `deadcode`: código inalcanzable | TAC | Doc. Etapa 5 |
| `tempslots`: reutilización de slots de temporales | assembly | Doc. Etapa 5 |

**Cambios a etapas anteriores y su motivo**

| Cambio | Motivo | Referencia |
|---|---|---|
| Aritmética normalizada a 32 bits (`movslq`, `movzbq` después de cada `call`) | El overflow difería de C, los `int` negativos devueltos por funciones C llegaban positivos y el plegado tenía que coincidir con la ejecución | Doc. Etapa 5 |
| El `Makefile` depende de todas las fuentes y headers | Modificar `object_code.c` no recompilaba | Doc. Etapa 5 |
| Guion bajo en identificadores | La gramática lo permite y el lexer no lo aceptaba | Doc. Etapa 5 |
| Chequeo de retorno en todos los caminos | `int f() { x = 1; }` compilaba y devolvía 0 | Doc. Etapa 5 |
| Nombres únicos para variables homónimas en el TAC (`x`, `x_1`) | Dos variables de distintos bloques se confundían en el TAC | Doc. Etapa 3 |
| Línea de los errores con `%locations` | Bison lee un token de más y el error salía en la línea siguiente | Doc. Etapa 2 |

## 4. Detalles de implementación interesantes

- **Comentario `/*` sin cerrar**: hay dos reglas en el lexer. La que reconoce el comentario completo es más larga, así que `"/*"` sola solo matchea cuando falta el cierre (Doc. Etapa 1).
- **`INT_MIN`**: el lexer no puede saber si un `-` es unario. Por eso `2147483648` llega como un token especial y `resolve_int_min` decide sobre el AST ya construido (Doc. Etapa 5).
- **Plegado sin reemplazar nodos**: el nodo plegado se transforma en literal en el mismo lugar, así el padre sigue apuntando a él y se conservan su `line` y su `next` (Doc. Etapa 5).
- **`constprop` y `deadcode` hasta un punto fijo**: cada una habilita a la otra, así que se repiten hasta que una vuelta no cambia nada. Siempre termina porque ninguna deshace lo que hace la otra (Doc. Etapa 5).
- **`tempslots`**: calcula la vida de cada temporal y, en cada instrucción, libera los slots que dejan de usarse antes de asignar los nuevos. El frame queda del tamaño del máximo de temporales vivos a la vez (Doc. Etapa 5).
- **Borrado en listas con `struct tac **pp`**: eliminar una instrucción es `*pp = i->next` (Doc. Etapa 5).

## 5. Problemas conocidos

Los 84 tests de `make test` pasan. No hay casos de test que fallen.

| Problema | Cómo se detectó | Alternativa propuesta |
|---|---|---|
| `float` no se compila a assembly (error explícito). | Revisión del generador (los literales se cargaban como 0) y del `.ci`. Pendiente etapa 3 (Doc. Etapa 3) | Marcar conversiones y traducirlas a assembly con registros SSE |
| `constprop` es local: pierde información en las etiquetas | Diseño (Doc. Etapa 5) | Análisis de flujo de datos (reaching definitions) |
| Después de un error sintáctico puede aparecer un error semántico de más ("debe retornar un valor en todos los caminos"), porque la recuperación descartó el `return` | Prueba manual con un `while` mal escrito | No correr ese chequeo en métodos donde hubo un error sintáctico |
| Con un valor inválido, `-target` se ignora y se compila completo | Prueba manual (`-target asembly`) | Reportar "etapa desconocida" y terminar con código 1 |
| El resumen dice "Fallo en el análisis sintáctico" también cuando los errores son solo semánticos | Salida de los tests de error | Cambiar el encabezado según el tipo de error |
| No se valida que la entrada sea `.ctds` ni que no empiece con `-` | Comparación con la consigna | Validar en `main.c` y renombrar los tests |
| `extern`: hay que declararlo antes de usarlo, las rutas con espacios no se citan y solo se admiten `int`/`boolean`/`void` | Doc. Etapa 5 | Citar los argumentos del comando de enlace |
| `print_ast` está vacío y `.sint` solo informa si el análisis fue exitoso | — | Volcar el AST con indentación |
| El intérprete del preproyecto no se adaptó al lenguaje completo. Está guardado en `preproyecto/interprete.c` | — | Consultar con la cátedra |

## Testing

`make test` (`run_tests.sh`) corre 84 casos:

- **Análisis** (`tests/cases/`, 38 programas, 23 con errores): compara stdout+stderr con `tests/expected/<nombre>.expected`.
- **TAC** (11 `.ci.expected`) y **TAC optimizado** (4 `.opt.expected`, cada uno solo con las optimizaciones que indica su línea `// opt:`).
- **Ejecución** (`tests/asm/`, 15 programas): ensambla, ejecuta y verifica `// exit:`, `// expect:` (salida de `printInt`/`printBool`), `// error:` y `// frame:`. Cada programa corre **sin optimizar y con `-opt all`**, y los dos resultados tienen que coincidir. Los valores esperados se verificaron contra gcc.
- Hay tiempo límite para compilar y ejecutar, así que un ciclo infinito da `FAIL` en vez de colgar el runner.
