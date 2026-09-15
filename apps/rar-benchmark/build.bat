@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0..\..") do set "QV_SOURCE_DIR=%%~fI"

if "%~1"=="" (
    set "QV_CONFIG=release"
) else if /I "%~1"=="debug" (
    set "QV_CONFIG=debug"
) else if /I "%~1"=="release" (
    set "QV_CONFIG=release"
) else (
    goto usage
)

if not defined QV_QT_DIR set "QV_QT_DIR=C:\Qt\6.11.2\msvc2022_64"
if not defined QV_VCVARS set "QV_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined QV_JOM set "QV_JOM=C:\Qt\Tools\QtCreator\bin\jom\jom.exe"
if not defined QV_RAR_BENCHMARK_BUILD_DIR set "QV_RAR_BENCHMARK_BUILD_DIR=C:\build\quickviewer-rar-benchmark-msvc2022_64-%QV_CONFIG%"

if not exist "%QV_VCVARS%" (
    echo ERROR: Visual Studio environment script not found: %QV_VCVARS%
    exit /b 2
)
if not exist "%QV_QT_DIR%\bin\qmake.exe" (
    echo ERROR: qmake not found under: %QV_QT_DIR%
    exit /b 2
)
if not exist "%QV_JOM%" (
    echo ERROR: jom not found: %QV_JOM%
    exit /b 2
)

call "%QV_VCVARS%"
if errorlevel 1 exit /b 2

set "QV_UNRAR_BUILD_DIR=%QV_RAR_BENCHMARK_BUILD_DIR%\components\rarextractor"
set "QV_BENCHMARK_BUILD_DIR=%QV_RAR_BENCHMARK_BUILD_DIR%\apps\rar-benchmark"

if not exist "%QV_UNRAR_BUILD_DIR%" mkdir "%QV_UNRAR_BUILD_DIR%"
if errorlevel 1 exit /b 2
if not exist "%QV_BENCHMARK_BUILD_DIR%" mkdir "%QV_BENCHMARK_BUILD_DIR%"
if errorlevel 1 exit /b 2

echo === Building unrar ^(%QV_CONFIG%^) ===
pushd "%QV_UNRAR_BUILD_DIR%"
if errorlevel 1 exit /b 2
call :run_qmake "%QV_SOURCE_DIR%\components\rarextractor\unrar.pro"
if errorlevel 1 goto build_failed
call :run_jom
if errorlevel 1 goto build_failed
popd

echo === Building rar-benchmark ^(%QV_CONFIG%^) ===
pushd "%QV_BENCHMARK_BUILD_DIR%"
if errorlevel 1 exit /b 2
call :run_qmake "%QV_SOURCE_DIR%\apps\rar-benchmark\rar-benchmark.pro"
if errorlevel 1 goto build_failed
call :run_jom
if errorlevel 1 goto build_failed
popd

echo.
echo Build succeeded:
echo   %QV_RAR_BENCHMARK_BUILD_DIR%\bin\rar-benchmark.exe
exit /b 0

:run_qmake
if /I "%QV_CONFIG%"=="debug" (
    "%QV_QT_DIR%\bin\qmake.exe" "%~1" CONFIG+=debug CONFIG-=release CONFIG-=debug_and_release CONFIG-=debug_and_release_target
) else (
    "%QV_QT_DIR%\bin\qmake.exe" "%~1" CONFIG+=release CONFIG-=debug CONFIG-=debug_and_release CONFIG-=debug_and_release_target
)
exit /b %ERRORLEVEL%

:run_jom
if defined QV_JOBS (
    "%QV_JOM%" -j %QV_JOBS% /f Makefile
) else (
    "%QV_JOM%" /f Makefile
)
exit /b %ERRORLEVEL%

:build_failed
set "QV_BUILD_EXIT=!ERRORLEVEL!"
popd
echo.
echo ERROR: Build failed with exit code !QV_BUILD_EXIT!.
exit /b !QV_BUILD_EXIT!

:usage
echo Usage: %~nx0 [debug^|release]
echo.
echo Default configuration: release
exit /b 2
