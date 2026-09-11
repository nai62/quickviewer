@echo off
setlocal EnableExtensions

for %%I in ("%~dp0..") do set "QV_SOURCE_DIR=%%~fI"

if not defined QV_QT_DIR set "QV_QT_DIR=C:\Qt\6.11.2\msvc2022_64"

set "QV_LUPDATE=%QV_QT_DIR%\bin\lupdate.exe"
set "QV_PROJECT=%QV_SOURCE_DIR%\QVproject.pro"

if not exist "%QV_LUPDATE%" (
    echo ERROR: lupdate not found: %QV_LUPDATE%
    exit /b 2
)

if not exist "%QV_PROJECT%" (
    echo ERROR: QVproject.pro not found: %QV_PROJECT%
    exit /b 2
)

echo === Updating Qt translation source files ===
echo Project: %QV_PROJECT%
echo lupdate: %QV_LUPDATE%
echo.

"%QV_LUPDATE%" "%QV_PROJECT%"
if errorlevel 1 (
    echo.
    echo ERROR: lupdate failed.
    exit /b 1
)

echo.
echo === Translation source files updated ===
echo Review changes under:
echo   %QV_SOURCE_DIR%\QuickViewer\translations
echo.
echo This command updates .ts files only.
echo It does not generate .qm files.
exit /b 0
