#!/bin/sh
# espera: 1
echo "inteira principal() @"
echo "["
echo "    flut x;"
awk 'BEGIN { printf "    x receba "; for (i = 0; i < 50000; i++) printf "raiz("; printf "2"; for (i = 0; i < 50000; i++) printf ")"; print ";" }'
echo "    escreva(x);"
echo "    respost 0;"
echo "]"
