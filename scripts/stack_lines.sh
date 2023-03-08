#!/bin/bash
if [ -z "$1" ]; then
    echo "What logfile? (e.g. log.txt)"
    exit 1
fi

IFS='
'
grep "TRACE" "$1" | while read -r line; do
    addr="$(echo "$line" | cut -d " " -f 2)"
    echo ""
    echo "addr: $addr"
    addr2line -e./kernel/kernel.elf -f -C "$addr"
done
