#!/bin/sh
find . -name ".*" -not -name "." -prune -o -name "*.json" -type f -exec sh -c 'python -m json.tool "$1" >/dev/null || echo "FAILED: $1"' _ {} \;
