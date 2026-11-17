#!/bin/sh
# programa .mac -> placa/<nome>.hex do Arduino Uno
set -e
PROG="$1"
BIN="${2:-./bin/macaronica}"
if [ -z "$PROG" ]; then
    echo "uso: sh scripts/placa.sh programa.mac [compilador]"
    exit 2
fi
NOME=$(basename "$PROG" .mac)
B=build/placa
mkdir -p "$B" placa

if command -v llc-18 > /dev/null 2>&1; then LLC=llc-18; else LLC=llc; fi

# 1. LLVM IR, com as conferencias da placa. E o mesmo .ll do PC
"$BIN" --alvo=uno "$PROG" -o "$B/$NOME.ll"

# 2. so aqui o alvo e escolhido
$LLC -mtriple=avr -mcpu=atmega328p -filetype=obj \
     -function-sections -data-sections "$B/$NOME.ll" -o "$B/$NOME.o"

# nucleos vetoriais descem no modo escalar (o ATmega nao tem vetor)
NUCLEOS=""
if [ -e "$B/$NOME.mlir" ]; then
    bash scripts/pipeline.sh "$B/$NOME.mlir" "$B/$NOME.nucleos.ll"
    $LLC -mtriple=avr -mcpu=atmega328p -filetype=obj \
         -function-sections -data-sections "$B/$NOME.nucleos.ll" -o "$B/$NOME.nucleos.o"
    NUCLEOS="$B/$NOME.nucleos.o"
fi

# 3. biblioteca de execucao da placa
avr-gcc -mmcu=atmega328p -Os -Wall -std=c99 -ffunction-sections -fdata-sections \
        -c runtime/zl_runtime_uno.c -o "$B/zl_runtime_uno.o"

# 4. ligacao, tirando o que o programa nao usa
avr-gcc -mmcu=atmega328p -Wl,--gc-sections "$B/$NOME.o" ${NUCLEOS:+"$NUCLEOS"} "$B/zl_runtime_uno.o" \
        -o "$B/$NOME.elf" -lm

# 5. ELF -> Intel HEX
avr-objcopy -O ihex -R .eeprom "$B/$NOME.elf" "placa/$NOME.hex"

# 6. flash = text + data, RAM fixa = data + bss
avr-size "$B/$NOME.elf" | awk -v n="$NOME" 'NR == 2 {
    printf "%s: flash %d de 32256 bytes, RAM fixa %d de 2048 bytes -> placa/%s.hex\n", n, $1 + $2, $2 + $3, n }'
