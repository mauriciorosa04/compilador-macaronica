#!/bin/sh
# espera: 1
echo "outrafuncao inteira f(inteira a) @"
echo "["
echo "    respost a;"
echo "]"
echo "inteira principal() @"
echo "["
echo "    inteira r;"
awk 'BEGIN { printf "    r receba f(1"; for (i = 1; i < 400000; i++) printf ", 1"; print ");" }'
echo "    respost r;"
echo "]"
