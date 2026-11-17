#!/bin/sh
# espera: 0
echo "inteira principal() @"
echo "["
awk 'BEGIN { for (i = 0; i < 400000; i++) print "    cond ( VER ) edai [ inteira t; leia(t); ] casocontrario edai [ ]" }'
echo "    respost 0;"
echo "]"
