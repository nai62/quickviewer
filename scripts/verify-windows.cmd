@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0..") do set "QV_SOURCE_DIR=%%~fI"

if "%~1"=="" goto usage
set "QV_CONFIG=%~1"
shift
set "QV_ACTION=verify"
set "QV_FORCE_QMAKE=0"
set "QV_TEST_NAME="
set "QV_TEST_FUNCTION="

if /I "%QV_CONFIG%"=="debug" (
    if not defined QV_BUILD_DIR set "QV_BUILD_DIR=C:\build\quickviewer-msvc2022_64-debug"
) else if /I "%QV_CONFIG%"=="release" (
    if not defined QV_BUILD_DIR set "QV_BUILD_DIR=C:\build\quickviewer-msvc2022_64-release"
) else (
    echo ERROR: Configuration must be debug or release.
    goto usage
)

if not defined QV_QT_DIR set "QV_QT_DIR=C:\Qt\6.11.2\msvc2022_64"
if not defined QV_VCVARS set "QV_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined QV_JOM set "QV_JOM=C:\Qt\Tools\QtCreator\bin\jom\jom.exe"

goto parse_args

:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="--qmake" goto parse_qmake
if /I "%~1"=="--build-only" goto parse_build_only
if /I "%~1"=="--build-viewer-only" goto parse_build_viewer_only
if /I "%~1"=="--tests-only" goto parse_tests_only
if /I "%~1"=="--test" goto parse_test
echo ERROR: Unknown option: %~1
goto usage

:parse_qmake
set "QV_FORCE_QMAKE=1"
shift
goto parse_args

:parse_build_only
call :set_action build
if errorlevel 1 goto usage
shift
goto parse_args

:parse_build_viewer_only
call :set_action viewer
if errorlevel 1 goto usage
shift
goto parse_args

:parse_tests_only
call :set_action tests
if errorlevel 1 goto usage
shift
goto parse_args

:parse_test
call :set_action test
if errorlevel 1 goto usage
if "%~2"=="" (
    echo ERROR: --test requires a test name.
    goto usage
)
set "QV_TEST_NAME=%~2"
shift
shift
if "%~1"=="" goto parse_args
set "QV_NEXT_ARG=%~1"
if "!QV_NEXT_ARG:~0,2!"=="--" goto parse_args
set "QV_TEST_FUNCTION=%~1"
shift
goto parse_args

:args_done
if /I "%QV_CONFIG%"=="release" (
    if /I "%QV_ACTION%"=="tests" (
        echo ERROR: --tests-only is supported only for Debug builds.
        goto usage
    )
    if /I "%QV_ACTION%"=="test" (
        echo ERROR: --test is supported only for Debug builds.
        goto usage
    )
)
if "%QV_FORCE_QMAKE%"=="1" (
    if /I "%QV_ACTION%"=="tests" (
        echo ERROR: --qmake cannot be combined with --tests-only because no build is performed.
        goto usage
    )
    if /I "%QV_ACTION%"=="test" (
        echo ERROR: --qmake cannot be combined with --test because no build is performed.
        goto usage
    )
)

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

if /I "%QV_ACTION%"=="tests" goto prepare_existing_build
if /I "%QV_ACTION%"=="test" goto prepare_existing_build

if not exist "%QV_JOM%" (
    echo ERROR: jom not found: %QV_JOM%
    echo Set QV_JOM to the full path of jom.exe.
    exit /b 2
)
if not exist "%QV_BUILD_DIR%" mkdir "%QV_BUILD_DIR%"
if errorlevel 1 exit /b 2
cd /d "%QV_BUILD_DIR%"
if errorlevel 1 exit /b 2
call :ensure_makefiles
if errorlevel 1 exit /b 2

if /I "%QV_ACTION%"=="viewer" goto build_viewer_incremental
goto build_top_level

:prepare_existing_build
if not exist "%QV_BUILD_DIR%" (
    echo ERROR: Build directory not found: %QV_BUILD_DIR%
    echo Run "%~nx0 debug" first to create and build it.
    exit /b 2
)
cd /d "%QV_BUILD_DIR%"
if errorlevel 1 exit /b 2
if /I "%QV_ACTION%"=="test" goto run_selected_test
goto run_tests

:build_top_level
echo === Incrementally building %QV_CONFIG% targets with jom ===
call :run_jom "%QV_BUILD_DIR%"
if errorlevel 1 exit /b 2
call :stage_translations
if errorlevel 1 exit /b 2

if /I "%QV_ACTION%"=="build" (
    call :stage_heif_plugin %QV_CONFIG%
    if errorlevel 1 exit /b 2
    exit /b 0
)
if /I "%QV_CONFIG%"=="release" (
    call :stage_heif_plugin release
    if errorlevel 1 exit /b 2
    exit /b 0
)
goto run_tests

:build_viewer_incremental
if not exist "%QV_BUILD_DIR%\apps\quickviewer\Makefile" (
    echo ERROR: QuickViewer Makefile was not generated under: %QV_BUILD_DIR%\apps\quickviewer
    exit /b 2
)
echo === Incrementally building QuickViewer only with jom ===
call :run_jom "%QV_BUILD_DIR%\apps\quickviewer"
if errorlevel 1 exit /b 2
call :stage_translations
if errorlevel 1 exit /b 2
call :stage_heif_plugin %QV_CONFIG%
if errorlevel 1 exit /b 2
exit /b 0

:ensure_makefiles
set "QV_NEED_QMAKE=%QV_FORCE_QMAKE%"
if not exist "%QV_BUILD_DIR%\Makefile" set "QV_NEED_QMAKE=1"
if /I "%QV_ACTION%"=="viewer" if not exist "%QV_BUILD_DIR%\apps\quickviewer\Makefile" set "QV_NEED_QMAKE=1"
if not "%QV_NEED_QMAKE%"=="1" exit /b 0

echo === Regenerating %QV_CONFIG% build with qmake -r ===
if /I "%QV_CONFIG%"=="debug" (
    "%QV_QT_DIR%\bin\qmake.exe" -r "%QV_SOURCE_DIR%\QVproject.pro" CONFIG+=debug CONFIG-=release CONFIG-=debug_and_release CONFIG-=debug_and_release_target
) else (
    "%QV_QT_DIR%\bin\qmake.exe" -r "%QV_SOURCE_DIR%\QVproject.pro" CONFIG+=release CONFIG-=debug CONFIG-=debug_and_release CONFIG-=debug_and_release_target
)
if errorlevel 1 exit /b 2
exit /b 0

:run_jom
pushd "%~1"
if errorlevel 1 exit /b 2
if defined QV_JOBS (
    echo === jom parallel jobs override: %QV_JOBS% ===
    "%QV_JOM%" -j %QV_JOBS% /f Makefile
) else (
    "%QV_JOM%" /f Makefile
)
set "QV_JOM_EXIT=!ERRORLEVEL!"
popd
exit /b !QV_JOM_EXIT!

:run_selected_test
set "QV_TEST_EXE_NAME="
if /I "%QV_TEST_NAME%"=="prefetchplanner" set "QV_TEST_EXE_NAME=tst_prefetchplannertest.exe"
if /I "%QV_TEST_NAME%"=="asynccache" set "QV_TEST_EXE_NAME=tst_asynccachetest.exe"
if /I "%QV_TEST_NAME%"=="latestresultdispatcher" set "QV_TEST_EXE_NAME=tst_latestresultdispatchertest.exe"
if /I "%QV_TEST_NAME%"=="fileloader" set "QV_TEST_EXE_NAME=tst_fileloadertest.exe"
if /I "%QV_TEST_NAME%"=="svgloader" set "QV_TEST_EXE_NAME=tst_svgloadertest.exe"
if /I "%QV_TEST_NAME%"=="viewernavigation" set "QV_TEST_EXE_NAME=tst_viewernavigationtest.exe"
if /I "%QV_TEST_NAME%"=="windowstartup" set "QV_TEST_EXE_NAME=tst_windowstartuptest.exe"
if not defined QV_TEST_EXE_NAME (
    echo ERROR: Unknown test name: %QV_TEST_NAME%
    echo Supported tests: prefetchplanner, asynccache, latestresultdispatcher, fileloader, svgloader, viewernavigation, windowstartup
    exit /b 2
)
set "PATH=%QV_QT_DIR%\bin;%QV_BUILD_DIR%\lib;%PATH%"
set "QV_TEST_FAILED=0"
call :stage_heif_plugin debug
if errorlevel 1 exit /b 2
call :run_test "%QV_TEST_EXE_NAME%" "%QV_TEST_FUNCTION%"
if not "!QV_TEST_FAILED!"=="0" exit /b 1
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
call :run_test tst_svgloadertest.exe
call :run_test tst_viewernavigationtest.exe
call :run_test tst_windowstartuptest.exe

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
if "%~2"=="" (
    "!QV_TEST_EXE!" -v1 -o "!QV_TEST_RESULT!,txt"
) else (
    "!QV_TEST_EXE!" %~2 -v1 -o "!QV_TEST_RESULT!,txt"
)
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
set "QV_TRANSLATION_SOURCE=%QV_SOURCE_DIR%\apps\quickviewer\translations"
set "QV_TRANSLATION_DEST=%QV_BUILD_DIR%\bin\translations"
if not exist "!QV_TRANSLATION_DEST!" mkdir "!QV_TRANSLATION_DEST!"
if errorlevel 1 exit /b 1
copy /Y "!QV_TRANSLATION_SOURCE!\languages.ini" "!QV_TRANSLATION_DEST!\" >nul
if errorlevel 1 exit /b 1
copy /Y "!QV_TRANSLATION_SOURCE!\qt_el.qm" "!QV_TRANSLATION_DEST!\" >nul
if errorlevel 1 exit /b 1
for %%F in ("!QV_TRANSLATION_SOURCE!\quickviewer_*.ts") do (
    if not exist "!QV_TRANSLATION_DEST!\%%~nF.qm" (
        echo ERROR: Expected generated translation catalog is missing: !QV_TRANSLATION_DEST!\%%~nF.qm
        exit /b 1
    )
)
exit /b 0

:set_action
if /I not "%QV_ACTION%"=="verify" (
    echo ERROR: Build/test modes cannot be combined: %QV_ACTION% and %~1.
    exit /b 1
)
set "QV_ACTION=%~1"
exit /b 0

:usage
echo Usage:
echo   %~nx0 debug [--qmake]
echo   %~nx0 release [--qmake]
echo   %~nx0 debug --build-only [--qmake]
echo   %~nx0 release --build-only [--qmake]
echo   %~nx0 debug --build-viewer-only [--qmake]
echo   %~nx0 release --build-viewer-only [--qmake]
echo   %~nx0 debug --tests-only
echo   %~nx0 debug --test ^<name^> [test-function]
echo.
echo Build modifiers:
echo   --qmake             Rerun qmake -r before building.
echo   --build-only        Build top-level targets and stage runtime files; do not run tests.
echo   --build-viewer-only Build only apps\quickviewer; dependencies may remain stale.
echo   --tests-only        Run the normal Debug test suite without building.
echo   --test              Run one Debug test by stable name, optionally one QtTest function.
echo.
echo Environment overrides:
echo   QV_BUILD_DIR  Build tree path. Defaults depend on debug/release.
echo   QV_QT_DIR     Qt MSVC kit root. Default: C:\Qt\6.11.2\msvc2022_64
echo   QV_VCVARS     vcvars64.bat path.
echo   QV_JOM        jom.exe path. Default: C:\Qt\Tools\QtCreator\bin\jom\jom.exe
echo   QV_JOBS       Optional positive integer passed as jom -j N. Unset = jom default.
echo.
echo Test names:
echo   prefetchplanner  asynccache  latestresultdispatcher  fileloader
echo   svgloader        viewernavigation  windowstartup
exit /b 2
