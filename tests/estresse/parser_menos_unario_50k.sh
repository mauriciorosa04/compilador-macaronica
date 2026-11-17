#!/bin/sh
# espera: 1
echo "inteira principal() @"
echo "["
echo "    inteira x;"
echo "    x receba 1;"
awk 'BEGIN { printf "    x receba "; for (i = 0; i < 50000; i++) printf "- "; print "x;" }'
echo "    respost x;"
echo "]"
