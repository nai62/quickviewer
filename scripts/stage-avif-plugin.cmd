@echo off
setlocal EnableExtensions
rem Arguments: configuration, destination directory.
if not defined QV_AVIF_SOURCE set "QV_AVIF_SOURCE=%~dp0..\..\..\qt-avif-image-plugin"
if not defined QV_AVIF_DEPS set "QV_AVIF_DEPS=%QV_AVIF_SOURCE%\3rdparty\install"
if /I "%~1"=="debug" (
    set "QV_AVIF_PLUGIN=%QV_AVIF_SOURCE%\qtbuild_6.11.2-debug\qavif6.dll"
) else (
    set "QV_AVIF_PLUGIN=%QV_AVIF_SOURCE%\qtbuild_6.11.2\qavif6.dll"
)
if not exist "%QV_AVIF_PLUGIN%" (
    echo ERROR: AVIF %~1 plug-in not found: %QV_AVIF_PLUGIN%
    exit /b 2
)
if not exist "%QV_AVIF_DEPS%\bin\avif.dll" (
    echo ERROR: AVIF runtime not found under: %QV_AVIF_DEPS%\bin
    exit /b 2
)
if not exist "%~2\imageformats" mkdir "%~2\imageformats"
if errorlevel 1 exit /b 2
copy /Y "%QV_AVIF_PLUGIN%" "%~2\imageformats\" >nul
if errorlevel 1 exit /b 2
copy /Y "%QV_AVIF_DEPS%\bin\*.dll" "%~2\" >nul
if errorlevel 1 exit /b 2
exit /b 0
