@echo off
setlocal enabledelayedexpansion

rem ============================================================================
rem AutoReflex - build + deploy
rem
rem Builds Release x64 and copies AutoReflex.dll into the POEFixer plugins
rem directory. Run from a regular shell — no VS Developer Prompt required.
rem
rem Usage:
rem   build.bat           (Release, default)
rem   build.bat Debug     (Debug)
rem   build.bat clean     (clean obj/x64 outputs)
rem
rem Note: paths produced by vswhere contain "(x86)". Inside any if-block we
rem must reference them with delayed expansion (!VAR!) so the `)` from "(x86)"
rem doesn't close the block early at parse time.
rem ============================================================================

set "PROJECT_DIR=%~dp0"
set "PROJECT_FILE=%PROJECT_DIR%AutoReflex.vcxproj"
set "DEPLOY_DIR=C:\auto_help\POE_FIXER\Plugins\AutoReflex"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"

if /i "%CONFIG%"=="clean"   goto :do_clean
if /i "%CONFIG%"=="Release" goto :have_config
if /i "%CONFIG%"=="Debug"   goto :have_config

echo [AutoReflex] Unknown configuration "%CONFIG%". Use Release or Debug.
exit /b 1

:have_config

rem --- Locate vswhere ---------------------------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
    echo [AutoReflex] vswhere.exe not found.
    echo               Install Visual Studio 2019/2022 or VS Build Tools.
    exit /b 1
)

rem --- Locate MSBuild via vswhere ---------------------------------------------
set "TMPFILE=%TEMP%\autoreflex_msbuild_path.txt"
"!VSWHERE!" -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" > "!TMPFILE!"
set /p MSBUILD=<"!TMPFILE!"
del "!TMPFILE!" >nul 2>&1

if not defined MSBUILD (
    echo [AutoReflex] MSBuild not found via vswhere.
    exit /b 1
)
if not exist "!MSBUILD!" (
    echo [AutoReflex] MSBuild path resolved but file does not exist.
    exit /b 1
)

echo [AutoReflex] MSBuild:    !MSBUILD!
echo [AutoReflex] Project:    !PROJECT_FILE!
echo [AutoReflex] Config:     !CONFIG! ^| x64
echo [AutoReflex] Deploy to:  !DEPLOY_DIR!
echo.

rem --- Build ------------------------------------------------------------------
"!MSBUILD!" "!PROJECT_FILE!" /p:Configuration=!CONFIG! /p:Platform=x64 /m /nologo /v:minimal
if errorlevel 1 (
    echo.
    echo [AutoReflex] Build FAILED.
    exit /b 1
)

rem --- Deploy -----------------------------------------------------------------
set "OUT_DIR=%PROJECT_DIR%x64\%CONFIG%\Plugins\AutoReflex"
set "OUT_DLL=!OUT_DIR!\AutoReflex.dll"
set "OUT_PDB=!OUT_DIR!\AutoReflex.pdb"

if not exist "!OUT_DLL!" (
    echo [AutoReflex] Build succeeded but DLL not found.
    exit /b 1
)

if not exist "!DEPLOY_DIR!" mkdir "!DEPLOY_DIR!"

copy /y "!OUT_DLL!" "!DEPLOY_DIR!\" >nul
if errorlevel 1 (
    echo [AutoReflex] Failed to copy DLL. Is POEFixer running with the DLL loaded?
    exit /b 1
)

if exist "!OUT_PDB!" copy /y "!OUT_PDB!" "!DEPLOY_DIR!\" >nul

rem --- Mirror rules so a fresh deploy works out of the box -------------------
if exist "!PROJECT_DIR!rules" (
    if not exist "!DEPLOY_DIR!\rules" mkdir "!DEPLOY_DIR!\rules"
    xcopy /y /q "!PROJECT_DIR!rules\*.json" "!DEPLOY_DIR!\rules\" >nul 2>&1
)

echo.
echo [AutoReflex] Deployed AutoReflex.dll -^> !DEPLOY_DIR!
echo [AutoReflex] Done.
exit /b 0

:do_clean
echo [AutoReflex] Cleaning...
if exist "%PROJECT_DIR%obj" rmdir /s /q "%PROJECT_DIR%obj"
if exist "%PROJECT_DIR%x64" rmdir /s /q "%PROJECT_DIR%x64"
echo [AutoReflex] Clean done.
exit /b 0
