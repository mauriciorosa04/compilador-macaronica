#!/bin/sh
# confere a mensagem de erro, nao so o codigo de saida.
# caso = nome.mac + nome.esperado (+ nome.args e nome.codigo, opcionais)
BIN="$1"
DIR="${2:-$(dirname "$0")/diagnostico}"
TMP="${TMPDIR:-/tmp}/zl_diag_$$"
ok=0
falhas=0
for f in "$DIR"/*.mac; do
    [ -e "$f" ] || continue
    base="${f%.mac}"
    if [ ! -e "$base.esperado" ]; then
        echo "SEM .esperado: $f"
        falhas=$((falhas+1))
        continue
    fi
    if [ -e "$base.args" ]; then
        args=$(tr -d '\r' < "$base.args" | sed "s|@|$f|g")
    else
        args="$f"
    fi
    "$BIN" $args > /dev/null 2> "$TMP.bruto"
    st=$?
    tr -d '\r' < "$TMP.bruto" > "$TMP.obtido"
    tr -d '\r' < "$base.esperado" > "$TMP.esperado"
    if [ -e "$base.codigo" ]; then
        quer=$(tr -d '\r\n ' < "$base.codigo")
    elif grep -q "\[ERRO" "$TMP.esperado"; then
        quer=1
    else
        quer=0
    fi
    if [ "$st" -eq "$quer" ] && cmp -s "$TMP.esperado" "$TMP.obtido"; then
        ok=$((ok+1))
    else
        falhas=$((falhas+1))
        echo "FALHOU: $f (saiu $st, esperava $quer)"
        diff "$TMP.esperado" "$TMP.obtido" | head -12
    fi
done
rm -f "$TMP.bruto" "$TMP.obtido" "$TMP.esperado"
echo "DIAGNOSTICO: $ok caso(s) corretos, $falhas falha(s)"
[ "$falhas" -eq 0 ]
