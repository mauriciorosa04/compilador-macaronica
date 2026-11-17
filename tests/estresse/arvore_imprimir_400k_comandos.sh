#!/bin/sh
# espera: 0
# args: --ast
echo "inteira principal() @"
echo "["
echo "    inteira x;"
echo "    x receba 0;"
awk 'BEGIN { for (i = 0; i < 400000; i++) print "    x receba x + 1;" }'
echo "    respost x;"
echo "]"
