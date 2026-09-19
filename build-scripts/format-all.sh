#!/usr/bin/env bash
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

build-scripts/fmt.sh --all
