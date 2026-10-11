#!/usr/bin/env python3
"""Tests of package.py: python tools/test_package.py"""

import hashlib
import sys
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import package  # noqa: E402


def make_bin(root, executable):
    (root / "assets" / "ui").mkdir(parents=True)
    (root / executable).write_bytes(b"exe")
    (root / "assets" / "ui" / "launcher.xml").write_text("<ui/>", encoding="utf-8")


class Pack(unittest.TestCase):
    def test_a_tar_gz_has_one_top_directory_the_executable_and_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            bin_dir, out = Path(directory) / "bin", Path(directory) / "dist"
            make_bin(bin_dir, "wind_launcher")
            archive = package.pack(bin_dir, "0.1.2a", "linux", "x86_64", out)
            self.assertEqual(archive.name, "wind-launcher-0.1.2a-linux-x86_64.tar.gz")
            with tarfile.open(archive) as tf:
                names = tf.getnames()
            top = "wind-launcher-0.1.2a-linux-x86_64"
            self.assertIn(top + "/wind_launcher", names)
            self.assertIn(top + "/assets/ui/launcher.xml", names)
            self.assertTrue(all(n == top or n.startswith(top + "/") for n in names))

    def test_a_windows_zip_has_one_top_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            bin_dir, out = Path(directory) / "bin", Path(directory) / "dist"
            make_bin(bin_dir, "wind_launcher.exe")
            archive = package.pack(bin_dir, "0.1.2a", "windows", "x86_64", out)
            self.assertEqual(archive.name, "wind-launcher-0.1.2a-windows-x86_64.zip")
            with zipfile.ZipFile(archive) as zf:
                names = zf.namelist()
            top = "wind-launcher-0.1.2a-windows-x86_64"
            self.assertIn(top + "/wind_launcher.exe", names)
            self.assertTrue(all(n.startswith(top + "/") for n in names))

    def test_the_checksum_file_matches_the_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            bin_dir, out = Path(directory) / "bin", Path(directory) / "dist"
            make_bin(bin_dir, "wind_launcher")
            archive = package.pack(bin_dir, "0.1.0a", "macos", "arm64", out)
            line = (out / (archive.name + ".sha256")).read_text(encoding="utf-8")
            self.assertEqual(line, "%s  %s\n" % (hashlib.sha256(archive.read_bytes()).hexdigest(), archive.name))

    def test_a_missing_executable_or_assets_is_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(SystemExit):
                package.pack(Path(directory), "0.1.0a", "linux", "x86_64", Path(directory) / "dist")
            (Path(directory) / "wind_launcher").write_bytes(b"x")
            with self.assertRaises(SystemExit):
                package.pack(Path(directory), "0.1.0a", "linux", "x86_64", Path(directory) / "dist")


if __name__ == "__main__":
    unittest.main()
