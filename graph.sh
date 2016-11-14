#!/bin/bash

./run > graph/loggraph_.txt
cat graph/loggraph_.txt | grep " GRAPH" | cut -f3 -d$'\t' | cut -d' ' -f1 > graph/loggraph.txt
rm graph/loggraph_.txt

CONSOLECOLUMNS=$(/usr/bin/tput cols)
./graph/graph -w $CONSOLECOLUMNS -c < graph/loggraph.txt
rm graph/loggraph.txt
