# QuickViewer verification runbook

This runbook covers the supported Windows build, test, and verification
workflow for QuickViewer.

## Prerequisites

Supported Windows verification uses 64-bit Windows with Qt 6.11.2 for MSVC
2022. QuickViewer supports Windows 10 version 1809 (build 17763) and later,
and the build asks the Windows headers for that version; the CI runners are
newer than that floor. The script defaults are:

| Setting | Default | Allowed value |
| --- | --- | --- |
| configuration | none | `debug` or `release` |
| `QV_QT_DIR` | `C:\Qt\6.11.2\msvc2022_64` | Qt MSVC kit directory |
| `QV_VCVARS` | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat` | full path to `vcvars64.bat` |
| `QV_BUILD_DIR` | `C:\build\quickviewer-msvc2022_64-debug` or `C:\build\quickviewer-msvc2022_64-release` | any writable build-tree path |
| `QV_JOM` | `C:\Qt\Tools\QtCreator\bin\jom\jom.exe` | full path to `jom.exe` |
| `QV_JOBS` | unset | positive integer; passed as `jom -j N` |
| `QV_HEIF_SOURCE` | `..\..\qt-heic-image-plugin` relative to the repository | qt-heic-image-plugin source/build root |

`QV_JOBS` is optional. When it is unset, `jom` is invoked without `-j` and
uses its normal automatic parallelism. The script does not fall back to
`nmake` when `jom` is missing.

The SVG loader also requires Rust 1.87.0 or newer with Cargo. A standard Windows
rustup installation is detected automatically by the project files.

Each build directory is configured as a single qmake configuration. The Debug
tree contains Debug artifacts and test targets. The Release tree contains
Release application/library targets; ordinary Release qmake generation omits
test targets that are not executed by the Release workflow.

## Default behavior

`scripts\verify-windows.cmd` is the single entry point for supported Windows
build, test, and verification work.

Normal builds are incremental. If the required Makefile already exists, the
script goes directly to `jom`. It runs `qmake -r` only when the Makefile is
missing or when `--qmake` is explicitly requested.

There is no `--rebuild` or other automatic clean/delete mode. The script never
deletes or recreates the Debug or Release build directory. If a genuinely clean
tree is required, delete or move that build directory manually and then run the
desired command again.

## Verification policy

Automated unit and regression tests run in Debug. Debug is the primary
development configuration because it uses the MSVC debug runtime and iterator
checks. Release is verified separately for release-specific qmake branches,
optimized compile/link behavior, and deployment readiness; ordinary Release
builds do not duplicate the Debug test targets or suite.

Use the narrowest build and test mode that covers the change during development.
Run the normal `debug` command when complete Debug verification is required, and
run the normal `release` command when Release compile/link verification is
required.

## Command summary

| Command | Build scope | qmake behavior | Staging | Tests |
| --- | --- | --- | --- | --- |
| `verify-windows.cmd debug` | top-level incremental Debug build | only if Makefile is missing | translations + Debug HEIF support | normal Debug suite |
| `verify-windows.cmd release` | top-level incremental Release build | only if Makefile is missing | translations + Release HEIF support | none |
| `... --qmake` | same build scope as the selected build mode | force `qmake -r` first | unchanged | unchanged |
| `... --build-only` | normal top-level incremental build | normal/`--qmake` rules | normal usable-build staging | none |
| `... --build-viewer-only` | only `apps\quickviewer` | normal/`--qmake` rules | translations + matching HEIF support | none |
| `debug --tests-only` | no build | no qmake | Debug HEIF support | normal Debug suite |
| `debug --test <name> [test-function]` | no build | no qmake | Debug HEIF support | one selected test |

`--qmake` is an independent modifier for build modes and can be combined with
`--build-only` or `--build-viewer-only`. It is intentionally rejected with
`--tests-only` and `--test`, because those modes do not build anything.

`--build-viewer-only` is the fastest narrow edit/build path. It runs `jom` only
inside `apps\quickviewer`; it may therefore leave changed libraries or other
dependencies stale. Use the normal top-level build for changes outside the
QuickViewer application itself.

## Common commands

Normal Debug verification:

```bat
scripts\verify-windows.cmd debug
```

Normal Release build and staging:

```bat
scripts\verify-windows.cmd release
```

Force qmake regeneration before either build:

```bat
scripts\verify-windows.cmd debug --qmake
scripts\verify-windows.cmd release --qmake
```

Build without running tests:

```bat
scripts\verify-windows.cmd debug --build-only
scripts\verify-windows.cmd release --build-only
```

Fast QuickViewer-only build:

```bat
scripts\verify-windows.cmd debug --build-viewer-only
```

Run tests from existing Debug artifacts without rebuilding:

```bat
scripts\verify-windows.cmd debug --tests-only
```

If a required test executable is missing, test-only modes fail rather than
silently skipping it.

## Individual test selection

Use one stable test name instead of a dedicated option for each test target:

```bat
scripts\verify-windows.cmd debug --test windowstartup
scripts\verify-windows.cmd debug --test viewernavigation
scripts\verify-windows.cmd debug --test windowstartup SomeTestFunction
```

Supported names are:

| Test name | Executable |
| --- | --- |
| `prefetchplanner` | `tst_prefetchplannertest.exe` |
| `asynccache` | `tst_asynccachetest.exe` |
| `latestresultdispatcher` | `tst_latestresultdispatchertest.exe` |
| `fileloader` | `tst_fileloadertest.exe` |
| `svgloader` | `tst_svgloadertest.exe` |
| `viewernavigation` | `tst_viewernavigationtest.exe` |
| `windowstartup` | `tst_windowstartuptest.exe` |
| `catalogdatabase` | `tst_catalogdatabasetest.exe` |

The optional `test-function` is passed to QtTest as the function selector.
`--viewer-only` and `--startup-only` are obsolete and are not supported.

## Running verification as an agent

Build and test output is far larger than a conversation needs. When an agent
runs any of the commands above, redirect stdout and stderr to a temporary file
outside the repository and search that file instead of streaming the whole run.
The exact command is in "Full Debug verification" for a Windows command prompt
and in "Running from WSL" for a WSL shell; both already write to a log file and
search it rather than printing the run in full.

Keep these rules when adapting those commands:

- Write the log outside the source tree (a WSL temporary directory, or
  `C:\build` on Windows) so it cannot be committed. Do not commit verification
  logs, transcripts, or captured test reports.
- Save the exit code immediately after the command, before the search runs,
  because the search replaces `ERRORLEVEL`/`$?`. The saved exit code is
  authoritative; a matching `Totals:` line alone does not prove success.
- Search for the error markers listed in those examples and print only the
  matching lines, with a little surrounding context for the first failure.
- Keep the log until the outcome is understood and anything worth reporting has
  been captured, then delete it.

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

## Release verification

```bat
scripts\verify-windows.cmd release
```

Use this for build-system changes, Release-only code paths, or when preparing a
distributable build. Release builds the application and its normal dependencies
with optimization but does not compile or run the Debug test suite.

## Deployment for Explorer launch

A successful build stages project runtime data and HEIF support, but it does not
copy the Qt runtime next to the application. Deploy Qt explicitly when the
executable must launch from Windows Explorer:

```bat
scripts\deploy-windows.cmd debug
scripts\deploy-windows.cmd release
```

Deployment is deliberately separate because Qt DLLs do not need to be recopied
after every source-only rebuild.

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

`--qmake` regenerates the Makefiles with whatever path the script was invoked
through. The `pushd` above maps a drive letter for the UNC path first, so it is
safe; invoking `scripts\verify-windows.cmd ... --qmake` directly through
`\\wsl.localhost\...` records UNC source paths instead, and the next build then
fails with `Error: dependent '\\wsl.localhost\...' does not exist`. Use the
`pushd` form or a mapped drive path (for example `Z:\...`).

## Performance benchmarks

Image loading and decoder performance should be measured with a Release build.
See [Benchmark.md](Benchmark.md) for the benchmark suites, decoder comparison
conditions, and output format.

## Profiling first-image display

```bat
set "QV_PROFILE_FIRST_IMAGE=C:\build\qv-first-image.tsv"
start "" /wait "C:\build\quickviewer-msvc2022_64-release\bin\QuickViewer.exe" "C:\path\book.rar"
set "QV_PROFILE_FIRST_IMAGE="
```

### Folder text responsiveness

`QV_PROFILE_FOLDER_TEXT=1` extends `QV_PROFILE_FIRST_IMAGE` profiling past the
reveal until the folder list has its text. It settles the whole folder, not only
the rows on screen: `folder-text.gui-heartbeat` counts event-loop service while
that happens, `folder-text.profile-timeout` means the run never settled, and
`folder-text.idle-stop` marks the text helper exiting after its idle period.

```bat
set "QV_PROFILE_FIRST_IMAGE=C:\build\qv-folder-text.tsv"
set "QV_PROFILE_FOLDER_TEXT=1"
start "" /wait "C:\build\quickviewer-msvc2022_64-release\bin\QuickViewer.exe" "C:\path\book.zip"
set "QV_PROFILE_FOLDER_TEXT="
set "QV_PROFILE_FIRST_IMAGE="
```

The helper is the same executable in `--folder-text-helper` mode, so nothing
extra is deployed. Check a folder holding a name the UI font cannot draw: it
shows a placeholder first, then the real text once the helper answers. While it
loads, close and reopen the panel, scroll and select rows, and try a narrow
column, a fullscreen window and another screen DPI. The panel's caption names the
folder it shows, so open a folder whose own path holds such a name as well: the
caption shows the placeholder at once and the path once the helper has answered,
at every panel width.

`scripts\verify-windows.cmd debug --test windowstartup` covers the model, cache,
caption and transport cases.

## C++ lint

C++ formatting is enforced by the tracked pre-commit hook and by CI. See
[CppLint.md](CppLint.md) for the hook mechanics, the lint scope, the commands
to check or apply formatting manually, and the environment requirements.

## Interactive checks

Startup painting, fullscreen, input timing, and other visual behavior must also
be checked interactively on Windows when affected. Headless automation does
not establish visual correctness.

A check that temporarily edits the portable `quickviewer.ini` next to the
executable - it holds the history of opened volumes - must keep that file
byte-exact: it is UTF-8, while PowerShell's `Get-Content` and `Set-Content` use
the system ANSI code page by default, and one read/write round trip through
them corrupts every name that code page cannot represent, which leaves those
history entries unopenable. Copy the bytes with `[IO.File]::ReadAllBytes` and
`WriteAllBytes`, and read and write text with `[IO.File]::ReadAllText` and
`WriteAllText` plus `New-Object System.Text.UTF8Encoding($false)`.

## Traps when adding source files or tests

### A new or removed source needs `--qmake`

`verify-windows.cmd` regenerates Makefiles only when they are missing or when
`--qmake` is passed. Adding or removing a source in a `.pro` therefore leaves an
existing build tree with a stale object list, and the first symptom is a link
error about a class that clearly exists in the tree:

```text
volume.obj : error LNK2001: unresolved external symbol
    "public: __cdecl ImageDecoder::ImageDecoder(struct ImageDecodeSettings)"
fatal error LNK1120: 7 unresolved externals
```

Pass `--qmake` for the configuration you are building. Debug and Release keep
separate build trees, so forcing qmake in one does not regenerate the other.

A first `--qmake` build can still link against the old object list. `jom` may
regenerate a sub-Makefile while it is already using the dependency graph it
read at startup, so that pass links without the new object. `verify-windows.cmd`
handles both ends of that: `--qmake` also runs the `qmake_all` target, which
regenerates every sub-Makefile before the build starts, and a failed build
regenerates them and retries once. A single invocation is therefore enough, and
a failure that survives the retry is a real one.

### moc can leave a zero-byte `.moc` behind

Qt 6.11.2 `moc` can fail to parse a translation unit that contains an awkward
multi-line raw string literal before it has finished parsing the `Q_OBJECT`
class. It then writes a zero-byte `.moc` and **still exits 0**:

```text
tst_viewernavigationtest.cpp: note: No relevant classes found. No output generated.
```

The compiler accepts the empty file, so the first visible failure is a link
error about the meta-object:

```text
tst_viewernavigationtest.obj : error LNK2001: unresolved external symbol
    "public: virtual struct QMetaObject const * __cdecl ViewerNavigationTest::metaObject(void)const"
```

The message text is localized, so match on the symbol name. When a link error
names `metaObject`, `qt_metacast`, or `qt_metacall`, check the size of the
generated `.moc` before anything else. In the test file that triggered this,
the cause was an inline SVG literal inside a slot:

```cpp
const QByteArray svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="40" height="20">
    <rect width="40" height="20" fill="#4080c0"/>
</svg>)";
```

An escaped literal decodes identically and parses cleanly:

```cpp
const QByteArray svg =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"40\" height=\"20\">"
    "<rect width=\"40\" height=\"20\" fill=\"#4080c0\"/>"
    "</svg>";
```

The exact minimal trigger is not fully characterized. Verified so far: the same
literal is harmless when it appears after the `Q_OBJECT` class, a multi-line
raw string without double quotes is harmless, and a single-line raw string
containing `//` is harmless. Prefer escaped literals in any file that `moc`
processes, or move the data out of the class body.

### Which test projects pick up application sources

`tests/windowstartup` and `tests/languageswitch` include
`apps/quickviewer/QuickViewer.pro` directly, so a source added to the
application reaches those two without an edit of their own.
`tests/viewernavigation` lists application sources explicitly and needs its own
entry.
