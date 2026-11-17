#!/bin/sh
# espera: 0
echo "inteira principal() @"
echo "["
echo "    inteira x;"
echo "    x receba 1;"
awk 'BEGIN { printf "    cond ( x > 0"; for (i = 1; i < 200000; i++) printf " E x > 0"; print " ) edai" }'
echo "    ["
echo "        x receba 2;"
echo "    ]"
echo "    respost x;"
echo "]"
