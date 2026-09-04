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

## Verification policy

Use targeted builds and relevant tests by default. Run the complete automated
suite or a complete Release build only when the user explicitly requests full
verification. The runner therefore requires an explicit mode and rejects an
invocation with no option.

## Running full verification from native Windows

From a Windows command prompt in the repository root, write the verbose build
output to a log instead of streaming it to the terminal:

```bat
if not exist C:\build mkdir C:\build
call scripts\test-windows.cmd --full > C:\build\quickviewer-test.log 2>&1
set "QV_TEST_EXIT=%ERRORLEVEL%"
findstr /n /i /c:"Totals:" /c:"All tests passed" /c:": error " /c:"fatal error" /c:"FAILED" /c:"ERROR:" C:\build\quickviewer-test.log
echo Exit code: %QV_TEST_EXIT%
```

The saved exit code is authoritative. The filtered output is only a concise
summary; inspect the log around the first error if the exit code is nonzero.

## Fast incremental viewer build

Use the viewer-only incremental build during normal edit/build cycles. It does
not rerun recursive qmake, rebuild unrelated libraries or tests, or execute the
test suite. It uses the Debug build already configured under `QV_BUILD_DIR` and
builds stale QuickViewer objects in parallel with `jom`:

```bat
scripts\test-windows.cmd --build-viewer-only
```

The default `jom` location is
`C:\Qt\Tools\QtCreator\bin\jom\jom.exe`, and the default parallelism is eight
jobs. Override either when needed:

```bat
set "QV_JOM=C:\path\to\jom.exe"
set "QV_JOBS=12"
scripts\test-windows.cmd --build-viewer-only
```

This mode requires an existing configured Debug build. If
`%QV_BUILD_DIR%\QuickViewer\Makefile.Debug` is missing, initialize the build by
running the full Debug build explicitly once.

From WSL, keep the source tree on the WSL filesystem and the build directory on
the Windows filesystem. Invoke the same incremental mode through `cmd.exe`:

```bash
repo_win_path="$(wslpath -w "$PWD")"
(
  cd /mnt/c
  cmd.exe /d /c pushd "$repo_win_path" '&&' call \
    'scripts\test-windows.cmd' --build-viewer-only
)
```

Running MSVC against a WSL-hosted source tree still has cross-filesystem
overhead. Keeping the incremental command scoped to the QuickViewer project
avoids recursively checking every repository target and is the preferred
day-to-day workflow. Use a Windows-hosted clone only if this targeted build is
still unacceptably slow, since WSL-side Git and source-search operations can be
slower under `/mnt/c`.

## Running from WSL

The repository must be accessible from Windows. From the repository root in
WSL, convert its path to a Windows path. Start `cmd.exe` from the Windows drive
to avoid its UNC working-directory warning, then use `pushd` to enter the
repository. Save the verbose output on the Windows filesystem, then filter the
completed log from WSL:

```bash
repo_win_path="$(wslpath -w "$PWD")"
mkdir -p /mnt/c/build
(
  cd /mnt/c
  cmd.exe /d /c pushd "$repo_win_path" '&&' call \
    'scripts\test-windows.cmd' --full \
    '>' 'C:\build\quickviewer-test.log' '2>&1'
)
build_status=$?
rg -n -i 'Totals:|All tests passed|: error |fatal error|FAILED|ERROR:' \
  /mnt/c/build/quickviewer-test.log
printf 'Exit code: %s\n' "$build_status"
```

Append any runner option after the script path. For example:

```bash
(
  cd /mnt/c
  cmd.exe /d /c pushd "$repo_win_path" '&&' call \
    'scripts\test-windows.cmd' --release-only \
    '>' 'C:\build\quickviewer-release.log' '2>&1'
)
```

If a run fails, read a bounded amount of context around each error rather than
printing the entire log:

```bash
rg -n -i -C 20 ': error |fatal error|FAILED|ERROR:' \
  /mnt/c/build/quickviewer-test.log | head -n 200
```

For long-running builds, wait for the process to finish without repeatedly
retrieving its output. If progress must be checked, inspect only the log size or
a short tail. Full `cl`, `moc`, and `nmake` command lines repeat large include
and option lists and are rarely useful unless diagnosing a specific invocation.
Automated agents should retain the full log as an artifact and return only the
exit code, test totals, and bounded error context to their model context.

## Verification modes

The commands in this section are not part of the default edit/build loop. Run
the complete automated suite or the complete Release build only when the user
explicitly requests full verification. Otherwise, build the affected target
and run only the relevant tests.

With `--full`, the runner performs full verification: it regenerates the Debug
qmake build, compiles all targets, stages the application translations, and
runs every expected QtTest executable. Use the logged form above when full
verification was explicitly requested:

```bat
scripts\test-windows.cmd --full
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

Under WSL, pass the same option after the quoted script path in the `cmd.exe`
command shown above. Report exactly which targeted tests were run; do not imply
that unrequested full verification was performed.

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
