#!/bin/sh
# espera: 0
echo "inteira principal() @"
echo "["
echo "    inteira x;"
echo "    x receba 1;"
awk 'BEGIN { printf "    durante ( x > 0"; for (i = 1; i < 200000; i++) printf "%s", (i % 2 ? " OU x > 0" : " XOR x > 0"); print " ) edai" }'
echo "    ["
echo "        x receba x - 1;"
echo "    ]"
echo "    respost x;"
echo "]"
