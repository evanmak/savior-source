#!/bin/sh
if [ -z $1 ]; then
    echo "Usage: $0 prog input"
    exit
fi
if [ -z $2 ]; then
    echo "Usage: $0 prog input"
    exit
fi
env 'AFL_LOC_TRACE_FILE=afl.loc' $1 < $2
