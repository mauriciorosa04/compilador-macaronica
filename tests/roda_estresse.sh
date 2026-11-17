#!/bin/sh
# entradas enormes geradas na hora. Cada caso diz o codigo esperado
# em "# espera: N" e opcoes extras em "# args: ..."
BIN="$1"
DIR="$(dirname "$0")/estresse"
TMP="${TMPDIR:-/tmp}/zl_estresse_$$.mac"
LIMITE=60
if command -v timeout > /dev/null 2>&1; then
    T="timeout $LIMITE"
else
    T=""
fi
ok=0
falhas=0
for g in "$DIR"/*.sh; do
    [ -e "$g" ] || continue
    quer=$(sed -n 's/^# espera: *\([0-9][0-9]*\).*/\1/p' "$g" | head -1)
    extra=$(sed -n 's/^# args: *\(.*\)$/\1/p' "$g" | tr -d '\r' | head -1)
    extra=$(echo "$extra" | sed "s|@SAIDA|$TMP.ll|")
    sh "$g" > "$TMP"
    $T "$BIN" $extra "$TMP" > /dev/null 2>&1
    st=$?
    if [ "$st" -eq "${quer:-0}" ]; then
        ok=$((ok+1))
    else
        falhas=$((falhas+1))
        echo "FALHOU: $g (saiu $st, esperava ${quer:-0})"
    fi
done
rm -f "$TMP" "$TMP.ll" "$TMP.mlir"
echo "ESTRESSE: $ok caso(s) corretos, $falhas falha(s)"
[ "$falhas" -eq 0 ]
