#!/bin/sh
# espera: 0
# args: -o @SAIDA
echo "inteira principal() @"
echo "["
echo "    inteira x;"
awk 'BEGIN { printf "    x receba 1"; for (i = 1; i < 200000; i++) printf " + 1"; print ";" }'
echo "    respost x;"
echo "]"
