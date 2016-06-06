#!/bin/bash

#make clean
make

make build/tests/vm/$1.result
cat build/tests/vm/$1.output
cat build/tests/vm/$1.result
