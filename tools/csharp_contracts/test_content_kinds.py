from __future__ import annotations

import copy
import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_content_kinds import EXPECTED, EXPECTED_ORDER, SPEC, render, validate


class ContentContractTests(unittest.TestCase):
    def setUp(self):
        self.data = json.loads(SPEC.read_text(encoding="utf-8"))

    def test_stable_enum_and_native_order(self):
        kinds = validate(self.data)
        self.assertEqual(tuple(row["kind"] for row in kinds), EXPECTED)
        self.assertEqual(
            tuple(row["base"] for row in sorted(
                (row for row in kinds if row["nativeOrder"] is not None),
                key=lambda row: row["nativeOrder"])),
            EXPECTED_ORDER)

    def test_player_is_managed_only(self):
        kinds = validate(self.data)
        self.assertIsNone(next(row for row in kinds if row["kind"] == "Player")["nativeOrder"])
        self.assertIn("BunnyPlayer", render(kinds)[1])

    def test_deterministic_generation(self):
        kinds = validate(self.data)
        self.assertEqual(render(kinds), render(list(reversed(kinds))))

    def test_reject_enum_abi_change(self):
        data = copy.deepcopy(self.data)
        data["contentKinds"][0]["value"] = 11
        with self.assertRaises(ValueError):
            validate(data)

    def test_reject_native_registration_reorder(self):
        data = copy.deepcopy(self.data)
        data["contentKinds"][0]["nativeOrder"] = 0
        data["contentKinds"][1]["nativeOrder"] = 1
        with self.assertRaises(ValueError):
            validate(data)

    def test_reject_managed_player_registered_natively(self):
        data = copy.deepcopy(self.data)
        next(row for row in data["contentKinds"] if row["kind"] == "Player")["nativeOrder"] = 9
        with self.assertRaises(ValueError):
            validate(data)

    def test_reject_duplicate_native_slots(self):
        data = copy.deepcopy(self.data)
        data["contentKinds"][0]["nativeOrder"] = 0
        with self.assertRaises(ValueError):
            validate(data)


if __name__ == "__main__":
    unittest.main()
