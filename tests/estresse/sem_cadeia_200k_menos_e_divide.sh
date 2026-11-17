#!/bin/sh
# espera: 0
echo "inteira principal() @"
echo "["
echo "    inteira x;"
echo "    flut f;"
awk 'BEGIN { printf "    x receba 1"; for (i = 1; i < 200000; i++) printf " - 1"; print ";" }'
awk 'BEGIN { printf "    f receba 1"; for (i = 1; i < 200000; i++) printf " / 1"; print ";" }'
echo "    escreva(f);"
echo "    respost x;"
echo "]"
