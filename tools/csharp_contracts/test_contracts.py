import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate import SPEC, validate, render


class HookContractTests(unittest.TestCase):
    def setUp(self):
        self.spec = json.loads(SPEC.read_text(encoding="utf-8"))

    def test_all_extended_modes_are_unique_and_contiguous(self):
        modes = validate(self.spec)
        self.assertEqual([mode["mode"] for mode in modes], [1, 2, 3, 4, 5])
        self.assertEqual(len({mode["name"] for mode in modes}), 5)

    def test_render_is_deterministic(self):
        modes = validate(self.spec)
        self.assertEqual(render(modes), render(list(reversed(modes))))

    def test_rejects_duplicates(self):
        self.spec["extendedHooks"][1]["mode"] = 1
        with self.assertRaises(ValueError):
            validate(self.spec)

    def test_rejects_wrong_parameter_count(self):
        self.spec["extendedHooks"][0]["parameters"] = 4
        with self.assertRaises(ValueError):
            validate(self.spec)

    def test_rejects_invalid_boolean(self):
        self.spec["extendedHooks"][0]["returnsInt32"] = 1
        with self.assertRaises(ValueError):
            validate(self.spec)

    def test_rejects_invalid_mode_name(self):
        self.spec["extendedHooks"][0]["name"] = "bad-name"
        with self.assertRaises(ValueError):
            validate(self.spec)


if __name__ == "__main__":
    unittest.main()
