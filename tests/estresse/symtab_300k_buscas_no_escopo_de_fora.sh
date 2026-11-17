#!/bin/sh
# espera: 0
echo "inteira principal() @"
echo "["
awk 'BEGIN { for (i = 0; i < 300000; i++) print "    inteira v" i ";" }'
echo "    cond ( VER ) edai"
echo "    ["
echo "        inteira dentro;"
echo "        leia(dentro);"
awk 'BEGIN { for (i = 0; i < 300000; i++) print "        leia(v" i ");" }'
echo "    ]"
echo "    respost 0;"
echo "]"
