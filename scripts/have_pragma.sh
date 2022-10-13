#!/bin/bash

if [ "$#" = 1 ]; then
    X="$(head -n 1 $1)"
    if [ "$X" != "#pragma once" ]; then
        echo $1
    fi
else
    find -name "*.hpp" -exec "$0" "{}" \;
fi
