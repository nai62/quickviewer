@echo off
setlocal EnableExtensions

for %%I in ("%~dp0..") do set "QV_SOURCE_DIR=%%~fI"

if /I "%~1"=="debug" (
    set "QV_CONFIG=debug"
    if not defined QV_BUILD_DIR set "QV_BUILD_DIR=C:\build\quickviewer-msvc2022_64-debug"
) else if /I "%~1"=="release" (
    set "QV_CONFIG=release"
    if not defined QV_BUILD_DIR set "QV_BUILD_DIR=C:\build\quickviewer-msvc2022_64-release"
) else (
    echo ERROR: Configuration must be debug or release.
    echo Usage: %~nx0 debug ^| release
    exit /b 2
)

if not defined QV_QT_DIR set "QV_QT_DIR=C:\Qt\6.11.2\msvc2022_64"
if not defined QV_VCVARS set "QV_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined QV_HEIF_SOURCE set "QV_HEIF_SOURCE=%QV_SOURCE_DIR%\..\..\qt-heic-image-plugin"

set "QV_EXE=%QV_BUILD_DIR%\bin\QuickViewer.exe"

if not exist "%QV_VCVARS%" (
    echo ERROR: Visual Studio environment script not found: %QV_VCVARS%
    exit /b 2
)
if not exist "%QV_QT_DIR%\bin\windeployqt.exe" (
    echo ERROR: windeployqt not found under: %QV_QT_DIR%
    exit /b 2
)
if not exist "%QV_EXE%" (
    echo ERROR: QuickViewer executable not found: %QV_EXE%
    echo Build it first with:
    echo   scripts\verify-windows.cmd %QV_CONFIG%
    exit /b 2
)

call "%QV_VCVARS%"
if errorlevel 1 exit /b 2

if /I "%QV_CONFIG%"=="debug" (
    set "QV_HEIF_PLUGIN=%QV_HEIF_SOURCE%\qtbuild_6.11.2-debug\kimg_heif6.dll"
    set "QV_DEPLOY_FLAG=--debug"
) else (
    set "QV_HEIF_PLUGIN=%QV_HEIF_SOURCE%\qtbuild_6.11.2\kimg_heif6.dll"
    set "QV_DEPLOY_FLAG=--release"
)

if not exist "%QV_HEIF_PLUGIN%" (
    echo ERROR: HEIF %QV_CONFIG% plug-in not found: %QV_HEIF_PLUGIN%
    exit /b 2
)

echo === Deploying Qt runtime for %QV_CONFIG% ===
"%QV_QT_DIR%\bin\windeployqt.exe" %QV_DEPLOY_FLAG% --compiler-runtime "%QV_EXE%"
if errorlevel 1 exit /b 2

echo === Staging HEIF support ===
if not exist "%QV_BUILD_DIR%\bin\imageformats" mkdir "%QV_BUILD_DIR%\bin\imageformats"
copy /Y "%QV_HEIF_PLUGIN%" "%QV_BUILD_DIR%\bin\imageformats\" >nul
if errorlevel 1 exit /b 2
copy /Y "%QV_HEIF_SOURCE%\3rdparty\install\bin\*.dll" "%QV_BUILD_DIR%\bin\" >nul
if errorlevel 1 exit /b 2

echo === Deployment complete ===
echo %QV_EXE%
exit /b 0
