# How to build

This document was originally written to make building QuickViewer from source
easier to understand. The historical cross-platform guidance is retained, while
the supported Windows development workflow is documented first.

If you encounter a build problem, please open an issue in the repository you
are working from.

## 1. Setup

### Set up the Qt SDK

Install the Qt SDK:

https://www.qt.io/download-open-source/

The currently supported Windows verification environment uses Qt 6.11.2 for
MSVC 2022 x64. See [Testing.md](Testing.md) for exact development settings.

### Set up Rust

QuickViewer builds the pinned `resvg` C API from source. Install a Rust MSVC
toolchain with Cargo before running qmake. `resvg` 0.47.0 requires Rust 1.87.0
or newer. On Windows, the build also checks
`%USERPROFILE%\.cargo\bin\cargo.exe`.

https://rustup.rs/

### Set up the source

Clone the repository and submodules:

```shell
git clone --recurse-submodules https://github.com/kanryu/quickviewer
cd quickviewer
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
nmake
```

The Windows verification and deployment scripts stage the appropriate HEIF
plug-in and libheif runtime DLLs.

## 2. Supported Windows development workflow

For ordinary Windows development, use the repository scripts instead of
invoking recursive qmake manually.

Build Debug and run the automated test suite:

```bat
scripts\verify-windows.cmd debug
```

Build Release:

```bat
scripts\verify-windows.cmd release
```

For a fast edit/build cycle after the Debug tree has been initialized:

```bat
scripts\verify-windows.cmd debug --build-viewer-only
```

A build does not copy Qt runtime DLLs into `bin`. To make the built application
launch directly from Windows Explorer:

```bat
scripts\deploy-windows.cmd debug
scripts\deploy-windows.cmd release
```

Default build trees:

```text
C:\build\quickviewer-msvc2022_64-debug
C:\build\quickviewer-msvc2022_64-release
```

See [Testing.md](Testing.md) for targeted tests, overrides, WSL invocation, and
verification policy.

## 3. Other and historical build methods

The following sections preserve the project's earlier guidance. They can still
be useful for other platforms or toolchains, but the MSVC 2022 workflow above
is the maintained Windows development path.

Linux distribution builds require
[linuxdeployqt](https://github.com/probonopd/linuxdeployqt) and
[appimagetool](https://github.com/AppImage/AppImageKit).

### Qt Creator

QuickViewer has historically been developed with Qt Creator. Load
`QVproject.pro` as the top-level project.

Older versions generated a distribution package by adding a Make build step
with `install` as its argument. Current Windows development should use
`verify-windows.cmd` and `deploy-windows.cmd` so Debug and Release remain
explicitly separated.

### Command-line builders

The general qmake pattern is:

```shell
cd ..
mkdir build
cd build
[QTSDK]/bin/qmake -o Makefile -recursive ../quickviewer/QVproject.pro
```

This generates a Makefile for the selected compiler and qmake configuration.

#### MinGW

```shell
mingw32-make
mingw32-make install
```

#### Legacy Visual Studio / nmake workflow

Historically, Visual Studio 2015 used the matching developer environment:

```shell
[amd64]/vsvars64.bat
nmake
nmake install
```

For current Windows development, use the MSVC 2022 scripts above.

### Legacy Visual Studio project generation

The original Qt VS Tools workflow was:

1. Install Visual Studio, the matching Qt SDK, and Qt VS Tools.
2. Select **Qt VS Tools -> Open Qt Project File (.pro)**.
3. Select `QVproject.pro`.

Historical command-line generation:

```shell
cd ..
mkdir build
cd build
[QTSDK]/bin/qmake -tp vc ..\quickviewer\QVproject.pro -recursive QMAKE_INCDIR_QT=$(QTDIR)\include QMAKE_LIBDIR=$(QTDIR)\lib QMAKE_MOC=$(QTDIR)\bin\moc.exe QMAKE_QMAKE=$(QTDIR)\bin\qmake.exe
```

If a Windows executable works from a developer prompt but fails from Explorer
because Qt DLLs or platform plug-ins are missing, use:

```bat
scripts\deploy-windows.cmd debug
```

or:

```bat
scripts\deploy-windows.cmd release
```

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

Historically, some runtime source directories were linked manually:

```shell
cd [build]/bin
ln -s ../../quickviewer/QuickViewer/database database
ln -s ../../quickviewer/QuickViewer/shaders shaders
ln -s ../../quickviewer/QuickViewer/translations translations
```

The current Windows scripts stage translations automatically.

## 5. Directory structure at runtime

### Windows

- **database**: SQLite database containing catalogs and thumbnails
- **shaders**: fragment shaders for image resizing (obsolete)
- **translations**: multi-language `.qm` files
- **QuickViewer.exe**: main application
- **AssociateFilesWithQuickViewer.exe**: configures image associations with UAC
- **quickviewer.ini**: main configuration including keyboard/mouse settings
- **progress.ini**: records the last displayed image in a volume

### Linux (.AppImage)

Linux builds require `linuxdeployqt` and `appimagetool`.

- **QuickViewer-XXX-AppDir**: AppImage source directory
- **AppDir/usr/bin**: executable installation destination
- **AppDir/usr/lib**: shared-library installation destination
- **AppDir/translations**: translations and `languages.ini`
- **AppDir/QuickViewer.desktop**: desktop entry
- **$HOME/.quickviewer/quickviewer.ini**: main configuration
- **$HOME/.quickviewer/progress.ini**: saved read progress
- **$HOME/.quickviewer/thumbnail.sqlite3.db**: catalog/thumbnail database

`AppDir/usr/bin/qt.conf` is currently not used.

> A historical successfully built example (not an `.AppImage`) is available
> [here](https://github.com/kanryu/quickviewer/issues/158#issuecomment-814652108).

## 6. Selection of rendering method

QuickViewer has historically rendered images primarily through:

1. the standard rendering method of each OS (Windows GDI on Windows)
2. OpenGL
3. Direct2D

To enable OpenGL, comment out `QV_WITHOUT_OPENGL` in `QVproject.pri`.

The original project notes observed that GDI can be competitive for 2D
bilinear drawing because it avoids transferring the image into a GPU texture.

Direct2D is implemented as a QPA plug-in and is selected when that plug-in is
enabled at startup. Other operating systems may similarly use other QPA
implementations.

## 7. Portable or system-standard installation

`QV_PORTABLE` in `QVproject.pri` selects portable operation versus a
system-style installation.

When `QV_PORTABLE` is defined, QuickViewer is intended to remain portable and
keep data files with the application where practical. Historical Linux and
macOS distribution targets used `.AppImage` and `.dmg`.

When it is not defined, QuickViewer follows platform installation conventions,
historically including `C:\Program Files` on Windows and `/usr/local/bin` on
Linux.

Enjoy! :)
