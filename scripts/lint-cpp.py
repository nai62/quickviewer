#!/usr/bin/env python3
"""Check C++ formatting.

With no file arguments, only lines changed from HEAD are checked. This lets the
repository adopt the rules incrementally without reformatting unrelated legacy
code. Pass files explicitly to check each complete file, or use --all for all
first-party C++ files.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from typing import Iterable


ROOT = Path(__file__).resolve().parent.parent
CPP_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp"}
FIRST_PARTY_PREFIXES = (
    "QuickViewer/",
    "qvtest/",
)
EXCLUDED_PREFIXES = (
    "QuickViewer/src/qactionmanager/",
    "QuickViewer/src/qfullscreenframe/",
    "QuickViewer/src/qlanguageselector/",
    "QuickViewer/src/qnamedpipe/",
)
HUNK_PATTERN = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")
VERSION_PATTERN = re.compile(r"version\s+(\d+)", re.IGNORECASE)


def run_git(*args: str) -> str:
    result = subprocess.run(
        ["git", *args], cwd=ROOT, text=True, stdout=subprocess.PIPE, check=True
    )
    return result.stdout


def is_cpp(path: str) -> bool:
    return Path(path).suffix.lower() in CPP_EXTENSIONS


def is_first_party(path: str) -> bool:
    normalized = path.replace("\\", "/")
    return normalized.startswith(FIRST_PARTY_PREFIXES) and not normalized.startswith(EXCLUDED_PREFIXES)


def changed_lines(diff_ref: str) -> dict[str, list[tuple[int, int]]]:
    output = run_git(
        "diff", "--unified=0", "--no-ext-diff", "--diff-filter=ACMR", diff_ref, "--"
    )
    ranges: dict[str, list[tuple[int, int]]] = {}
    current_path: str | None = None
    for line in output.splitlines():
        if line.startswith("+++ b/"):
            candidate = line[6:]
            current_path = candidate if is_cpp(candidate) and is_first_party(candidate) else None
            continue
        if current_path is None:
            continue
        match = HUNK_PATTERN.match(line)
        if not match:
            continue
        start = int(match.group(1))
        count = int(match.group(2) or "1")
        if count:
            ranges.setdefault(current_path, []).append((start, start + count - 1))
    untracked = run_git("ls-files", "--others", "--exclude-standard", "-z").split("\0")
    for path in untracked:
        if not path or not is_cpp(path) or not is_first_party(path):
            continue
        line_count = max(1, len((ROOT / path).read_bytes().splitlines()))
        ranges[path] = [(1, line_count)]
    return ranges


def tracked_first_party_files() -> list[str]:
    paths = run_git("ls-files", "-z").split("\0")
    return sorted(path for path in paths if path and is_cpp(path) and is_first_party(path))


def find_tool(requested: str | None, environment_name: str, candidates: Iterable[str]) -> str:
    preferred = requested or os.environ.get(environment_name)
    names = [preferred] if preferred else list(candidates)
    for name in names:
        if name and shutil.which(name):
            return name
    searched = ", ".join(name for name in names if name)
    raise RuntimeError(f"required tool not found (tried: {searched})")


def require_clang_format_18(executable: str) -> None:
    result = subprocess.run(
        [executable, "--version"], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True
    )
    match = VERSION_PATTERN.search(result.stdout)
    if not match or int(match.group(1)) != 18:
        raise RuntimeError(
            "clang-format 18 is required for reproducible repository-wide formatting; "
            f"found: {result.stdout.strip()}"
        )


def validate_clang_format_config(executable: str) -> None:
    subprocess.run(
        [executable, "--style=file", "--dump-config"],
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        check=True,
    )


def check_format(
    executable: str,
    files: list[str],
    line_ranges: dict[str, list[tuple[int, int]]] | None,
    fix: bool,
) -> bool:
    succeeded = True
    for path in files:
        command = [executable, "--style=file"]
        if fix:
            command.append("-i")
        else:
            command.extend(("--dry-run", "--Werror"))
        if line_ranges is not None:
            for start, end in line_ranges[path]:
                command.append(f"--lines={start}:{end}")
        command.append(path)
        result = subprocess.run(command, cwd=ROOT)
        succeeded = result.returncode == 0 and succeeded
    return succeeded


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="*", help="check complete contents of these files")
    parser.add_argument("--all", action="store_true", help="check all tracked first-party C++ files")
    parser.add_argument(
        "--diff-ref",
        default="HEAD",
        help="git revision/range used when no files are given (default: HEAD)",
    )
    parser.add_argument("--fix", action="store_true", help="apply clang-format fixes")
    parser.add_argument("--clang-format", dest="clang_format", help="clang-format executable")
    return parser.parse_args()


def main() -> int:
    args = parse_arguments()
    if args.all and args.files:
        print("error: --all cannot be combined with file arguments", file=sys.stderr)
        return 2
    try:
        ranges: dict[str, list[tuple[int, int]]] | None
        if args.all:
            files = tracked_first_party_files()
            ranges = None
        elif args.files:
            files = sorted({str(Path(path).as_posix()) for path in args.files if is_cpp(path)})
            out_of_scope = [path for path in files if not is_first_party(path)]
            if out_of_scope:
                raise RuntimeError(
                    "files outside the QuickViewer/qvtest lint scope: " + ", ".join(out_of_scope)
                )
            ranges = None
        else:
            ranges = changed_lines(args.diff_ref)
            files = sorted(ranges)

        clang_format = find_tool(
            args.clang_format,
            "CLANG_FORMAT",
            ("clang-format-18", "clang-format"),
        )
        require_clang_format_18(clang_format)
        validate_clang_format_config(clang_format)
        if not files:
            print("No C++ changes to check.")
            return 0
        succeeded = check_format(clang_format, files, ranges, args.fix)
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    if not succeeded:
        print("C++ lint failed. Run python3 scripts/lint-cpp.py --fix with the same files.", file=sys.stderr)
        return 1
    print(f"C++ lint passed ({len(files)} file(s)).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
