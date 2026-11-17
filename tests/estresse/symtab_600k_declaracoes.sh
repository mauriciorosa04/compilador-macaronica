#!/bin/sh
# espera: 0
echo "inteira principal() @"
echo "["
awk 'BEGIN { for (i = 0; i < 600000; i++) print "    inteira v" i ";" }'
echo "    respost 0;"
echo "]"
