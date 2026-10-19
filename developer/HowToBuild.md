# How to build

This document describes how to build QuickViewer from source. The supported
Windows workflow is documented first, followed by a Linux development build
and other build targets.

If you encounter a build problem, please open an issue in the repository you
are working from.

## 1. Setup

### Set up the Qt SDK

Install the Qt SDK:

https://www.qt.io/download-open-source/

QuickViewer requires Qt 6.6 or later, the first release with
`QEvent::DevicePixelRatioChange`; `QVproject.pri` stops qmake on an older
release. Only the Qt version listed in [Testing.md](Testing.md) is verified,
so use it and its MSVC kit on Windows, since the paths in the steps below
assume that installation.

### Set up Rust

QuickViewer builds the pinned `resvg` C API from source. Install a Rust
toolchain with Cargo before running qmake - the MSVC toolchain on Windows and
the default GNU toolchain on Linux; see [Testing.md](Testing.md) for the
required version. The build runs `cargo` from `PATH`; on Windows it also checks
`%USERPROFILE%\.cargo\bin\cargo.exe`.

https://rustup.rs/

### Set up the source

Clone the repository and submodules:

```shell
git clone --recurse-submodules <this repository URL>
cd <the cloned directory>
```

For an existing clone:

```shell
git submodule update --init --recursive
```

### 7-Zip archive engine

The bundled Windows `7z.dll` files in `third_party/7zip/windll` are from the
official 7-Zip 26.03 installers.
The x64 DLL came from `7z2603-x64.exe` (DLL SHA-256
`65e4c1f855f9ef6e8f0f5df8e3f27d9eb5f07311408639da0a1ca0b8f4871b0d`)
and the x86 DLL from `7z2603.exe` (DLL SHA-256
`d132e89038c802c5d5281e543a83dc407680effe0144f21b4fb431dd45fca61d`).
Both installers are published at the [7-Zip 26.03 release](https://github.com/ip7z/7zip/releases/tag/26.03).

On Linux, the fileloader build uses the official 7-Zip 26.03 source
submodule pinned at `third_party/7zip/upstream` to build the full-format
`7z.so` in the shadow build tree. The build stages it next to the viewer and
fileloader test executables and in the existing library and AppImage bundle
locations.
The Linux lib7zip loader prefers the copy beside the executable over a
system-installed `7z.so`. A C and C++ toolchain and GNU Make are required
for that source build.

`third_party/7zip/7zip` and `third_party/p7zip` are older source copies
tracked directly in this repository. Their headers are still required to
compile the existing `lib7zip` wrapper on Windows and Unix, respectively. They
are not the bundled archive engines. When updating the engine again, check
its COM interface and exported functions against these headers before changing
the DLL or shared library.

### Set up HEIC/HEIF support

HEIC/HEIF support uses
[qt-heic-image-plugin](https://github.com/novomesk/qt-heic-image-plugin).

For a Windows Release build, build that plug-in with Qt 6.11.2 under
`../../qt-heic-image-plugin/qtbuild_6.11.2` and build its bundled libheif
dependencies under `../../qt-heic-image-plugin/3rdparty/install`.

The Debug test suite requires a plug-in linked against the Debug Qt libraries.
After building the bundled libheif dependencies, run this from the repository
root:

```bat
set "QV_REPO=%CD%"
mkdir ..\..\qt-heic-image-plugin\qtbuild_6.11.2-debug
cd ..\..\qt-heic-image-plugin\qtbuild_6.11.2-debug
C:\Qt\6.11.2\msvc2022_64\bin\qmake.exe "%QV_REPO%\scripts\qt-heic-image-plugin-debug.pro"
C:\Qt\Tools\QtCreator\bin\jom\jom.exe
```

The project expects the plug-in source at `..\..\qt-heic-image-plugin`
relative to the repository. For a checkout elsewhere, add
`HEIF_SOURCE=<that directory>` to the qmake command, and set `QV_HEIF_SOURCE` to
the same directory for the verification scripts.

The Windows verification and deployment scripts stage the appropriate HEIF
plug-in and libheif runtime DLLs.

## 2. Supported Windows development workflow

Use `scripts\verify-windows.cmd` for building and testing. Builds stage the
application data but not the Qt runtime, so run `scripts\deploy-windows.cmd`
when the executable must launch from Windows Explorer or when a program that
starts from a developer prompt fails there because Qt DLLs or platform plug-ins
are missing.

[Testing.md](Testing.md) is the runbook for that workflow: supported
environment, environment variables, the complete command set, test selection,
the Debug/Release verification policy, and WSL invocation.

## 3. Linux development build

The Ubuntu CI (`.github/workflows/linux-build-and-test.yml`) builds Debug and
Release this way on Ubuntu 24.04 with Qt 6.11.2, and runs the Debug tests as
described in [Testing.md](Testing.md#linux).

Install the system packages. libheif and its decoder plug-in back the HEIF
image plug-in, TurboJPEG is loaded at run time by the JPEG decoder, and xvfb
gives the tests a display:

```bash
sudo apt-get install -y build-essential libgl1-mesa-dev libheif-dev \
  libheif-plugin-libde265 libturbojpeg xvfb
```

Install Qt 6.11.2 for `linux_gcc_64` with the Qt Image Formats and Qt SVG
modules, for example with the Qt online installer or aqtinstall, and put its
`bin` directory on `PATH`. Qt's own builds bring ICU with them; add the
directory that holds `libicui18n.so.73` to `LD_LIBRARY_PATH` when the
executables do not find it:

```bash
export PATH="$HOME/Qt/6.11.2/gcc_64/bin:$PATH"
icu=$(find "$HOME/Qt/6.11.2/gcc_64" -name 'libicui18n.so.73' -print -quit)
export LD_LIBRARY_PATH="$(dirname "$icu"):$LD_LIBRARY_PATH"
```

Install Rust 1.87.0 or newer with rustup. Then configure one shadow build tree
per configuration from the repository root and build it:

```bash
mkdir build-debug
cd build-debug
qmake -r ../QVproject.pro CONFIG+=debug CONFIG-=release \
  CONFIG-=debug_and_release CONFIG-=debug_and_release_target
make -j"$(nproc)"
```

For Release, use `build-release` with `CONFIG+=release CONFIG-=debug`. The
build compiles `7z.so` from the 7-Zip submodule (see "7-Zip archive engine"
above), so the submodules must be checked out. The executables and test
programs load the project's shared libraries from the build tree, so add its
`lib` directory to `LD_LIBRARY_PATH` before running them:

```bash
export LD_LIBRARY_PATH="$PWD/lib:$LD_LIBRARY_PATH"
bin/QuickViewer
```

The HEIF image plug-in is built separately, as the CI does: check out
[qt-heic-image-plugin](https://github.com/novomesk/qt-heic-image-plugin), build
`src/heif.cpp` as the `kimg_heif6` Qt plug-in linked with `-lheif` and the
same Debug or Release configuration, and copy `libkimg_heif6.so` into the build
tree's `lib/imageformats`. The workflow's "Build HEIF image plug-in" step holds
the project file it uses.

## 4. Other build targets

Linux distribution builds require
[linuxdeployqt](https://github.com/probonopd/linuxdeployqt) and
[appimagetool](https://github.com/AppImage/AppImageKit). The Windows scripts do
not cover this target.

### Qt Creator

Load `QVproject.pro` as the top-level project. Windows development should still
use the scripts documented in [Testing.md](Testing.md), which keep Debug and
Release separate.

### Command-line builds

For platforms without a dedicated script, the general qmake pattern is:

```shell
cd ..
mkdir build
cd build
[QTSDK]/bin/qmake -o Makefile -recursive ../quickviewer/QVproject.pro
```

This generates a Makefile for the selected compiler and qmake configuration.

## 5. Directory structure at build time

A shadow build contains:

- `bin`
- `lib`
- one directory for each subproject

`lib` contains libraries built from subprojects. `bin` contains application
executables and staged runtime data.

The maintained Windows workflow uses separate Debug and Release roots so
objects and static libraries built with different MSVC runtime settings do not
mix.

The Windows scripts stage the runtime data listed in the next section.

## 6. Directory structure at runtime

### Windows

- **database**: SQLite database containing catalogs and thumbnails
- **translations**: multi-language `.qm` files
- **QuickViewer.exe**: main application
- **portable.ini**: marker that keeps the data files below beside the
  executable; see section 8
- **quickviewer.ini**: main configuration including keyboard/mouse settings
- **progress.ini**: records the last displayed image in a volume

### Linux (.AppImage)

- **QuickViewer-XXX-AppDir**: AppImage source directory
- **AppDir/usr/bin**: executable installation destination
- **AppDir/usr/lib**: shared-library installation destination
- **AppDir/translations**: translations and `languages.ini`
- **AppDir/QuickViewer.desktop**: desktop entry
- **$HOME/.quickviewer/quickviewer.ini**: main configuration
- **$HOME/.quickviewer/progress.ini**: saved read progress
- **$HOME/.quickviewer/thumbnail.sqlite3.db**: catalog database, kept under
  its historical file name

`AppDir/usr/bin/qt.conf` is currently not used.

> A historical successfully built example (not an `.AppImage`) is available
> [here](https://github.com/kanryu/quickviewer/issues/158#issuecomment-814652108).

## 7. Selection of rendering method

QuickViewer draws its pages with the standard rendering method of the platform
(Windows GDI on Windows), or with Direct2D when that QPA plug-in is selected at
startup.

Resizing a page is a separate choice, made in the Rendering menu: either the
view scales the page while it draws it (Bilinear, Nearest Neighbor) or the page
is resized once by the CPU through zimg (Bicubic, Spline16, Spline36, Lanczos3,
Lanczos4). GDI can be competitive for 2D bilinear drawing because it avoids
transferring the image into a GPU texture.

Direct2D is implemented as a QPA plug-in and is selected when that plug-in is
enabled at startup.

## 8. Portable or system-standard installation

On Windows one build serves both the portable archive and the installed copy;
the directory a running copy keeps its settings, progress and catalog files in
is chosen when it starts:

- With a `portable.ini` beside `QuickViewer.exe`, the files are kept beside the
  executable: `quickviewer.ini`, `progress.ini` and
  `database\thumbnail.sqlite3.db`. The contents of `portable.ini` are not read.
- Without it, they are kept in `%APPDATA%\QuickViewer`: `quickviewer.ini`,
  `progress.ini` and `thumbnail.sqlite3.db`.

These are the places the last upstream release used for its portable and
installed packages, so the files of either are read as they are. The build
copies `portable.ini` into `bin`, which keeps a development build portable, and
the release staging tree carries it for the portable archive. The NSIS
installer lists its files one by one and leaves it out. A portable copy updated
by replacing `QuickViewer.exe` alone has no marker and starts with the
settings of `%APPDATA%\QuickViewer` until `portable.ini` is put back; its own
files are left as they were.

Linux and macOS always keep the files in `$HOME/.quickviewer`. A Linux release
build is packaged as an AppImage; run qmake with `CONFIG+=qv_system_install` to
install it under `/usr/local` instead.

File associations are registered in two ways. The application registers the
selected formats for the current user under `HKEY_CURRENT_USER` and then opens
the Windows Default Apps page, because Windows does not let a program pick the
default application itself. The installer registers every format under
`HKEY_LOCAL_MACHINE` for all users.

Enjoy! :)
