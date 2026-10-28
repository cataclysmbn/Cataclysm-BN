#!/usr/bin/env python3
"""Regression tests for martial-art buff string extraction (issue #2771).

Run: python3 lang/tests/test_martial_art_extraction.py
Requires the same polib and luaparser dependencies as the extractor.
"""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import polib


EXTRACTOR = Path(__file__).resolve().parents[1] / "extract_json_strings.py"
BUFF_EVENTS = (
    "static_buffs", "onmove_buffs", "onpause_buffs", "onhit_buffs",
    "onattack_buffs", "ondodge_buffs", "onblock_buffs", "ongethit_buffs",
    "onmiss_buffs", "oncrit_buffs", "onkill_buffs",
)


class MartialArtExtractionTest(unittest.TestCase):
    def extract(self, style):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            inputs = root / "input"
            inputs.mkdir()
            (inputs / "martialarts.json").write_text(
                json.dumps([style]), encoding="utf-8"
            )
            output = root / "strings.pot"
            subprocess.run(
                [sys.executable, str(EXTRACTOR), "-i", str(inputs),
                 "-o", str(output)],
                check=True, capture_output=True, text=True,
            )
            return {(entry.msgctxt, entry.msgid)
                    for entry in polib.pofile(str(output))}

    def style(self):
        return {
            "type": "martial_art", "id": "style_extraction_test",
            "name": "Extraction test style",
            "description": "Extraction test style description.",
        }

    def check_all_events(self, contextual):
        style = self.style()
        expected = {(None, style["name"]), (None, style["description"])}
        for event in BUFF_EVENTS:
            name = "Test " + event + " name"
            description = "Test " + event + " description."
            context = "buff extraction test" if contextual else None
            style[event] = [{
                "id": "buff_id_" + event,
                "name": {"str": name, "ctxt": context} if contextual else name,
                "description": ({"str": description, "ctxt": context}
                                if contextual else description),
            }]
            expected.update(((context, name), (context, description)))
        # Exact set also rejects accidental extraction of technical buff IDs.
        self.assertEqual(self.extract(style), expected)

    def test_all_event_names_and_descriptions(self):
        self.check_all_events(contextual=False)

    def test_all_events_preserve_translation_context(self):
        self.check_all_events(contextual=True)

    def test_optional_event_arrays_can_be_absent(self):
        style = self.style()
        self.assertEqual(self.extract(style),
                         {(None, style["name"]), (None, style["description"])})


if __name__ == "__main__":
    unittest.main()
