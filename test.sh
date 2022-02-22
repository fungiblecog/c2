#!/bin/bash

for f in "$@"; do

    echo "Running test file: $f ...";
    ./runtest.py $f -- ./c2
done
