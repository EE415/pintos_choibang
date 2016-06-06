#!/bin/bash

#make clean
make

make build/tests/userprog/no-vm/$1.result
cat build/tests/userprog/no-vm/$1.output
cat build/tests/userprog/no-vm/$1.result
