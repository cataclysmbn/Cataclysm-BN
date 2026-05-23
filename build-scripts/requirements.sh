#!/bin/bash

set -e
set -x

if [[ "$LIBBACKTRACE" == "1" ]]; then
    echo "LIBBACKTRACE is built by CMake when enabled."
fi

# needed for newer ubuntu versions
# https://stackoverflow.com/questions/75608323/how-do-i-solve-error-externally-managed-environment-every-time-i-use-pip-3
if [[ $(bc <<< "$(lsb_release -rs) > 22.04") -eq 1 ]]; then
  PIP_FLAGS="--break-system-packages"
fi

if [ -n "$LANGUAGES" ]; then
  pip install --user polib luaparser $PIP_FLAGS
fi

set +x
