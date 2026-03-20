import json
import subprocess
import tempfile
import unittest
from pathlib import Path

import polib


class ProcJsonExtractionTest(unittest.TestCase):
    def test_extracts_proc_slot_roles_and_builder_text(self) -> None:
        repo_root = Path(__file__).resolve().parents[1]
        script = repo_root / "lang" / "extract_json_strings.py"

        proc_json = [
            {
                "type": "PROC",
                "id": "sandwich_test",
                "cat": "food",
                "res": "sandwich_generic",
                "slot": [
                    {
                        "id": "bread",
                        "role": "upper bread",
                        "min": 1,
                        "max": 1,
                        "ok": ["tag:bread"],
                    },
                    {
                        "id": "fill",
                        "role": "savory filling",
                        "min": 0,
                        "max": 2,
                        "rep": True,
                        "ok": ["tag:meat"],
                    },
                ],
            }
        ]
        recipe_json = [
            {
                "type": "recipe",
                "result": "sandwich_generic",
                "category": "CC_FOOD",
                "subcategory": "CSC_FOOD_MEAT",
                "skill_used": "cooking",
                "difficulty": 1,
                "time": 100,
                "proc": True,
                "proc_id": "sandwich_test",
                "builder_name": {"str": "Builder Sandwich"},
                "builder_desc": {"str": "Pick bread and fillings."},
                "qualities": [],
                "tools": [],
                "components": [],
            }
        ]

        with tempfile.TemporaryDirectory() as tmpdir:
            temp_root = Path(tmpdir)
            (temp_root / "proc.json").write_text(json.dumps(proc_json), encoding="utf-8")
            (temp_root / "recipe.json").write_text(json.dumps(recipe_json), encoding="utf-8")
            pot_path = temp_root / "out.pot"

            result = subprocess.run(
                ["python3", str(script), "-i", str(temp_root), "-o", str(pot_path)],
                cwd=repo_root,
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertEqual(
                result.returncode,
                0,
                msg=f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}",
            )

            extracted = {entry.msgid for entry in polib.pofile(str(pot_path))}
            self.assertIn("upper bread", extracted)
            self.assertIn("savory filling", extracted)
            self.assertIn("Builder Sandwich", extracted)
            self.assertIn("Pick bread and fillings.", extracted)


if __name__ == "__main__":
    unittest.main()
