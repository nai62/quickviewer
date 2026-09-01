# QuickViewer test runbook

The repository-wide verification requirements are defined in `AGENTS.md`.
This file describes how to run them.

## Prerequisites

The supported verification environment is 64-bit Windows with Qt 6.11.2 for
MSVC 2022. The test runner uses these defaults:

```text
Qt:      C:\Qt\6.11.2\msvc2022_64
Debug:   C:\build\quickviewer-msvc2022_64-debug
Release: C:\build\quickviewer-msvc2022_64-release
```

Override `QV_QT_DIR`, `QV_VCVARS`, or `QV_BUILD_DIR` before invoking the
script when the local installation differs.

## Running from native Windows

From a Windows command prompt in the repository root, run:

```bat
scripts\test-windows.cmd
```

## Running from WSL

The repository must be accessible from Windows. From the repository root in
WSL, convert its path to a Windows path. Start `cmd.exe` from the Windows drive
to avoid its UNC working-directory warning, then use `pushd` to enter the
repository:

```bash
repo_win_path="$(wslpath -w "$PWD")"
(
  cd /mnt/c
  cmd.exe /d /c pushd "$repo_win_path" '&&' call \
    'scripts\test-windows.cmd'
)
```

Append any runner option after the script path. For example:

```bash
(
  cd /mnt/c
  cmd.exe /d /c pushd "$repo_win_path" '&&' call \
    'scripts\test-windows.cmd' --release-only
)
```

## Verification modes

With no option, the runner regenerates the Debug qmake build, compiles all
targets, stages the application translations, and runs every expected QtTest
executable:

```bat
scripts\test-windows.cmd
```

A missing test executable is a failure. Use the process exit code and current
QtTest totals rather than a historical fixed test count.

To compile and link the complete Release build and stage its translations, run:

```bat
scripts\test-windows.cmd --release-only
```

Run the complete suite without rebuilding:

```bat
scripts\test-windows.cmd --tests-only
```

Run only the viewer regression suite, optionally selecting one QtTest
function:

```bat
scripts\test-windows.cmd --viewer-only
scripts\test-windows.cmd --viewer-only fittingModeRelayoutsRenderedPage
```

These commands speed up iteration but do not replace the complete verification
required before handoff. Under WSL, pass the same option after the quoted
script path in the `cmd.exe` command shown above.

## C++ lint

Install Python 3 and clang-format 18. From the repository root, check C++ files
changed from `HEAD`:

```bash
python3 scripts/lint-cpp.py
```

Apply formatting fixes to those files with `--fix`. Pass one or more paths to
check specific files, or use `--all` to check the C++ files tracked under
`QuickViewer` and `qvtest`. Submodules under `QuickViewer/src` are deliberately
excluded. The root `.clang-format-ignore` applies the same exclusions when
clang-format is invoked directly or through an editor integration.

The root `.clang-format` requires a space before control-statement parentheses,
spaces around binary and assignment operators, and braces around `if`, `for`,
`while`, and related statement bodies. Member-access operators (`.` and `->`)
remain unspaced.

Set `CLANG_FORMAT` when the executable has a nonstandard name, or pass
`--clang-format` explicitly.

On Windows, use `py -3` in place of `python3` when Python is installed through
the standard Windows launcher. Pull requests check all files in the lint scope
automatically.

## Interactive checks

Startup painting, fullscreen, OpenGL, input timing, and other visual behavior
must also be checked in an interactive Windows session when affected. An
automated or headless run does not establish visual correctness. Report any
interactive checks that were not performed.
