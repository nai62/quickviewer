#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = [
#     "clang-format==23.1.1",
# ]
# ///
"""Check C++ formatting.

With no file arguments, C++ files changed from HEAD are checked in full. Pass
files explicitly to check them, use --staged for the staged C++ lint scope, or
use --all for all tracked first-party C++ files.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CPP_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp"}
FIRST_PARTY_PREFIXES = (
    "apps/",
    "components/",
    "tests/",
)


def run_git(*args: str) -> str:
    result = subprocess.run(
        ["git", *args], cwd=ROOT, text=True, stdout=subprocess.PIPE, check=True
    )
    return result.stdout


def is_cpp(path: str) -> bool:
    return Path(path).suffix.lower() in CPP_EXTENSIONS


def is_first_party(path: str) -> bool:
    normalized = path.replace("\\", "/")
    return normalized.startswith(FIRST_PARTY_PREFIXES)


def filter_cpp_files(paths: list[str]) -> list[str]:
    return sorted(
        {
            path
            for path in paths
            if path and is_cpp(path) and is_first_party(path)
        }
    )


def changed_files(diff_ref: str) -> list[str]:
    changed = run_git(
        "diff",
        "--name-only",
        "-z",
        "--no-ext-diff",
        "--diff-filter=ACMR",
        diff_ref,
        "--",
    ).split("\0")
    untracked = run_git("ls-files", "--others", "--exclude-standard", "-z").split("\0")
    return filter_cpp_files([*changed, *untracked])


def staged_files() -> list[str]:
    paths = run_git(
        "diff",
        "--cached",
        "--name-only",
        "-z",
        "--no-ext-diff",
        "--diff-filter=ACMR",
        "--",
    ).split("\0")
    return filter_cpp_files(paths)


def tracked_first_party_files() -> list[str]:
    paths = run_git("ls-files", "-z").split("\0")
    return filter_cpp_files(paths)


def unstaged_files(paths: list[str]) -> list[str]:
    if not paths:
        return []
    changed = run_git(
        "diff",
        "--name-only",
        "-z",
        "--no-ext-diff",
        "--",
        *paths,
    ).split("\0")
    return sorted(path for path in changed if path)


def find_clang_format() -> str:
    executable = shutil.which("clang-format")
    if executable is None:
        raise RuntimeError(
            "clang-format not found; run this script with "
            "`uv run --script scripts/lint-cpp.py`"
        )
    return executable


def pinned_clang_format_version() -> str | None:
    text = Path(__file__).read_text(encoding="utf-8")
    match = re.search(r'"clang-format==([^"]+)"', text)
    return match.group(1) if match else None


def warn_on_version_mismatch(executable: str) -> None:
    pinned = pinned_clang_format_version()
    if pinned is None:
        return
    result = subprocess.run(
        [executable, "--version"],
        text=True,
        stdout=subprocess.PIPE,
        check=False,
    )
    match = re.search(r"(\d+\.\d+\.\d+)", result.stdout)
    if match is None or match.group(1) == pinned:
        return
    print(
        f"warning: clang-format {match.group(1)} is not the pinned version {pinned}; "
        "run this script with `uv run --script scripts/lint-cpp.py`.",
        file=sys.stderr,
    )


def validate_clang_format_config(executable: str) -> None:
    subprocess.run(
        [executable, "--style=file", "--dump-config"],
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        check=True,
    )


def check_format(
    executable: str, files: list[str], fix: bool
) -> tuple[bool, list[str]]:
    """Check every file and report whether every check succeeded.

    With `fix`, the returned list holds the files whose contents changed.
    """
    succeeded = True
    reformatted: list[str] = []
    for path in files:
        command = [executable, "--style=file"]
        if fix:
            command.append("-i")
        else:
            command.extend(("--dry-run", "--Werror"))
        command.append(path)
        before = (ROOT / path).read_bytes() if fix else b""
        result = subprocess.run(command, cwd=ROOT, check=False)
        if result.returncode != 0:
            succeeded = False
        elif fix and (ROOT / path).read_bytes() != before:
            reformatted.append(path)
    return succeeded, reformatted


def stage_files(files: list[str]) -> None:
    if files:
        subprocess.run(["git", "add", "--", *files], cwd=ROOT, check=True)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "files", nargs="*", help="check complete contents of these files"
    )
    parser.add_argument(
        "--all", action="store_true", help="check all tracked first-party C++ files"
    )
    parser.add_argument(
        "--staged",
        action="store_true",
        help="check staged first-party C++ files; --fix also re-stages fixes",
    )
    parser.add_argument(
        "--diff-ref",
        default="HEAD",
        help="git revision/range used when no files are given (default: HEAD)",
    )
    parser.add_argument("--fix", action="store_true", help="apply clang-format fixes")
    return parser.parse_args()


def main() -> int:
    args = parse_arguments()
    selection_count = int(args.all) + int(args.staged) + int(bool(args.files))
    if selection_count > 1:
        print(
            "error: --all, --staged, and explicit files are mutually exclusive",
            file=sys.stderr,
        )
        return 2

    try:
        if args.all:
            files = tracked_first_party_files()
        elif args.staged:
            files = staged_files()
        elif args.files:
            normalized = sorted(
                {str(Path(path).as_posix()) for path in args.files if is_cpp(path)}
            )
            out_of_scope = [path for path in normalized if not is_first_party(path)]
            if out_of_scope:
                raise RuntimeError(
                    "files outside the configured first-party lint scope: "
                    + ", ".join(out_of_scope)
                )
            files = normalized
        else:
            files = changed_files(args.diff_ref)

        if not files:
            print("No C++ changes to check.")
            return 0

        if args.staged and args.fix:
            partial = unstaged_files(files)
            if partial:
                raise RuntimeError(
                    "cannot auto-fix staged C++ files that also have unstaged changes: "
                    + ", ".join(partial)
                    + ". Stage or stash those changes before committing."
                )

        clang_format = find_clang_format()
        warn_on_version_mismatch(clang_format)
        validate_clang_format_config(clang_format)
        succeeded, reformatted = check_format(clang_format, files, args.fix)
        if not succeeded:
            print(
                "C++ lint failed. Run "
                "`uv run --script scripts/lint-cpp.py --fix` with the same files.",
                file=sys.stderr,
            )
            return 1

        if args.staged and args.fix:
            stage_files(files)

        if reformatted:
            destination = "re-staged" if args.staged else "updated"
            print(f"Formatted and {destination}: {', '.join(reformatted)}")
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    print(f"C++ lint passed ({len(files)} file(s)).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
