#!/bin/sh
# programa .mac -> executavel do PC em build/pc/
set -e
PROG="$1"
BIN="${2:-./bin/macaronica}"
if [ -z "$PROG" ]; then
    echo "uso: sh scripts/compila.sh programa.mac [compilador]"
    exit 2
fi
NOME=$(basename "$PROG" .mac)
B=build/pc
mkdir -p "$B"
acha() { if command -v "$1-18" > /dev/null 2>&1; then echo "$1-18"; else echo "$1"; fi; }
CLANG=$(acha clang)
LLVM_LINK=$(acha llvm-link)

# 1. Macaronica -> LLVM IR (e MLIR, se tem operacao vetorial)
"$BIN" "$PROG" -o "$B/$NOME.ll"

# 2. nucleos: MLIR -> LLVM IR e junta com o programa
TUDO="$B/$NOME.ll"
if [ -e "$B/$NOME.mlir" ]; then
    bash scripts/pipeline.sh "$B/$NOME.mlir" "$B/$NOME.nucleos.ll"
    $LLVM_LINK "$B/$NOME.ll" "$B/$NOME.nucleos.ll" -S -o "$B/$NOME.tudo.ll"
    TUDO="$B/$NOME.tudo.ll"
fi

# 3. liga com a biblioteca. O alvo e escolhido aqui, nao no .ll
$CLANG -Wno-override-module "$TUDO" runtime/zl_runtime_posix.c -o "$B/$NOME" -lm
echo "$B/$NOME"
