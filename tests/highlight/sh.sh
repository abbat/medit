#!/bin/sh
# A comment, and then a variable inside a string.

name=${1:-world}
count=$((2 + 3))

for i in one two three; do
    printf 'hello, %s\n' "$name from $i"
done

if [ "$count" -gt 4 ]; then
    echo "counted $(expr "$count" + 1)"
fi
