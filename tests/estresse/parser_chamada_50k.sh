#!/bin/sh
# espera: 1
echo "outrafuncao inteira f(inteira a) @"
echo "["
echo "    respost a;"
echo "]"
echo "inteira principal() @"
echo "["
echo "    inteira r;"
awk 'BEGIN { printf "    r receba "; for (i = 0; i < 50000; i++) printf "f("; printf "1"; for (i = 0; i < 50000; i++) printf ")"; print ";" }'
echo "    respost r;"
echo "]"
