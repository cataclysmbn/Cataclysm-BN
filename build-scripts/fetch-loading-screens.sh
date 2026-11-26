#!/usr/bin/env bash
# Download loading screen images from https://github.com/cataclysmbn/loading-screen into data/.
set -euo pipefail
cd "$(dirname "$0")/.."
curl -fsSL --retry 3 "https://github.com/cataclysmbn/loading-screen/releases/download/release/loading-screens.tar.gz" | tar -xz
