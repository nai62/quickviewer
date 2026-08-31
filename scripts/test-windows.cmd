@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0..") do set "QV_SOURCE_DIR=%%~fI"
if not defined QV_BUILD_DIR set "QV_BUILD_DIR=C:\build\quickviewer-msvc2022_64"
if not defined QV_QT_DIR set "QV_QT_DIR=C:\Qt\6.11.2\msvc2022_64"
if not defined QV_VCVARS set "QV_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%QV_VCVARS%" (
    echo ERROR: Visual Studio environment script not found: %QV_VCVARS%
    exit /b 2
)
if not exist "%QV_QT_DIR%\bin\qmake.exe" (
    echo ERROR: qmake not found under: %QV_QT_DIR%
    exit /b 2
)

call "%QV_VCVARS%"
if errorlevel 1 exit /b 2

if not exist "%QV_BUILD_DIR%" mkdir "%QV_BUILD_DIR%"
if errorlevel 1 exit /b 2
cd /d "%QV_BUILD_DIR%"
if errorlevel 1 exit /b 2

echo === Regenerating Debug build ===
"%QV_QT_DIR%\bin\qmake.exe" -r "%QV_SOURCE_DIR%\QVproject.pro" CONFIG+=debug CONFIG-=release
if errorlevel 1 exit /b 2

echo === Building Debug targets ===
nmake /f Makefile debug
if errorlevel 1 exit /b 2

set "PATH=%QV_QT_DIR%\bin;%QV_BUILD_DIR%\lib;%PATH%"
set "QV_TEST_FAILED=0"

call :run_test tst_prefetchplannertest.exe
call :run_test tst_asynccachetest.exe
call :run_test tst_latestresultdispatchertest.exe
call :run_test tst_fileloadertest.exe

if not "!QV_TEST_FAILED!"=="0" (
    echo === One or more tests failed ===
    exit /b 1
)

echo === All tests passed ===
exit /b 0

:run_test
set "QV_TEST_EXE=%QV_BUILD_DIR%\lib\%~1"
echo === %~1 ===
if not exist "!QV_TEST_EXE!" (
    echo ERROR: Expected test executable is missing: !QV_TEST_EXE!
    set "QV_TEST_FAILED=1"
    exit /b 0
)
"!QV_TEST_EXE!" -v1
if errorlevel 1 set "QV_TEST_FAILED=1"
exit /b 0
