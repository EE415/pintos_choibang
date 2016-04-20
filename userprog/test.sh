#!/bin/bash

#make clean
make

make build/tests/userprog/$1.result
cat build/tests/userprog/$1.output
cat build/tests/userprog/$1.result
