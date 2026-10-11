#!/usr/bin/env python3
"""The version of the next Wind Launcher release: <major>.<minor>.<patch><stage>.

major, minor and stage are the one line of version.txt at the repository root, <major>.<minor><stage>: "0.1a" is the
0.1 alpha line, "0.2b" a beta, "1.0" production. The patch is one more than the highest patch of the existing release
tags v<major>.<minor>.*, whatever their stage, or 0 when there is none. Same rules as the engine's tools/ci/next_version.py.

  tools/ci/next_version.py                      print the next version, e.g. 0.1.7a
  tools/ci/next_version.py --github-output      also append version, patch, stage, prerelease to $GITHUB_OUTPUT
  tools/ci/next_version.py --major 0 --minor 2 --stage b --tag v0.2.5a --tag v0.2.4a    no git, no version.txt
"""

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

TAG = re.compile(r"^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)([ab]?)$")
STAGES = ("a", "b", "")


def parse_tag(tag):
    """(major, minor, patch, stage) of a release tag v<major>.<minor>.<patch><stage>, or None for any other text."""
    match = TAG.match(tag.strip())
    if match is None:
        return None
    major, minor, patch, stage = match.groups()
    return int(major), int(minor), int(patch), stage


def next_patch(tags, major, minor):
    """One more than the highest patch among the tags of major.minor, 0 without any. Other tags are ignored."""
    patches = []
    for tag in tags:
        parsed = parse_tag(tag)
        if parsed is not None and parsed[0] == major and parsed[1] == minor:
            patches.append(parsed[2])
    return max(patches) + 1 if patches else 0


def next_version(tags, major, minor, stage):
    return "%d.%d.%d%s" % (major, minor, next_patch(tags, major, minor), stage)


VERSION_LINE = re.compile(r"^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)([ab]?)$")


def read_version_file(path):
    """(major, minor, stage) from version.txt: one line, <major>.<minor><stage>, like 0.1a."""
    text = Path(path).read_text(encoding="utf-8").strip()
    match = VERSION_LINE.match(text)
    if match is None:
        raise ValueError('%s: expected <major>.<minor>[a|b] like "0.1a", found "%s"' % (path, text))
    return int(match.group(1)), int(match.group(2)), match.group(3)


def git_tags(major, minor):
    result = subprocess.run(
        ["git", "tag", "-l", "v%d.%d.*" % (major, minor)], capture_output=True, text=True, check=True
    )
    return result.stdout.split()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--version-file", default=str(Path(__file__).resolve().parents[1] / "version.txt"))
    parser.add_argument("--major", type=int)
    parser.add_argument("--minor", type=int)
    parser.add_argument("--stage", choices=STAGES, help='"a", "b", or "" for production')
    parser.add_argument("--tag", action="append", help="an existing tag (repeatable); without any, `git tag -l` is asked")
    parser.add_argument("--github-output", action="store_true", help="append the results to $GITHUB_OUTPUT")
    args = parser.parse_args(argv)

    major, minor, stage = args.major, args.minor, args.stage
    if major is None or minor is None or stage is None:
        file_major, file_minor, file_stage = read_version_file(args.version_file)
        major = file_major if major is None else major
        minor = file_minor if minor is None else minor
        stage = file_stage if stage is None else stage
    tags = args.tag if args.tag is not None else git_tags(major, minor)

    patch = next_patch(tags, major, minor)
    version = "%d.%d.%d%s" % (major, minor, patch, stage)
    print(version)
    if args.github_output:
        path = os.environ.get("GITHUB_OUTPUT")
        if not path:
            print("--github-output without $GITHUB_OUTPUT", file=sys.stderr)
            return 1
        with open(path, "a", encoding="utf-8") as output:
            output.write("version=%s\npatch=%d\nstage=%s\nprerelease=%s\n" % (
                version, patch, stage, "true" if stage else "false"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
