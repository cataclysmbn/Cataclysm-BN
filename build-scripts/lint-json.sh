#!/bin/sh
find . -name ".*" -not -name "." -prune -o -name "*.json" -type f -print0 |
python3 -c '
import json
import sys

failed = False
for raw_path in sys.stdin.buffer.read().split(b"\0"):
    if not raw_path:
        continue
    path = raw_path.decode()
    try:
        with open(path, encoding="utf-8") as file:
            json.load(file)
    except Exception as error:
        print(f"FAILED: {path}: {error}", file=sys.stderr)
        failed = True
sys.exit(1 if failed else 0)
'
