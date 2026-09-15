@echo off
setlocal EnableExtensions

for %%I in ("%~dp0..") do set "QV_SOURCE_DIR=%%~fI"

if not defined QV_QT_DIR set "QV_QT_DIR=C:\Qt\6.11.2\msvc2022_64"

set "QV_LUPDATE_PRO=%QV_QT_DIR%\bin\lupdate-pro.exe"
set "QV_PROJECT=%QV_SOURCE_DIR%\QVproject.pro"

if not exist "%QV_LUPDATE_PRO%" (
    echo ERROR: lupdate-pro not found: %QV_LUPDATE_PRO%
    exit /b 2
)

if not exist "%QV_PROJECT%" (
    echo ERROR: QVproject.pro not found: %QV_PROJECT%
    exit /b 2
)

echo === Updating Qt translation source files ===
echo Project: %QV_PROJECT%
echo lupdate-pro: %QV_LUPDATE_PRO%
echo.

"%QV_LUPDATE_PRO%" "%QV_PROJECT%"
if errorlevel 1 (
    echo.
    echo ERROR: lupdate-pro failed.
    exit /b 1
)

echo.
echo === Translation source files updated ===
echo Review changes under:
echo   %QV_SOURCE_DIR%\apps\quickviewer\translations
echo.
echo This command updates .ts files only.
echo It does not generate .qm files.
exit /b 0
