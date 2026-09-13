#!/bin/sh

POT_DIRECTORY="lang/po"
TEMP_POT_FROM_CODE="$POT_DIRECTORY/temp-code.pot"
TEMP_POT_FROM_JSON="$POT_DIRECTORY/temp-json.pot"
FINAL_POT_FILE="$POT_DIRECTORY/cataclysm-bn.pot"

if [ ! -d $POT_DIRECTORY ]
then
    if [ -d ../$POT_DIRECTORY ]
    then
        cd ..
    else
        echo "Error: Could not find $POT_DIRECTORY subdirectory."
        exit 1
    fi
fi

echo "> Extracting strings from json"
if ! lang/bn_extract_json_strings.sh "$@" --output $TEMP_POT_FROM_JSON
then
    echo "Error in extract_json_strings.py. Aborting"
    exit 1
fi

SOURCE_FILES=$(mktemp) || exit 1

trap 'rm -f "$SOURCE_FILES"' 0 # Gets rid of "$SOURCE_FILES" on exit

if ! find src \
  \( -path src/lua -o -path src/sol -o -path src/third-party \) -prune -o \
  -type f \( -name '*.cpp' -o -name '*.h' \) -print > "$SOURCE_FILES"; then # Should probably use a constant to point to folders/files that shouldn't be targeted.
    echo "Failed to discover source files." >&2
    exit 1
fi

if [ ! -s "$SOURCE_FILES" ]; then
  echo "No source files found." >&2
  exit 1
fi

echo "> Extracting strings from source code"
xgettext --default-domain="cataclysm-bn" \
         --add-comments="~" \
         --sort-by-file \
         --output="$TEMP_POT_FROM_CODE" \
         --keyword="_" \
         --keyword="pgettext:1c,2" \
         --keyword="vgettext:1,2" \
         --keyword="vpgettext:1c,2,3" \
         --keyword="translate_marker" \
         --keyword="translate_marker_context:1c,2" \
         --keyword="to_translation:1,1t" \
         --keyword="to_translation:1c,2,2t" \
         --keyword="pl_translation:1,2,2t" \
         --keyword="pl_translation:1c,2,3,3t" \
         --from-code="UTF-8" \
         --files-from"$SOURCE_FILES"
if [ $? -ne 0 ]; then
    echo "Error in xgettext. Aborting"
    exit 1
fi

# Fix headers to allow compiling this .pot with msgfmt
# 1. Remove first 6 strings (they contain default comments and "fizzy" marker)
# 2. Configure 'Project-Id-Version' header
# 3. Remove unconfigured 'Plural-Forms' header
if [ "`head -n1 $TEMP_POT_FROM_CODE`" = "# SOME DESCRIPTIVE TITLE." ]
then
    echo "> Fixing .pot file headers"
    package="cataclysm-bn"
    version=$(grep '^VERSION *= *' Makefile | tr -d [:space:] | cut -f 2 -d '=')
    pot_file="$TEMP_POT_FROM_CODE"
    sed -e "1,6d" \
    -e "s/^\"Project-Id-Version:.*\"$/\"Project-Id-Version: $package $version\\\n\"/1" \
    -e "/\"Plural-Forms:.*\"$/d" $pot_file > $pot_file.temp
    mv $pot_file.temp $pot_file
fi

echo "> Combining JSON and source code strings"
if ! lang/concat_pot_files.py $TEMP_POT_FROM_JSON $TEMP_POT_FROM_CODE $FINAL_POT_FILE
then
    echo "Error in concat_pot_files.py. Aborting"
    exit 1
fi

echo "> Resolving duplicates and conflicts"
if ! lang/dedup_pot_file.py $FINAL_POT_FILE
then
    echo "Error in dedup_pot_file.py. Aborting"
    exit 1
fi

# Final compilation check
echo "> Testing to compile the .pot file"
if ! msgfmt -c -o /dev/null $FINAL_POT_FILE
then
    echo "Updated pot file contain gettext errors. Aborting."
    exit 1
fi

# Check for broken Unicode symbols
echo "> Checking for wrong Unicode symbols"
if ! lang/unicode_check.py $FINAL_POT_FILE
then
    echo "Updated pot file contain broken Unicode symbols. Aborting."
    exit 1
fi

# Remove temporary files
echo "> Cleaning up"
if ! rm $TEMP_POT_FROM_CODE
then
    echo "Failed to remove $TEMP_POT_FROM_CODE"
fi
if ! rm $TEMP_POT_FROM_JSON
then
    echo "Failed to remove $TEMP_POT_FROM_JSON"
fi

echo "ALL DONE!"
