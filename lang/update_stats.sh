#!/usr/bin/env bash

set -euo pipefail
shopt -s nullglob

if [ ! -d lang/po ]
then
    if [ -d ../lang/po ]
    then
        cd ..
    else
        echo "Error: Could not find lang/po subdirectory."
        exit 1
    fi
fi

mkdir -p lang/stats src
rm -f lang/stats/*

for f in lang/po/*.po
do
    n=$(basename -- "${f}" .po)
    if [ "${n}" = "placeholder" ]
    then
        continue
    fi

    o="lang/po/${n}.po"
    echo "getting stats for ${n}"
    num_translated=$( \
        msgattrib --translated -- "${o}" | grep -c '^msgid' || true)
    num_untranslated=$( \
        msgattrib --untranslated -- "${o}" | grep -c '^msgid' || true)
    printf '{"%s"sv, %d, %d},\n' \
        "${n}" "$((num_translated-1))" "$((num_untranslated-1))" \
        > "lang/stats/${n}"
done

: > src/lang_stats.inc

stats_files=(lang/stats/*)
if [ ${#stats_files[@]} -gt 0 ]
then
    printf '%s\n' "${stats_files[@]}"
    cat "${stats_files[@]}" > src/lang_stats.inc
else
    echo "No translation statistics generated."
fi
