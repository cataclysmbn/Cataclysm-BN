#!/usr/bin/env python3
"""Validate fresh native 1.0 contract/wire exports with the actual Draft 2020-12 schema.

BN_SCHEMA_SAMPLE_FILES contains path-separated stdout files from dedicated native
suites run with BN_CONTRACT_SCHEMA_SAMPLES=1 and BN_WIRE_SCHEMA_SAMPLES=1.
The runner must supply fresh files; committed examples are not a substitute.
"""

import copy
import json
import os
from pathlib import Path
import unittest

from jsonschema import Draft202012Validator

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = json.loads((ROOT / "docs/schema/engine-client/1.0.schema.json").read_text())
VALIDATOR = Draft202012Validator(SCHEMA)


def native_samples(path):
    samples = []
    for line in path.read_text().splitlines():
        for prefix in ("SCHEMA_SAMPLE ", "WIRE_SAMPLE "):
            if line.startswith(prefix):
                samples.append((prefix.strip(), json.loads(line[len(prefix):])))
    return samples


class NativeSchemaCompatibility(unittest.TestCase):
    def test_actual_schema_and_fresh_exports(self):
        Draft202012Validator.check_schema(SCHEMA)
        paths = os.environ.get("BN_SCHEMA_SAMPLE_FILES", "").split(os.pathsep)
        self.assertTrue(paths and all(paths), "fresh native output files are required")
        total = 0
        for filename in paths:
            path = Path(filename)
            samples = native_samples(path)
            self.assertGreater(len(samples), 0, filename)
            for prefix in ("SCHEMA_SAMPLE", "WIRE_SAMPLE"):
                self.assertTrue(any(kind == prefix for kind, _ in samples), (filename, prefix))
            for index, (kind, value) in enumerate(samples):
                with self.subTest(file=filename, index=index, kind=kind):
                    VALIDATOR.validate(value)
            contracts = [value for _, value in samples if "contract_version" in value]
            self.assertTrue(contracts, "actual negotiated 1.0 output must be present")
            for value in contracts:
                self.assertEqual(value["contract_version"], "1.0")
                invalid = copy.deepcopy(value)
                invalid["contract_version"] = "1.1"
                self.assertFalse(VALIDATOR.is_valid(invalid))
            invalid = copy.deepcopy(samples[0][1])
            invalid["unexpected_schema_member"] = True
            self.assertFalse(VALIDATOR.is_valid(invalid), "closed 1.0 shape must reject unknown fields")
            total += len(samples)
            print(f"NATIVE_SCHEMA {filename}: {len(samples)} generated values validated")
        print(f"NATIVE_SCHEMA total={total}; Draft 2020-12; contract 1.0")


if __name__ == "__main__":
    unittest.main()
