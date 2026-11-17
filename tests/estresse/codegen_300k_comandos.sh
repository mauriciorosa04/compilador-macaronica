#!/bin/sh
# espera: 0
# args: -o @SAIDA
echo "inteira principal() @"
echo "["
echo "    inteira x;"
awk 'BEGIN { for (i = 0; i < 100000; i++) { print "    x receba x + " i ";"; print "    escreva(\"linha\");"; print "    escreva(x);" } }'
echo "    respost 0;"
echo "]"
