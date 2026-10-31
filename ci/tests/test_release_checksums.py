import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location(
    "checksums", Path(__file__).resolve().parents[1] / "release_checksums.py")
checksums = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(checksums)


class ReleaseChecksumsTest(unittest.TestCase):
    def test_hash_matches_known_vector(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / "example.zip"
            archive.write_bytes(b"abc")
            section = checksums.checksum_section([archive])
            self.assertIn("`example.zip`", section)
            self.assertIn("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", section)

    def test_all_six_assets_required(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for pattern in checksums.PATTERNS:
                path = root / pattern.replace("*", "1.5.5")
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"zip fixture")
            files = checksums.collect_assets(root)
            self.assertEqual(len(files), 6)
            files[0].unlink()
            with self.assertRaisesRegex(ValueError, "Expected exactly one"):
                checksums.collect_assets(root)

    def test_rerun_preserves_notes_without_duplicate_sections(self):
        section = checksums.checksum_section([])
        body = "人工发行说明\n\n" + section + "\n\nAdditional notes."
        updated = checksums.merge_notes(body, section.replace("## SHA256", "## SHA256 updated"))
        self.assertTrue(updated.startswith("人工发行说明\n\n"))
        self.assertTrue(updated.endswith("\n\nAdditional notes."))
        self.assertEqual(updated.count(checksums.START), 1)
        self.assertEqual(checksums.merge_notes(updated, section), body)
        self.assertEqual(checksums.merge_notes("", section), section + "\n")

    def test_malformed_markers_fail_without_overwriting_notes(self):
        with self.assertRaises(ValueError):
            checksums.merge_notes("manual " + checksums.START, "new")

    @patch.object(checksums.subprocess, "run")
    def test_publish_updates_only_body(self, run):
        run.return_value.stdout = json.dumps({"body": "Existing release notes"})
        checksums.publish_section("checksum section", "owner/repo", "42")
        args, kwargs = run.call_args
        self.assertIn("repos/owner/repo/releases/42", args[0])
        payload = json.loads(kwargs["input"])
        self.assertEqual(list(payload), ["body"])
        self.assertEqual(payload["body"], "Existing release notes\n\nchecksum section\n")


if __name__ == "__main__":
    unittest.main()
