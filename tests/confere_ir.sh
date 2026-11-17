#!/bin/sh
# confere as regras do IR portavel: sem alvo, sem i64, sem variadica,
# nounwind em tudo e @zl_principal. Uso: sh tests/confere_ir.sh arquivo.ll
LL="$1"
NUCLEOS="$2"
ruim=0
reclama() {
    echo "  IR fora da regra ($1): $2"
    ruim=1
}
grep -v '^ *;' "$LL" | grep '^target triple' > /dev/null && reclama 1 "tem target triple"
grep -v '^ *;' "$LL" | grep '^target datalayout' > /dev/null && reclama 1 "tem target datalayout"
grep -v '^ *;' "$LL" | grep 'i64' > /dev/null && reclama 2 "usa i64"
grep -v '^ *;' "$LL" | grep -F '...' > /dev/null && reclama 3 "tem funcao variadica"
grep -v '^ *;' "$LL" | grep -E '@(printf|scanf|puts|putchar)' > /dev/null && reclama 3 "chama a libc direto"
if [ -z "$NUCLEOS" ]; then
    if grep -E '^(define|declare) ' "$LL" | grep -v 'nounwind' > /dev/null; then
        reclama 5 "define ou declare sem nounwind"
    fi
else
    if grep -E '^(define|declare) ' "$LL" | grep -v ' #[0-9]' > /dev/null ||
       grep '^attributes #' "$LL" | grep -v 'nounwind' > /dev/null; then
        reclama 5 "nucleo sem nounwind"
    fi
fi
if [ -z "$NUCLEOS" ]; then
    grep '^define i32 @zl_principal() nounwind' "$LL" > /dev/null || reclama 6 "nao tem @zl_principal"
fi
# o llc do AVR erra endereco de alloca com align >= 4
if [ -z "$NUCLEOS" ] && grep -v '^ *;' "$LL" | grep -v '^@\.str\.' | grep -w 'align' > /dev/null; then
    reclama align "align fora de constante de texto"
fi
exit $ruim
