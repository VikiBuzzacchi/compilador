#!/bin/sh
# Corre cada tests/cases/*.txt con ./compilador y compara stdout+stderr con tests/expected/<nombre>.expected
# Cada test pasa (PASS) o falla (FAIL). Termina con código distinto de 0 si alguno falla.

cd "$(dirname "$0")" || exit 1
BIN=./compilador
[ -x "$BIN" ] || { echo "No existe $BIN. Ejecutar 'make' primero."; exit 1; }

pass=0; fail=0
tmp=$(mktemp)
expected_out=$(mktemp)
trap 'rm -f "$tmp" "$expected_out"' EXIT

for input in tests/cases/*.txt; do
    name=$(basename "$input" .txt)
    expected="tests/expected/$name.expected"

    # -debug: sin esa opción el compilador no imprime el mensaje de éxito
    "$BIN" -debug < "$input" > "$tmp" 2>&1

    if [ ! -f "$expected" ]; then
        echo "FAIL   $name (falta $expected)"; fail=$((fail+1))
    elif diff -q "$tmp" "$expected" > /dev/null; then
        echo "PASS   $name"; pass=$((pass+1))
    else
        echo "FAIL   $name"; diff "$expected" "$tmp" | sed 's/^/         /'; fail=$((fail+1))
    fi
done

# Código intermedio (etapa 3): compara ./compilador -target codinter con <nombre>.ci.expected
for expected in tests/expected/*.ci.expected; do
    name=$(basename "$expected" .ci.expected)
    "$BIN" -target codinter -debug < "tests/cases/$name.txt" > "$tmp" 2>&1

    if diff -q "$tmp" "$expected" > /dev/null; then
        echo "PASS   $name (codinter)"; pass=$((pass+1))
    else
        echo "FAIL   $name (codinter)"; diff "$expected" "$tmp" | sed 's/^/         /'; fail=$((fail+1))
    fi
done

# Código objeto (etapa 4): compila y ejecuta cada tests/asm/*.txt. Se chequea:
#   "// exit: N"       el exit code
#   "// expect: valor" la salida, una línea por valor y en orden (impresa con printInt/printBool);
#                      puede ir al final de la línea del print: printInt(x);  // expect: 3
#   "// error: mensaje" la compilación tiene que fallar, mostrar ese mensaje y no generar el .ass
# Un test sin ninguna de las tres no se chequea.
# Cada test corre dos veces: sin optimizar y con -opt all. Una optimización no puede cambiar el resultado.
for mode in "" "-opt all"; do
label="asm${mode:+ $mode}"
for input in tests/asm/*.txt; do
    name=$(basename "$input" .txt)
    want=$(sed -n 's|^// exit: *\([0-9]*\).*|\1|p' "$input" | head -1)
    sed -n 's|.*// expect: *||p' "$input" > "$expected_out"
    errmsg=$(sed -n 's|^// error: *||p' "$input" | head -1)
    [ -n "$want" ] || [ -s "$expected_out" ] || [ -n "$errmsg" ] || continue

    links=$(sed -n 's|^// link: *||p' "$input")      # archivos C que definen los extern

    # Test de error: no tiene que compilar
    if [ -n "$errmsg" ]; then
        rm -f "$name.ass"
        if "$BIN" $mode "$input" $links -o "$tmp.out" > "$tmp" 2>&1; then
            echo "FAIL   $name ($label): compiló, se esperaba el error \"$errmsg\""; fail=$((fail+1))
        elif ! grep -qF "$errmsg" "$tmp"; then
            echo "FAIL   $name ($label): falló sin el mensaje \"$errmsg\""; sed 's/^/         /' "$tmp"; fail=$((fail+1))
        elif [ -f "$name.ass" ]; then
            echo "FAIL   $name ($label): dio el error pero generó $name.ass"; fail=$((fail+1))
        else
            echo "PASS   $name ($label)"; pass=$((pass+1))
        fi
        rm -f "$tmp.out" "$name.ass"; continue
    fi

    exe=$(mktemp)
    if ! "$BIN" $mode "$input" $links -o "$exe" > "$tmp" 2>&1; then
        echo "FAIL   $name ($label: no compila)"; sed 's/^/         /' "$tmp"; fail=$((fail+1))
        rm -f "$exe" "$name.ass"; continue
    fi
    "$exe" > "$tmp"; got=$?
    rm -f "$exe" "$name.ass"

    if [ -n "$want" ] && [ "$got" -ne "$want" ]; then
        echo "FAIL   $name ($label): exit $got, esperado $want"; fail=$((fail+1))
    elif [ -s "$expected_out" ] && ! diff -q "$expected_out" "$tmp" > /dev/null; then
        echo "FAIL   $name ($label): salida distinta (< esperado, > obtenido)"
        diff "$expected_out" "$tmp" | sed 's/^/         /'; fail=$((fail+1))
    else
        echo "PASS   $name ($label)"; pass=$((pass+1))
    fi
done
done

echo
echo "PASS: $pass  FAIL: $fail"
[ "$fail" -eq 0 ]
