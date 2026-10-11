#!/usr/bin/env python3
"""Packs the built launcher: one top directory wind-launcher-<version>-<os>-<arch>/ with the executable and assets/.

  tools/package.py --bin build/bin/Release --version 0.1.2a --os linux --arch x86_64 --out dist

Writes dist/wind-launcher-<version>-<os>-<arch>.zip (windows) or .tar.gz (linux, macos) and the .sha256 next to it
(`<hex>  <file name>`, the sha256sum format). The last line printed is the archive path. Standard library only.
"""

import argparse
import hashlib
import shutil
import sys
import tarfile
import tempfile
import zipfile
from pathlib import Path


def archive_name(version, os_name, arch):
    extension = "zip" if os_name == "windows" else "tar.gz"
    return "wind-launcher-%s-%s-%s.%s" % (version, os_name, arch, extension)


def pack(bin_dir, version, os_name, arch, out_dir):
    bin_dir, out_dir = Path(bin_dir), Path(out_dir)
    executable = "wind_launcher.exe" if os_name == "windows" else "wind_launcher"
    if not (bin_dir / executable).is_file():
        raise SystemExit("package: %s has no %s" % (bin_dir, executable))
    if not (bin_dir / "assets").is_dir():
        raise SystemExit("package: %s has no assets/" % bin_dir)

    top = "wind-launcher-%s-%s-%s" % (version, os_name, arch)
    out_dir.mkdir(parents=True, exist_ok=True)
    archive = out_dir / archive_name(version, os_name, arch)
    with tempfile.TemporaryDirectory() as staging:
        root = Path(staging) / top
        root.mkdir()
        shutil.copy2(bin_dir / executable, root / executable)
        shutil.copytree(bin_dir / "assets", root / "assets")
        if os_name == "windows":
            with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zf:
                for path in sorted(root.rglob("*")):
                    zf.write(path, path.relative_to(root.parent).as_posix())
        else:
            with tarfile.open(archive, "w:gz") as tf:
                tf.add(root, arcname=top)

    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    (out_dir / (archive.name + ".sha256")).write_text("%s  %s\n" % (digest, archive.name), encoding="utf-8")
    return archive


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--bin", required=True, help="the directory with wind_launcher and assets/")
    parser.add_argument("--version", required=True)
    parser.add_argument("--os", required=True, choices=("windows", "linux", "macos"))
    parser.add_argument("--arch", required=True, choices=("x86_64", "arm64"))
    parser.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    print(pack(args.bin, args.version, args.os, args.arch, args.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
