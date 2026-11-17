#!/bin/sh
# espera: 1
echo "inteira principal() @"
echo "["
echo "    inteira x;"
awk 'BEGIN { printf "    x receba 1"; for (i = 1; i < 1000000; i++) printf "+1"; print ";" }'
echo "    respost x"
echo "]"
