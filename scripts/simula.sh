#!/bin/sh
# roda um .elf no simavr e mostra a serial (o sed tira cor e o ponto do fim)
ELF="$1"
timeout 20 simavr -m atmega328p -f 16000000 "$ELF" 2>&1 > /dev/null |
    sed -e 's/\x1b\[[0-9;]*m//g' -e 's/\.$//'
