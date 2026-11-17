#!/bin/sh
# espera: 1
echo "inteira principal() @"
echo "["
echo "    bool b;"
echo "    b receba VER;"
awk 'BEGIN { printf "    cond ( "; for (i = 0; i < 50000; i++) printf "NAO "; print "b ) edai" }'
echo "    ["
echo "        escreva(\"ok\");"
echo "    ]"
echo "    respost 0;"
echo "]"
