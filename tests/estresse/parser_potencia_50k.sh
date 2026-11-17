#!/bin/sh
# espera: 1
echo "inteira principal() @"
echo "["
echo "    inteira x;"
awk 'BEGIN { printf "    x receba 2"; for (i = 0; i < 50000; i++) printf "^2"; print ";" }'
echo "    respost x;"
echo "]"
