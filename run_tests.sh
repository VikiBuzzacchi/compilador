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

    "$BIN" < "$input" > "$tmp" 2>&1

    if [ ! -f "$expected" ]; then
        echo "FAIL   $name (falta $expected)"; fail=$((fail+1))
    elif diff -q "$tmp" "$expected" > /dev/null; then
        echo "PASS   $name"; pass=$((pass+1))
    else
        echo "FAIL   $name"; diff "$expected" "$tmp" | sed 's/^/         /'; fail=$((fail+1))
    fi
done

echo
echo "PASS: $pass  FAIL: $fail"
[ "$fail" -eq 0 ]
