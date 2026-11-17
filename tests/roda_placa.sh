#!/bin/sh
# monta cada programa de tests/placa/ pro Uno e roda no simavr.
# A serial tem que sair igual ao .esperado do PC
BIN="$1"
AQUI="$(dirname "$0")"
if ! command -v avr-gcc > /dev/null 2>&1 || ! command -v simavr > /dev/null 2>&1 ||
   { ! command -v llc-18 > /dev/null 2>&1 && ! command -v llc > /dev/null 2>&1; }; then
    echo "PLACA: avr-gcc, simavr ou llc nao encontrado. Os testes da placa NAO rodaram."
    exit 0
fi
TMP="${TMPDIR:-/tmp}/zl_placa_$$"
mkdir -p "$TMP"
ok=0
falhas=0
for f in "$AQUI"/placa/*.mac; do
    [ -e "$f" ] || continue
    base="${f%.mac}"
    nome=$(basename "$base")
    if ! sh "$AQUI/../scripts/placa.sh" "$f" "$BIN" > "$TMP/monta.txt" 2>&1; then
        falhas=$((falhas+1))
        echo "FALHOU: $f (nao montou pro Uno)"
        head -6 "$TMP/monta.txt"
        continue
    fi
    sh "$AQUI/../scripts/simula.sh" "build/placa/$nome.elf" > "$TMP/serial"
    tr -d '\r' < "$base.esperado" > "$TMP/esperado"
    [ -e "$base.erro" ] && tr -d '\r' < "$base.erro" >> "$TMP/esperado"
    if cmp -s "$TMP/esperado" "$TMP/serial"; then
        ok=$((ok+1))
    else
        falhas=$((falhas+1))
        echo "FALHOU: $f (serial diferente do esperado)"
        diff "$TMP/esperado" "$TMP/serial" | head -8
    fi
done
rm -rf "$TMP"
echo "PLACA: $ok caso(s) corretos no simavr, $falhas falha(s)"
[ "$falhas" -eq 0 ]
