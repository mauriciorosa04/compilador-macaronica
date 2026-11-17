#!/bin/sh
# espera: 1
echo "inteira principal() @"
echo "["
echo "    inteira a;"
echo "    a receba 2;"
awk 'BEGIN { printf "    cond ( "; for (i = 0; i < 50000; i++) printf "( "; printf "a > 1"; for (i = 0; i < 50000; i++) printf " )"; print " ) edai" }'
echo "    ["
echo "        escreva(\"ok\");"
echo "    ]"
echo "    respost 0;"
echo "]"
