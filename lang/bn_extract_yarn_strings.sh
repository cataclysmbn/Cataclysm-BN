#!/bin/sh

# Extract Yarn Spinner dialogue strings from base game and in-repo mods

lang/extract_yarn_strings.py \
    -i "data/dialogue" \
    "$@"
