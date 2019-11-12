#!/bin/bash

set -e

if [ $# -lt 3 ]; then
	echo "Usage: $0 EDGE_ID LOCMAP PROG_WITH_CONCRETE_INPUT" > /dev/stderr
	exit 1
fi

EDGE_ID="$1"
LOCMAP="$2"
PROG="${@:3}"

if [ ! -f "loctrace.csv" ]; then
    AFL_LOC_TRACE_FILE="loctrace.csv" $PROG < /dev/stdin &> /dev/null 
else
    for cur_loc in $(grep ",$EDGE_ID" "loctrace.csv" | cut -d, -f1); do
	      grep "^$cur_loc" $LOCMAP | cut -d, -f2-4 | sed 's/,/\//' | sed 's/,/:/'
    done | sort | uniq
    rm "loctrace.csv"
fi


