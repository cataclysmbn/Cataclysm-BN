#!/bin/bash

set -e
set -x

if [[ "$LIBBACKTRACE" == "1" ]]; then
    echo "LIBBACKTRACE is built by CMake when enabled."
fi

set +x
