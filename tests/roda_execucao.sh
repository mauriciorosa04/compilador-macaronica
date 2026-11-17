#!/bin/sh
# compila cada programa, confere o IR e roda pelo lli e como executavel.
# caso = nome.mac + nome.esperado (+ .entrada, .codigo e .erro, opcionais)
BIN="$1"
AQUI="$(dirname "$0")"
RAIZ="$AQUI/.."

acha() {
    for c in "$1-18" "$1"; do
        if command -v "$c" > /dev/null 2>&1; then
            echo "$c"
            return
        fi
    done
}
LLI=$(acha lli)
CLANG=$(acha clang)
LLVMAS=$(acha llvm-as)
OPT=$(acha opt)
LLVMLINK=$(acha llvm-link)
MLIROPT=$(acha mlir-opt)
if [ -z "$LLI" ] || [ -z "$CLANG" ] || [ -z "$LLVMAS" ] || [ -z "$OPT" ]; then
    echo "EXECUCAO: LLVM nao encontrado (lli, clang, llvm-as, opt)."
    echo "          Os testes de execucao NAO rodaram. No Windows, rode o make test dentro do WSL."
    exit 0
fi

TMP="${TMPDIR:-/tmp}/zl_exec_$$"
mkdir -p "$TMP"
ok=0
falhas=0
falhou() {
    falhas=$((falhas+1))
    echo "FALHOU: $1 ($2)"
}

# a biblioteca e montada uma vez so
if ! $CLANG -O1 -S -emit-llvm "$RAIZ/runtime/zl_runtime_posix.c" -o "$TMP/rt.ll" 2> "$TMP/rt.err" ||
   ! $CLANG -O1 -c "$RAIZ/runtime/zl_runtime_posix.c" -o "$TMP/rt.o" 2>> "$TMP/rt.err"; then
    echo "FALHOU: a biblioteca de execucao nao compilou"
    cat "$TMP/rt.err"
    rm -rf "$TMP"
    exit 1
fi

confere() {
    base="${1%.mac}"
    if [ -e "$base.codigo" ]; then
        quer=$(tr -d '\r\n ' < "$base.codigo")
    else
        quer=0
    fi
    tr -d '\r' < "$base.esperado" > "$TMP/esperado"
    if ! cmp -s "$TMP/esperado" "$TMP/saida"; then
        falhou "$1" "$2: saida diferente"
        diff "$TMP/esperado" "$TMP/saida" | head -8
        return 1
    fi
    if [ "$3" -ne "$quer" ]; then
        falhou "$1" "$2: saiu com $3, esperava $quer"
        return 1
    fi
    if [ -e "$base.erro" ]; then
        tr -d '\r' < "$base.erro" > "$TMP/erro_esperado"
        if ! cmp -s "$TMP/erro_esperado" "$TMP/erro"; then
            falhou "$1" "$2: mensagem de erro diferente"
            diff "$TMP/erro_esperado" "$TMP/erro" | head -8
            return 1
        fi
    fi
    return 0
}

for dir in "$AQUI/execucao" "$AQUI/dsl" "$AQUI/placa"; do
    for f in "$dir"/*.mac; do
        [ -e "$f" ] || continue
        base="${f%.mac}"
        [ -e "$base.esperado" ] || continue
        rm -f "$TMP/p.ll"
        if ! "$BIN" "$f" -o "$TMP/p.ll" > /dev/null 2> "$TMP/comp.err"; then
            falhou "$f" "nao compilou"
            head -5 "$TMP/comp.err"
            continue
        fi
        if ! $LLVMAS "$TMP/p.ll" -o /dev/null 2> "$TMP/as.err"; then
            falhou "$f" "llvm-as recusou o IR"
            head -5 "$TMP/as.err"
            continue
        fi
        if ! $OPT -passes=verify -disable-output "$TMP/p.ll" 2> "$TMP/as.err"; then
            falhou "$f" "o verificador do opt recusou o IR"
            head -5 "$TMP/as.err"
            continue
        fi
        if ! sh "$AQUI/confere_ir.sh" "$TMP/p.ll"; then
            falhou "$f" "IR fora das regras de portabilidade"
            continue
        fi
        if [ -e "$TMP/p.mlir" ]; then
            if [ -z "$MLIROPT" ] || [ -z "$LLVMLINK" ]; then
                falhou "$f" "precisa de mlir-opt e llvm-link pros nucleos vetoriais"
                continue
            fi
            if ! bash "$RAIZ/scripts/pipeline.sh" "$TMP/p.mlir" "$TMP/p.nuc.ll" 2> "$TMP/as.err" ||
               ! sh "$AQUI/confere_ir.sh" "$TMP/p.nuc.ll" nucleos ||
               ! $LLVMLINK "$TMP/p.ll" "$TMP/p.nuc.ll" -S -o "$TMP/p.tudo.ll" 2>> "$TMP/as.err"; then
                falhou "$f" "os nucleos em MLIR nao desceram ate LLVM IR"
                head -5 "$TMP/as.err"
                continue
            fi
            mv "$TMP/p.tudo.ll" "$TMP/p.ll"
        fi
        if [ -e "$base.entrada" ]; then
            ent="$base.entrada"
        else
            ent=/dev/null
        fi

        $LLI --extra-module="$TMP/rt.ll" "$TMP/p.ll" < "$ent" > "$TMP/saida" 2> "$TMP/erro"
        confere "$f" "lli" $? || continue

        if ! $CLANG -Wno-override-module "$TMP/p.ll" "$TMP/rt.o" -o "$TMP/prog" -lm 2> "$TMP/as.err"; then
            falhou "$f" "o clang nao montou o executavel"
            head -5 "$TMP/as.err"
            continue
        fi
        "$TMP/prog" < "$ent" > "$TMP/saida" 2> "$TMP/erro"
        confere "$f" "executavel" $? || continue

        ok=$((ok+1))
    done
done

# programa invalido sai 1 e nao deixa arquivo
for f in "$AQUI"/invalidos/*.mac; do
    [ -e "$f" ] || continue
    echo "; resto de outra compilacao" > "$TMP/inv.ll"
    "$BIN" "$f" -o "$TMP/inv.ll" > /dev/null 2>&1
    st=$?
    if [ "$st" -ne 1 ] || [ -e "$TMP/inv.ll" ]; then
        falhou "$f" "programa invalido com -o: saiu $st e o arquivo deveria ter sumido"
    else
        ok=$((ok+1))
    fi
done

# os validos do PjBL 1 nao tem saida esperada, mas o IR tem que passar
for f in "$AQUI"/validos/*.mac; do
    [ -e "$f" ] || continue
    if "$BIN" "$f" -o "$TMP/v.ll" > /dev/null 2>&1 &&
       $LLVMAS "$TMP/v.ll" -o /dev/null 2> /dev/null &&
       $OPT -passes=verify -disable-output "$TMP/v.ll" 2> /dev/null &&
       sh "$AQUI/confere_ir.sh" "$TMP/v.ll"; then
        ok=$((ok+1))
    else
        falhou "$f" "IR de programa valido do PjBL 1 nao passou"
    fi
done

rm -rf "$TMP"
echo "EXECUCAO: $ok caso(s) corretos, $falhas falha(s)"
[ "$falhas" -eq 0 ]
