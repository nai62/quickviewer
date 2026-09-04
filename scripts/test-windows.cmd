@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0..") do set "QV_SOURCE_DIR=%%~fI"
if not defined QV_BUILD_DIR (
    if /I "%~1"=="--release-only" (
        set "QV_BUILD_DIR=C:\build\quickviewer-msvc2022_64-release"
    ) else (
        set "QV_BUILD_DIR=C:\build\quickviewer-msvc2022_64-debug"
    )
)
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

if /I "%~1"=="--tests-only" goto run_tests
if /I "%~1"=="--build-viewer-only" goto build_viewer_incremental
if /I "%~1"=="--viewer-only" (
    set "PATH=%QV_QT_DIR%\bin;%QV_BUILD_DIR%\lib;%PATH%"
    set "QV_TEST_FAILED=0"
    call :run_test tst_viewernavigationtest.exe %~2
    if not "!QV_TEST_FAILED!"=="0" exit /b 1
    exit /b 0
)
if /I "%~1"=="--release-only" goto build_release
if /I "%~1"=="--full" goto build_debug_full

echo ERROR: Select an explicit verification mode.
echo Usage: %~nx0 --build-viewer-only ^| --viewer-only [test-function] ^| --tests-only ^| --full ^| --release-only
exit /b 2

:build_debug_full
echo === Regenerating Debug build ===
"%QV_QT_DIR%\bin\qmake.exe" -r "%QV_SOURCE_DIR%\QVproject.pro" CONFIG+=debug CONFIG-=release
if errorlevel 1 exit /b 2

echo === Building Debug targets ===
nmake /f Makefile debug
if errorlevel 1 exit /b 2
call :stage_translations
if errorlevel 1 exit /b 2
goto run_tests

:build_viewer_incremental
if not defined QV_JOM set "QV_JOM=C:\Qt\Tools\QtCreator\bin\jom\jom.exe"
if not defined QV_JOBS set "QV_JOBS=8"
if not exist "%QV_JOM%" (
    echo ERROR: jom not found: %QV_JOM%
    exit /b 2
)
if not exist "%QV_BUILD_DIR%\QuickViewer\Makefile.Debug" (
    echo ERROR: Configured QuickViewer Debug build not found under: %QV_BUILD_DIR%
    echo Run the full Debug build explicitly once to initialize it.
    exit /b 2
)
echo === Incrementally building QuickViewer with %QV_JOBS% jobs ===
cd /d "%QV_BUILD_DIR%\QuickViewer"
if errorlevel 1 exit /b 2
"%QV_JOM%" -j %QV_JOBS% /f Makefile.Debug
if errorlevel 1 exit /b 2
call :stage_translations
if errorlevel 1 exit /b 2
exit /b 0

:build_release
echo === Regenerating Release build ===
"%QV_QT_DIR%\bin\qmake.exe" -r "%QV_SOURCE_DIR%\QVproject.pro" CONFIG+=release CONFIG-=debug
if errorlevel 1 exit /b 2

echo === Building Release targets ===
nmake /f Makefile release
if errorlevel 1 exit /b 2
call :stage_translations
if errorlevel 1 exit /b 2
call :stage_heif_plugin release
if errorlevel 1 exit /b 2
exit /b 0

:run_tests
set "PATH=%QV_QT_DIR%\bin;%QV_BUILD_DIR%\lib;%PATH%"
set "QV_TEST_FAILED=0"
call :stage_heif_plugin debug
if errorlevel 1 exit /b 2

call :run_test tst_prefetchplannertest.exe
call :run_test tst_asynccachetest.exe
call :run_test tst_latestresultdispatchertest.exe
call :run_test tst_fileloadertest.exe
call :run_test tst_viewernavigationtest.exe

if not "!QV_TEST_FAILED!"=="0" (
    echo === One or more tests failed ===
    exit /b 1
)

echo === All tests passed ===
exit /b 0

:run_test
set "QV_TEST_EXE=%QV_BUILD_DIR%\lib\%~1"
set "QV_TEST_RESULT_DIR=%QV_BUILD_DIR%\test-results"
set "QV_TEST_RESULT=!QV_TEST_RESULT_DIR!\%~n1.txt"
echo === %~1 ===
if not exist "!QV_TEST_EXE!" (
    echo ERROR: Expected test executable is missing: !QV_TEST_EXE!
    set "QV_TEST_FAILED=1"
    exit /b 0
)
if not exist "!QV_TEST_RESULT_DIR!" mkdir "!QV_TEST_RESULT_DIR!"
if exist "!QV_TEST_RESULT!" del /Q "!QV_TEST_RESULT!"
"!QV_TEST_EXE!" %~2 -v1 -o "!QV_TEST_RESULT!,txt"
set "QV_TEST_EXIT=!ERRORLEVEL!"
if exist "!QV_TEST_RESULT!" type "!QV_TEST_RESULT!"
if not "!QV_TEST_EXIT!"=="0" set "QV_TEST_FAILED=1"
exit /b 0

:stage_heif_plugin
if not defined QV_HEIF_SOURCE set "QV_HEIF_SOURCE=%QV_SOURCE_DIR%\..\..\qt-heic-image-plugin"
if /I "%~1"=="debug" (
    set "QV_HEIF_PLUGIN=%QV_HEIF_SOURCE%\qtbuild_6.11.2-debug\kimg_heif6.dll"
) else (
    set "QV_HEIF_PLUGIN=%QV_HEIF_SOURCE%\qtbuild_6.11.2\kimg_heif6.dll"
)
if not exist "%QV_HEIF_PLUGIN%" (
    echo ERROR: HEIF %~1 plug-in not found: %QV_HEIF_PLUGIN%
    exit /b 2
)
for %%D in ("%QV_BUILD_DIR%\bin" "%QV_BUILD_DIR%\lib") do (
    if not exist "%%~D\imageformats" mkdir "%%~D\imageformats"
    copy /Y "%QV_HEIF_PLUGIN%" "%%~D\imageformats\" >nul
    if errorlevel 1 exit /b 2
    copy /Y "%QV_HEIF_SOURCE%\3rdparty\install\bin\*.dll" "%%~D\" >nul
    if errorlevel 1 exit /b 2
)
exit /b 0

:stage_translations
set "QV_TRANSLATION_SOURCE=%QV_SOURCE_DIR%\QuickViewer\translations"
set "QV_TRANSLATION_DEST=%QV_BUILD_DIR%\bin\translations"
if not exist "!QV_TRANSLATION_DEST!" mkdir "!QV_TRANSLATION_DEST!"
if errorlevel 1 exit /b 1
copy /Y "!QV_TRANSLATION_SOURCE!\languages.ini" "!QV_TRANSLATION_DEST!\" >nul
if errorlevel 1 exit /b 1
copy /Y "!QV_TRANSLATION_SOURCE!\quickviewer_*.qm" "!QV_TRANSLATION_DEST!\" >nul
if errorlevel 1 exit /b 1
copy /Y "!QV_TRANSLATION_SOURCE!\qt_el.qm" "!QV_TRANSLATION_DEST!\" >nul
if errorlevel 1 exit /b 1
if not exist "!QV_TRANSLATION_DEST!\quickviewer_ja.qm" exit /b 1
exit /b 0
