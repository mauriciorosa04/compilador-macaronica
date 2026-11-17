#!/bin/sh
# espera: 1
awk 'BEGIN { for (i = 0; i < 300000; i++) printf "\200\277\360\237\230\200\200\n" }'
echo "inteira principal() @"
echo "["
echo "    respost 0;"
echo "]"
