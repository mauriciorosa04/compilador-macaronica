#!/bin/sh
# espera: 0
# args: -o @SAIDA
echo "inteira principal() @"
echo "["
echo "    inteira x;"
echo "    x receba 1;"
awk 'BEGIN { printf "    cond ( x > 0"; for (i = 1; i < 200000; i++) printf "%s", (i % 2 ? " E x > 0" : " OU x > 0"); print " ) edai" }'
echo "    ["
echo "        x receba 2;"
echo "    ]"
echo "    respost x;"
echo "]"
