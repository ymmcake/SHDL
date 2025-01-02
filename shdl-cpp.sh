#!/bin/sh
if [ -z "$1" ]; then
	echo "usage: $0 <input> [<output>]"
	exit 2
elif [ -z "$2" ]; then
	cpp -C -P -nostdinc $1 | ./shdl
else
	cpp -C -P -nostdinc $1 | ./shdl > $2
fi
