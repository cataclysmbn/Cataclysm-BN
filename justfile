set shell := ["bash", "-eu", "-o", "pipefail", "-c"]

default:
    @just --list

fmt *ARGS:
    build-scripts/fmt.sh {{ARGS}}

fmt-cpp *FILES:
    build-scripts/fmt.sh cpp {{FILES}}

fmt-json *FILES:
    build-scripts/fmt.sh json {{FILES}}

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

check: lint
