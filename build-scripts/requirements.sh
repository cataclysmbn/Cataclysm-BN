#!/bin/bash

set -e
set -x

if [[ "$LIBBACKTRACE" == "1" ]]; then
    git clone https://github.com/ianlancetaylor/libbacktrace.git
    (
        cd libbacktrace
        git checkout 4d2dd0b172f2c9192f83ba93425f868f2a13c553
        ./configure
        make -j$(nproc)
        sudo make install
    )
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
