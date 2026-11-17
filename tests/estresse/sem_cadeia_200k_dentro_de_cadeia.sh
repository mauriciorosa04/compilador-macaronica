#!/bin/sh
# espera: 0
echo "inteira principal() @"
echo "["
echo "    inteira x;"
awk 'BEGIN {
    printf "    x receba 1";
    for (k = 0; k < 2; k++) {
        printf " + (1";
        for (i = 1; i < 100000; i++) printf " * 1";
        printf ")";
    }
    print " + 1;"
}'
echo "    respost x;"
echo "]"
