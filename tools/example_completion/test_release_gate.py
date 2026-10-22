#!/usr/bin/env python3
"""Teste isolado do bloqueio de APK. Nao executa Gradle nem gera binarios."""
import hashlib
import tempfile
import unittest
from pathlib import Path

from release_gate import BASELINE_JS_COUNT, COMPLETE, UPSTREAM_COMMIT, check_release


class ReleaseGateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.project = root / "csharp"
        self.upstream = root / "upstream"
        content = self.upstream / "samples/ExampleMod/content/Content"
        content.mkdir(parents=True)
        item = self.project / "samples/ExampleModCSharp/Playable/content/Example.cs"
        item.parent.mkdir(parents=True)
        item.write_text("public class Example {}\n")
        self.rows = []
        for index in range(BASELINE_JS_COUNT):
            relative = f"Content/Example{index:03}.js"
            body = b"export class Example {}\n"
            (content / f"Example{index:03}.js").write_bytes(body)
            blob = hashlib.sha1(b"blob " + str(len(body)).encode() + b"\x00" + body).hexdigest()
            self.rows.append({
                "id": relative,
                "javaScript": {"upstreamBlob": blob},
                "status": COMPLETE,
                "cSharp": ["samples/ExampleModCSharp/Playable/content/Example.cs"],
                "missingFeatures": [],
                "automatedEvidence": ["host test passed"],
                "deviceEvidence": ["Android real: referencia rastreavel"],
                "technicalBlockEvidence": []
            })
        self.matrix = {
            "references": {"upstream": {"commit": UPSTREAM_COMMIT}},
            "counts": {"canonicalJsFiles": BASELINE_JS_COUNT},
            "examples": self.rows
        }

    def check(self):
        return check_release(self.matrix, self.project, self.upstream)

    def test_full_parity_passes_without_compilation(self):
        self.assertEqual([], self.check())

    def test_partial_and_missing_hooks_block(self):
        self.rows[0]["status"] = "Parcial"
        self.rows[0]["missingFeatures"] = ["Shoot: oito pellets"]
        self.assertTrue(any("Example000" in x for x in self.check()))

    def test_complete_source_can_qualify_for_first_qa_apk(self):
        self.rows[0]["status"] = "Implementado, aguardando validação"
        self.rows[0]["deviceEvidence"] = []
        self.assertEqual([], self.check())
        self.assertTrue(any("sem evidencia Android" in x for x in
                            check_release(self.matrix, self.project, self.upstream,
                                          phase="release")))

    def test_android_release_needs_every_device_proof(self):
        self.rows[0]["deviceEvidence"] = []
        self.assertEqual([], self.check())
        self.assertTrue(any("sem evidencia Android" in x for x in
                            check_release(self.matrix, self.project, self.upstream,
                                          phase="release")))

    def test_missing_code_and_unsafe_paths_block(self):
        self.rows[0]["cSharp"] = ["../../outside.cs"]
        self.assertTrue(any("caminho invalido" in x for x in self.check()))
        self.rows[0]["cSharp"] = []
        self.assertTrue(any("sem implementacao" in x for x in self.check()))

    def test_added_upstream_example_blocks_even_with_206_mapped(self):
        path = self.upstream / "samples/ExampleMod/content/Content/NewExample.js"
        path.write_text("export class NewExample {}\n")
        self.assertTrue(any("upstream" in x for x in self.check()))

    def test_missing_or_replaced_original_sprite_blocks(self):
        relative = "content/Content/Items/Example.png"
        original = self.upstream / "samples/ExampleMod" / relative
        original.parent.mkdir(parents=True, exist_ok=True)
        original.write_bytes(b"original sprite bytes")
        self.rows[0]["assets"] = [relative]
        self.assertTrue(any("ausente no pacote" in x for x in self.check()))
        copy = self.project / "samples/ExampleModCSharp/Playable" / relative
        copy.parent.mkdir(parents=True, exist_ok=True)
        copy.write_bytes(original.read_bytes())
        self.assertEqual([], self.check())
        copy.write_bytes(b"modified sprite bytes")
        self.assertTrue(any("asset alterado" in x for x in self.check()))

    def test_modified_js_with_same_filename_blocks(self):
        original = self.upstream / "samples/ExampleMod/content/Content/Example000.js"
        original.write_text("export class Changed {}\\n")
        self.assertTrue(any("JS original mudou" in x for x in self.check()))

    def test_stale_snapshot_and_duplicate_block(self):
        self.matrix["references"]["upstream"]["commit"] = "old"
        self.rows[1]["id"] = self.rows[0]["id"]
        failures = self.check()
        self.assertTrue(any("desatualizada" in x for x in failures))
        self.assertTrue(any("duplicado" in x for x in failures))

    def test_tests_required_not_just_android(self):
        self.rows[0]["automatedEvidence"] = []
        self.assertTrue(any("teste automatizado" in x for x in self.check()))

    def test_upstream_checkout_mandatory(self):
        errors = check_release(self.matrix, self.project, None)
        self.assertTrue(any("obrigatorio" in x for x in errors))


if __name__ == "__main__":
    unittest.main()
