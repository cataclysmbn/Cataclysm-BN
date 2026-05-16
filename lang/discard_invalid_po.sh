#!/usr/bin/env bash

# Sometimes PO files pulled from Transifex are not accepted by GNU gettext, for example
#   lang/po/hu.po:430257: 'msgid' and 'msgstr' entries do not both begin with '\n'
#   lang/po/hu.po:534682: 'msgid' and 'msgstr' entries do not both end with '\n'
#   lang/po/hu.po:534692: 'msgid' and 'msgstr' entries do not both end with '\n'
#
# This script tries to compile each updated PO file, and revert PO files that cannot be compiled by GNU gettext.
# So the invalid PO files won't fail Basic Build CI test and block merging the i18n update pull requests.

set -euo pipefail
shopt -s nullglob

discard_po() {
    local po_file="$1"

    echo "Discarding ${po_file}"

    if git ls-files --error-unmatch -- "${po_file}" >/dev/null 2>&1; then
        git restore --source=HEAD -- "${po_file}"
    else
        rm -f -- "${po_file}"
    fi
}

for po_file in lang/po/*.po; do
    if [ "${po_file}" = "lang/po/placeholder.po" ]; then
        continue
    fi

    if ! msgfmt -o /dev/null -- "${po_file}"; then
        discard_po "${po_file}"
    fi
done
