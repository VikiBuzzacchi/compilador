# Documentación — Etapa 4: Generador de Código Objeto (Trini)

Entrada: lista de instrucciones de Código de Tres Direcciones (TAC) construida a partir del AST validado. Salida: assembly x86-64 (sintaxis AT&T), `nombre.ass`, y el ejecutable `nombre.out`.


## Generación de código objeto
- Generador de código objeto (`object_code.h/.c`), representación del TAC en lista (`tac.h` y refactor de `intermediate_code.c`), cambios en `main.c`, `Makefile`.

## Decisiones de diseño, asunciones y aclaraciones

- **Destino:** x86-64, sintaxis AT&T de GNU as.
- **Todo en memoria, sin asignación de registros.** Cada variable local, parámetro y temporal tiene un slot de 8 bytes en la pila (`-8(%rbp)`, `-16(%rbp)`, ...). Cada instrucción TAC carga operandos en `%r10`/`%r11`, opera y guarda el resultado en el slot.
- **Enteros y booleanos como valores de 64 bits** (`boolean` = 0/1). 
- **Convención de llamadas System V:** los 6 primeros argumentos en `%rdi, %rsi, %rdx, %rcx, %r8, %r9`; el resto en la pila, en orden inverso. El resultado vuelve en `%rax`. Antes de cada `call` se pone `%rax = 0` (necesario para funciones variádicas como `printInt`).
- **Etiquetas locales** con prefijo `.` (`.L0`), para que no choquen con funciones o globales del usuario.
- **Variables globales** en `.bss`, inicializadas en 0 y accedidas con `nombre(%rip)`. Se detectan recorriendo el TAC: solo se emiten las que alguna instrucción usa.
- **Retorno implícito:** al final de cada función se emite `movq $0, %rax; leave; ret`, para los métodos `void` que terminan sin `return`.
- **Salida:** `-target assembly` genera `nombre.ass` y además lo ensambla con `gcc -x assembler` para obtener `nombre.out` (`-o` lo renombra). 
- **Cortocircuito de `&&` y `||`:** la consigna lo exige. Se resuelve en el TAC con `ifFalse`, `goto` y etiquetas; el generador de assembly no necesita saber nada especial.

### Cambios realizados a `intermediate_code.c`

Para poder generar código objeto fue necesario modificar el generador de TAC. Ninguno de los cambios altera el formato impreso del `.ci`, salvo el cortocircuito.
1. **El TAC pasa de texto a una lista de instrucciones (`tac.h`).**
   La etapa 3 escribía cada instrucción como texto directo a un `FILE *`. El generador de assembly necesita conocer, para cada instrucción, su operación y el tipo de cada operando, y recuperar eso desde texto como `t1 = -a` o `ifFalse t0 goto L1` es frágil. Ahora `gen_expr` y `gen_stmt` agregan una `struct tac` a una lista (`emit`), `build_tac` devuelve la lista y `print_tac` la imprime con el formato anterior. `gen_intermediate_code` conserva su firma (`build_tac` + `print_tac`), por lo que `main.c` no necesitó cambios en su uso.
2. **Los operandos guardan su `struct symbol *` y su tipo.**
   `gen_expr` devuelve un `struct operand` (literal, variable o temporal) en lugar de un texto. El `symbol *` permite al generador de assembly saber si una variable es global, local o parámetro, sin depender del nombre. 
3. **`func begin` guarda el símbolo y la lista de parámetros del método.**
   El prólogo en assembly necesita copiar `%rdi`, `%rsi`, ... a los slots de los parámetros, y esa información no estaba en el TAC textual.
4. **Evaluación de cortocircuito de `&&` y `||`.**
   La semántica del lenguaje exige que el segundo operando no se evalúe si el primero determina el resultado. Ahora `a && b` y `a || b` se traducen con `ifFalse`, `goto` y etiquetas, asignando el resultado a un temporal.

### Estructura del generador (`object_code.c`)

1. Recorre el TAC y emite las globales usadas en `.bss`.
2. Por cada `func begin`, recorre las instrucciones hasta `func end` y asigna slots: primero los parámetros, luego el resto de las variables y por último los temporales. El tamaño del frame se redondea a múltiplo de 16 para mantener la pila alineada en los `call`.
3. Emite `enter $N, $0` y copia los parámetros recibidos (registros o pila) a sus slots.
4. Traduce cada instrucción con una plantilla fija.

| TAC                      | Assembly                                                   |
|--------------------------|------------------------------------------------------------|
| `x = a`                  | cargar `a` en `%r10`, guardar en `x`                       |
| `+ - *`                  | `addq` / `subq` / `imulq` sobre `%r10`, `%r11`             |
| `/` y `%`                | `cqto` + `idivq`; cociente en `%rax`, resto en `%rdx`      |
| `< > ==`                 | `cmpq` + `setl/setg/sete` + `movzbq` (resultado 0/1)       |
| `&&` y `\|\|` sobre 0/1  | `andq` / `orq` (el cortocircuito ya resuelto en el TAC)    |
| `-a` / `!a`              | `negq` / `xorq $1`                                         |
| `ifFalse c goto L`       | `cmpq $0` + `je .L`                                        |
| `goto L` / `L:`          | `jmp .L` / `.L:`                                           |
| `param` + `call f, n`    | argumentos a registros o pila, `%rax = 0`, `call f`, resultado de `%rax` al temporal |
| `return e`               | `e` en `%rax`, `leave`, `ret`                              |



### Problemas conocidos y pendientes

- **`float` no soportado** en el generador de assembly (los literales se cargan como 0). 
- **Aritmética de 64 bits**: un cálculo que desborda 32 bits no hace wrap como el `int` de C.
- **Comportamiento por defecto sin `-target`:** hoy no genera aún el ejecutable; `-debug` está parseado pero todavía no controla las impresiones.
- **Falta un script de test para assembly** que ensamble, ejecute y compare el código de salida. 





