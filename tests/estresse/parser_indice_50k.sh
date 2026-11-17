#!/bin/sh
# espera: 1
echo "inteira principal() @"
echo "["
echo "    inteira vet{5} v;"
echo "    inteira x;"
echo "    v{0} receba 0;"
awk 'BEGIN { printf "    x receba "; for (i = 0; i < 50000; i++) printf "v{"; printf "0"; for (i = 0; i < 50000; i++) printf "}"; print ";" }'
echo "    respost x;"
echo "]"
