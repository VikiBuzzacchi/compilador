#!/bin/sh
# Corre cada tests/cases/*.txt con ./compilador y compara stdout+stderr con tests/expected/<nombre>.expected
# Cada test pasa (PASS) o falla (FAIL). Termina con código distinto de 0 si alguno falla.

cd "$(dirname "$0")" || exit 1
BIN=./compilador
[ -x "$BIN" ] || { echo "No existe $BIN. Ejecutar 'make' primero."; exit 1; }

pass=0; fail=0
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT

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

# Código objeto (etapa 4): compila, ejecuta y compara el exit code con "// exit: N" (primera línea)
for input in tests/asm/*.txt; do
    name=$(basename "$input" .txt)
    want=$(sed -n 's|^// exit: *\([0-9]*\).*|\1|p' "$input" | head -1)
    [ -n "$want" ] || continue                     # sin "// exit:" no se chequea

    exe=$(mktemp)
    if ! "$BIN" "$input" -o "$exe" > "$tmp" 2>&1; then
        echo "FAIL   $name (asm: no compila)"; sed 's/^/         /' "$tmp"; fail=$((fail+1))
        rm -f "$exe" "$name.ass"; continue
    fi
    "$exe"; got=$?
    rm -f "$exe" "$name.ass"

    if [ "$got" -eq "$want" ]; then
        echo "PASS   $name (asm)"; pass=$((pass+1))
    else
        echo "FAIL   $name (asm): exit $got, esperado $want"; fail=$((fail+1))
    fi
done

echo
echo "PASS: $pass  FAIL: $fail"
[ "$fail" -eq 0 ]
