#!/usr/bin/env python3
"Extract translatable strings from Yarn Spinner (.yarn) dialogue files into a POT file."
"The resulting file may contain duplicate entries; use dedup_pot_file.py to resolve them."

import os
import re
import time
from optparse import OptionParser

try:
    import polib
except ImportError:
    print("You need 'polib' module installed for the script to work.")
    exit(1)

parser = OptionParser()
parser.add_option("-i", "--input", action="append", dest="input_folders",
                  help="input folder(s) to scan recursively for .yarn files")
parser.add_option("-o", "--output", action="store", dest="output_file",
                  help="output .pot file path")
(options, args) = parser.parse_args()

if not options.input_folders:
    print("Missing input folder(s)")
    exit(1)
if not options.output_file:
    print("Missing output file")
    exit(1)

# Strip trailing <<command>> blocks from choice labels (handles multiple).
RE_TRAILING_CMD = re.compile(r'\s*<<[^>]*>>\s*$')
# Strip trailing #tag annotations from choice labels.
RE_TRAILING_TAG = re.compile(r'\s*#\w+\s*$')


def strip_speaker(text):
    """Remove 'Speaker: ' prefix when present.

    Matches the C++ parser rule: colon exists, nothing before it is a space,
    i.e. 'Word: rest' but not 'some text: more'.
    """
    colon = text.find(':')
    if colon > 0:
        first_space = text.find(' ')
        if first_space == -1 or first_space > colon:
            return text[colon + 1:].strip()
    return text


def emit(pot, msgid, source_file):
    """Append msgid to the POT, annotating {expr} strings for translators."""
    if not msgid:
        return
    comment = None
    if re.search(r'\{[^}]+\}', msgid):
        comment = "~ Preserve {…} placeholders unchanged when translating."
    entry = pot.find(msgid)
    if entry is None:
        entry = polib.POEntry(
            msgid=msgid,
            msgstr='',
            occurrences=[(source_file, None)],
        )
        if comment:
            entry.comment = comment
        pot.append(entry)
    else:
        entry.occurrences.append((source_file, None))


def extract_file(pot, filepath):
    rel = os.path.relpath(filepath)
    in_body = False
    with open(filepath, encoding='utf-8') as f:
        for raw_line in f:
            line = raw_line.rstrip('\n')
            stripped = line.strip()

            # Node body delimiters
            if stripped == '---':
                in_body = True
                continue
            if stripped == '===':
                in_body = False
                continue

            if not in_body:
                continue

            # Skip blank lines and line comments
            if not stripped or stripped.startswith('//'):
                continue

            # Skip command-only lines (<<command ...>>)
            if stripped.startswith('<<'):
                continue

            # Choice label:  -> text [<<if cond>>] [#tag ...]
            if stripped.startswith('->'):
                text = stripped[2:].strip()
                prev = None
                while prev != text:
                    prev = text
                    text = RE_TRAILING_CMD.sub('', text)
                    text = RE_TRAILING_TAG.sub('', text)
                emit(pot, text.strip(), rel)
                continue

            # Line-group item:  => [Speaker: ] text  (or  => - narrator text)
            if stripped.startswith('=>'):
                text = stripped[2:].strip()
                if text.startswith('- '):
                    text = text[2:].strip()
                else:
                    text = strip_speaker(text)
                emit(pot, text, rel)
                continue

            # Narrator line:  - text
            if stripped.startswith('- '):
                emit(pot, stripped[2:].strip(), rel)
                continue

            # Attributed or continuation dialogue line
            emit(pot, strip_speaker(stripped), rel)


def scan_directory(pot, folder):
    for dirpath, _, filenames in os.walk(folder):
        for name in sorted(filenames):
            if name.endswith('.yarn'):
                extract_file(pot, os.path.join(dirpath, name))


pot = polib.POFile()
pot.metadata = {
    'Project-Id-Version': 'cataclysm-bn',
    'POT-Creation-Date': time.strftime('%Y-%m-%d %H:%M%z'),
    'Language': '',
    'MIME-Version': '1.0',
    'Content-Type': 'text/plain; charset=UTF-8',
    'Content-Transfer-Encoding': '8bit',
}

for folder in options.input_folders:
    scan_directory(pot, os.path.normpath(folder))

out = os.path.normpath(options.output_file)
pot.save(out, newline='\n')
print(f"Wrote {len(pot)} entries to {out}")
