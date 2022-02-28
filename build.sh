#!/bin/bash

# Variables
COMPILER="gcc -c"
LINKER="gcc"
FILES=""
OBJECTS=""
CCFLAGS=""
LNFLAGS=""

# Functions
RUNONATE() {
	echo "$*"
	$*
}

CC() {
	FILES="$FILES $*"
	for i in $*; do
		OBJECT="build/$i.o"
		OBJECTS="$OBJECTS $OBJECT"
		mkdir -p "$(dirname $OBJECT)"
		RUNONATE $COMPILER $CCFLAGS -o "$OBJECT" $i
	done
}

# Compile files
CC src/*.c
CC src/common/*.c

# Link files
RUNONATE $LINKER $LNFLAGS -o gr8emu $OBJECTS
