#!/bin/bash

for f in ./tests/*.mal; do

    echo "Processing $f file...";
    ./runtest.py $f -- ./c2
done

for f in ./tests/lib/*.mal; do

    echo "Processing $f file...";
    ./runtest.py $f -- ./c2
done

for f in ./tests/c2/*.mal; do

    echo "Processing $f file...";
    ./runtest.py $f -- ./c2
done
