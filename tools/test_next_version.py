#!/usr/bin/env python3
"""Tests of next_version.py: python tools/test_next_version.py"""

import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import next_version  # noqa: E402


class NextVersion(unittest.TestCase):
    def test_no_tags_starts_at_patch_zero(self):
        self.assertEqual(next_version.next_version([], 0, 1, "a"), "0.1.0a")

    def test_the_patch_is_one_more_than_the_highest(self):
        self.assertEqual(next_version.next_version(["v0.1.0a", "v0.1.1a", "v0.1.2a"], 0, 1, "a"), "0.1.3a")
        self.assertEqual(next_version.next_version(["v0.1.2a", "v0.1.0a"], 0, 1, "a"), "0.1.3a")

    def test_the_number_goes_on_across_a_stage_change(self):
        tags = ["v0.2.4a", "v0.2.5a"]
        self.assertEqual(next_version.next_version(tags, 0, 2, "b"), "0.2.6b")
        tags.append("v0.2.6b")
        self.assertEqual(next_version.next_version(tags, 0, 2, "b"), "0.2.7b")
        self.assertEqual(next_version.next_version(tags, 0, 2, ""), "0.2.7")

    def test_the_stage_can_go_back_and_the_number_still_grows(self):
        self.assertEqual(next_version.next_version(["v0.2.5b"], 0, 2, "a"), "0.2.6a")

    def test_a_new_minor_resets_the_patch(self):
        tags = ["v0.1.7a", "v0.1.99a"]
        self.assertEqual(next_version.next_version(tags, 0, 2, "a"), "0.2.0a")
        self.assertEqual(next_version.next_version(tags, 1, 1, "a"), "1.1.0a")

    def test_a_major_and_minor_keep_to_themselves(self):
        tags = ["v0.1.5a", "v0.10.9a", "v1.1.8a", "v0.2.3a"]
        self.assertEqual(next_version.next_version(tags, 0, 1, "a"), "0.1.6a")
        self.assertEqual(next_version.next_version(tags, 0, 10, "a"), "0.10.10a")

    def test_other_text_is_ignored(self):
        tags = ["latest", "v0.1", "v0.1.x", "0.1.50a", "v0.1.9c", "v0.1.9-dev", "v0.1.9a-dev", "release-0.1.80",
                "v0.1.08a", "v0.1.7aa", "V0.1.30a", "vv0.1.30a", " ", "", "v0.1.5a"]
        self.assertEqual(next_version.next_version(tags, 0, 1, "a"), "0.1.6a")

    def test_a_patch_past_ninety_nine_is_fine(self):
        self.assertEqual(next_version.next_version(["v0.1.99a"], 0, 1, "a"), "0.1.100a")
        self.assertEqual(next_version.next_version(["v0.1.99a", "v0.1.100a", "v0.1.9a"], 0, 1, "a"), "0.1.101a")

    def test_a_production_tag_counts_too(self):
        self.assertEqual(next_version.next_version(["v1.0.0", "v1.0.1"], 1, 0, ""), "1.0.2")

    def test_parse_tag(self):
        self.assertEqual(next_version.parse_tag("v0.1.7a"), (0, 1, 7, "a"))
        self.assertEqual(next_version.parse_tag("v12.0.100"), (12, 0, 100, ""))
        self.assertEqual(next_version.parse_tag("v0.1.7a\n"), (0, 1, 7, "a"))
        self.assertIsNone(next_version.parse_tag("v0.1.7a-dev"))
        self.assertIsNone(next_version.parse_tag("0.1.7a"))


class CommandLine(unittest.TestCase):
    def test_reads_the_version_from_the_version_file(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "version.txt"
            path.write_text("0.3b\n", encoding="utf-8")
            self.assertEqual(next_version.read_version_file(path), (0, 3, "b"))
            path.write_text("1.0", encoding="utf-8")
            self.assertEqual(next_version.read_version_file(path), (1, 0, ""))
            for junk in ("1", "1.0.0", "1.0c", "", "v1.0"):
                path.write_text(junk, encoding="utf-8")
                with self.assertRaises(ValueError, msg=junk):
                    next_version.read_version_file(path)

    def test_the_repository_version_file_is_readable(self):
        root = Path(__file__).resolve().parents[1] / "version.txt"
        major, minor, stage = next_version.read_version_file(root)
        self.assertIn(stage, ("a", "b", ""))
        self.assertGreaterEqual(major, 0)
        self.assertGreaterEqual(minor, 0)

    def test_the_rolling_latest_tag_is_not_a_version(self):
        self.assertIsNone(next_version.parse_tag("latest"))
        self.assertEqual(next_version.next_version(["latest", "v0.1.0a"], 0, 1, "a"), "0.1.1a")

    def test_main_prints_and_writes_github_output(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "out.txt"
            os.environ["GITHUB_OUTPUT"] = str(output)
            try:
                code = next_version.main(["--major", "0", "--minor", "2", "--stage", "b",
                                          "--tag", "v0.2.5a", "--tag", "junk", "--github-output"])
            finally:
                del os.environ["GITHUB_OUTPUT"]
            self.assertEqual(code, 0)
            self.assertEqual(output.read_text(encoding="utf-8"),
                             "version=0.2.6b\npatch=6\nstage=b\nprerelease=true\n")

    def test_production_is_not_a_prerelease(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "out.txt"
            os.environ["GITHUB_OUTPUT"] = str(output)
            try:
                next_version.main(["--major", "1", "--minor", "0", "--stage", "", "--tag", "v1.0.0", "--github-output"])
            finally:
                del os.environ["GITHUB_OUTPUT"]
            self.assertIn("prerelease=false\n", output.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
