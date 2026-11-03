set shell := ["bash", "-eu", "-o", "pipefail", "-c"]

default:
    @just --list

fmt *ARGS:
    build-scripts/fmt.sh {{ARGS}}

fmt-cpp *FILES:
    build-scripts/fmt.sh cpp {{FILES}}

fmt-json *FILES:
    build-scripts/fmt.sh json {{FILES}}

fmt-return *PATHS:
    build-scripts/format-trailing-return.sh {{PATHS}}

fmt-docs:
    build-scripts/fmt.sh docs

fmt-lua:
    build-scripts/fmt.sh lua

lint: lint-json lint-dialogue

lint-json:
    build-scripts/lint-json.sh

lint-dialogue:
    tools/dialogue_validator.py data/json/npcs/* data/json/npcs/*/* data/json/npcs/*/*/*

hooks-setup:
    prek install

# Download and launch a PR tiles build: just playtest [--os OS] <PR URL>.
[positional-arguments]
playtest *args:
    deno run --allow-read --allow-write --allow-run --allow-env scripts/playtest.ts "$@"

# Download the latest .po files from Transifex (needs `tx` and TX_TOKEN): just pull-po [LANG,...].
pull-po *LANGS:
    tx pull --force --all {{ if LANGS == '' { '' } else { '--languages ' + replace(LANGS, ' ', ',') } }}

check: lint
