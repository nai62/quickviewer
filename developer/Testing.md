# QuickViewer verification runbook

The repository-wide verification requirements are defined in `../AGENTS.md`.
This file describes how to run them.

## Prerequisites

Supported Windows verification uses 64-bit Windows with Qt 6.11.2 for MSVC
2022. The scripts default to:

```text
Qt:      C:\Qt\6.11.2\msvc2022_64
Debug:   C:\build\quickviewer-msvc2022_64-debug
Release: C:\build\quickviewer-msvc2022_64-release
```

The SVG loader also requires Rust 1.87.0 or newer with Cargo. A standard Windows
rustup installation is detected automatically.

Override `QV_QT_DIR`, `QV_VCVARS`, or `QV_BUILD_DIR` when needed.

Each build directory is configured as a single qmake configuration. The Debug
tree contains only Debug artifacts and the Release tree contains only Release
artifacts.

## Verification policy

Automated unit and regression tests run in Debug by default. Debug is the
primary development configuration because it uses the MSVC debug runtime and
iterator checks.

Release is verified separately when needed. A Release build checks
release-specific qmake branches, compile/link behavior under optimization, and
deployment readiness. Do not routinely run the entire QtTest suite in both
Debug and Release unless a task specifically depends on Release behavior.

Use targeted builds and relevant tests by default. Run the complete Debug suite
or complete Release build only when full verification is requested.

## Commands

Configuration and action are separate concepts:

```bat
scripts\verify-windows.cmd debug
scripts\verify-windows.cmd debug --tests-only
scripts\verify-windows.cmd debug --build-viewer-only
scripts\verify-windows.cmd debug --viewer-only [test-function]
scripts\verify-windows.cmd debug --startup-only [test-function]

scripts\verify-windows.cmd release

scripts\deploy-windows.cmd debug
scripts\deploy-windows.cmd release
```

`verify-windows.cmd debug` regenerates the Debug qmake build, builds all
targets, stages translations and the Debug HEIF plug-in, and runs every
expected QtTest executable.

`verify-windows.cmd release` regenerates and builds the complete Release tree,
then stages translations and the Release HEIF plug-in. It does not duplicate
the Debug test suite.

`deploy-windows.cmd` is deliberately separate. It runs `windeployqt` for the
selected configuration and stages the matching HEIF plug-in and libheif runtime
DLLs so `QuickViewer.exe` can be launched directly from Windows Explorer.

## Full Debug verification

From a Windows command prompt in the repository root:

```bat
if not exist C:\build mkdir C:\build
call scripts\verify-windows.cmd debug > C:\build\quickviewer-test.log 2>&1
set "QV_TEST_EXIT=%ERRORLEVEL%"
findstr /n /i /c:"Totals:" /c:"All tests passed" /c:": error " /c:"fatal error" /c:"FAILED" /c:"ERROR:" C:\build\quickviewer-test.log
echo Exit code: %QV_TEST_EXIT%
```

The saved exit code is authoritative. Inspect the log around the first error if
it is nonzero.

## Fast incremental viewer build

```bat
scripts\verify-windows.cmd debug --build-viewer-only
```

This avoids recursive qmake, unrelated library/test builds, and the test suite.
It uses the configured Debug tree and `jom`.

Defaults:

```text
jom:  C:\Qt\Tools\QtCreator\bin\jom\jom.exe
jobs: 8
```

Override with `QV_JOM` and `QV_JOBS`.

The incremental mode requires `%QV_BUILD_DIR%\QuickViewer\Makefile`. Initialize
the Debug tree once with:

```bat
scripts\verify-windows.cmd debug
```

## Targeted tests

Run all existing Debug tests without rebuilding:

```bat
scripts\verify-windows.cmd debug --tests-only
```

Run the viewer regression suite:

```bat
scripts\verify-windows.cmd debug --viewer-only
scripts\verify-windows.cmd debug --viewer-only fittingModeRelayoutsRenderedPage
```

Run the window-startup suite:

```bat
scripts\verify-windows.cmd debug --startup-only
```

To build the startup test alone:

```bat
set "QV_SOURCE_DIR=C:\path\to\quickviewer"
set "QV_BUILD_DIR=C:\build\quickviewer-msvc2022_64-debug"
if not exist "%QV_BUILD_DIR%\qvtest\windowstartup" mkdir "%QV_BUILD_DIR%\qvtest\windowstartup"
cd /d "%QV_BUILD_DIR%\qvtest\windowstartup"
C:\Qt\6.11.2\msvc2022_64\bin\qmake.exe "%QV_SOURCE_DIR%\qvtest\windowstartup\windowstartup.pro" CONFIG+=debug CONFIG-=release CONFIG-=debug_and_release CONFIG-=debug_and_release_target
C:\Qt\Tools\QtCreator\bin\jom\jom.exe -j 8 /f Makefile
call "%QV_SOURCE_DIR%\scripts\verify-windows.cmd" debug --startup-only
```

## Release verification

```bat
scripts\verify-windows.cmd release
```

Use this for build-system changes, Release-only code paths, or when preparing a
distributable build.

## Deployment for Explorer launch

A successful build does not copy the Qt runtime next to the application.
Deploy explicitly when the executable must launch from Explorer:

```bat
scripts\deploy-windows.cmd debug
```

or:

```bat
scripts\deploy-windows.cmd release
```

Deployment is not part of ordinary verification because Qt DLLs do not need to
be recopied after every source-only rebuild.

## Running from WSL

Keep the source tree on the WSL filesystem and build trees on the Windows
filesystem:

```bash
repo_win_path="$(wslpath -w "$PWD")"
mkdir -p /mnt/c/build
(
  cd /mnt/c
  cmd.exe /d /c pushd "$repo_win_path" '&&' call \
    'scripts\verify-windows.cmd' debug \
    '>' 'C:\build\quickviewer-test.log' '2>&1'
)
build_status=$?
rg -n -i 'Totals:|All tests passed|: error |fatal error|FAILED|ERROR:' \
  /mnt/c/build/quickviewer-test.log
printf 'Exit code: %s\n' "$build_status"
```

For Release, replace the command with
`scripts\verify-windows.cmd release`.

## Performance benchmarks

Image loading and decoder performance should be measured with a Release build.
See [Benchmark.md](Benchmark.md) for the benchmark CLI, paired decoder
comparison mode, and output format.

## Profiling first-image display

```bat
set "QV_PROFILE_FIRST_IMAGE=C:\build\qv-first-image.tsv"
start "" /wait "C:\build\quickviewer-msvc2022_64-release\bin\QuickViewer.exe" "C:\path\book.rar"
set "QV_PROFILE_FIRST_IMAGE="
```

## C++ lint

Install Python 3 and clang-format 18.

```bash
python3 scripts/lint-cpp.py
```

Apply fixes with `--fix`; use `--all` for the tracked C++ lint scope. On
Windows, `py -3` can be used in place of `python3`.

## Interactive checks

Startup painting, fullscreen, OpenGL, input timing, and other visual behavior
must also be checked interactively on Windows when affected. Headless
automation does not establish visual correctness.
