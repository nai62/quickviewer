# How to build

This document describes how to build QuickViewer from source. The supported
Windows workflow is documented first, followed by other build targets.

If you encounter a build problem, please open an issue in the repository you
are working from.

## 1. Setup

### Set up the Qt SDK

Install the Qt SDK:

https://www.qt.io/download-open-source/

QuickViewer requires Qt 6 or later; any Qt 6 release builds it. Use the Qt
version and MSVC kit listed in [Testing.md](Testing.md) for the verified
configuration, since the paths in the steps below assume that installation.

### Set up Rust

QuickViewer builds the pinned `resvg` C API from source. Install a Rust MSVC
toolchain with Cargo before running qmake; see [Testing.md](Testing.md) for the
required version. On Windows, the build also checks
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

### Set up HEIC/HEIF support

HEIC/HEIF support uses
[qt-heic-image-plugin](https://github.com/novomesk/qt-heic-image-plugin).

For a Windows Release build, build that plug-in with Qt 6.11.2 under
`../../qt-heic-image-plugin/qtbuild_6.11.2` and build its bundled libheif
dependencies under `../../qt-heic-image-plugin/3rdparty/install`.

The Debug test suite requires a plug-in linked against the Debug Qt libraries.
After building the bundled libheif dependencies:

```bat
mkdir ..\..\qt-heic-image-plugin\qtbuild_6.11.2-debug
cd ..\..\qt-heic-image-plugin\qtbuild_6.11.2-debug
C:\Qt\6.11.2\msvc2022_64\bin\qmake.exe ^
  ..\..\others\quickviewer\scripts\qt-heic-image-plugin-debug.pro
C:\Qt\Tools\QtCreator\bin\jom\jom.exe
```

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

## 3. Other build targets

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

## 4. Directory structure at build time

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

## 5. Directory structure at runtime

### Windows

- **database**: SQLite database containing catalogs and thumbnails
- **translations**: multi-language `.qm` files
- **QuickViewer.exe**: main application
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

## 6. Selection of rendering method

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

## 7. Portable or system-standard installation

`QV_PORTABLE` in `QVproject.pri` selects portable operation versus a
system-style installation.

When `QV_PORTABLE` is defined, QuickViewer is intended to remain portable and
keep data files with the application; Linux AppImage builds use this mode.

When it is not defined, QuickViewer follows platform installation conventions,
such as `C:\Program Files` on Windows and `/usr/local/bin` on Linux.

On Windows the define also decides where a running copy keeps its settings,
progress and catalog files: a build with `QV_PORTABLE` writes them beside the
executable and a build without it writes them into the user's data directory.
The installation the NSIS installer lays down is therefore staged from a build
made without `QV_PORTABLE`, and the portable archive from one made with it.

File associations follow the same split. The application registers the
selected formats for the current user under `HKEY_CURRENT_USER` and then opens
the Windows Default Apps page, because Windows does not let a program pick the
default application itself. The installer registers every format under
`HKEY_LOCAL_MACHINE` for all users.

Enjoy! :)
