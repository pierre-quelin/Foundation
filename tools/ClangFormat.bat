@ECHO OFF
SETLOCAL enabledelayedexpansion
REM Apply or check clang-format using MSYS2 ucrt64 (Clang 19+ options in /.clang-format).
REM
REM Usage (from repo root or tools/) :
REM   tools\ClangFormat.bat              dry-run on src\ (+ tests\ if present)
REM   tools\ClangFormat.bat -i           write changes on default trees
REM   tools\ClangFormat.bat -i path\to\file.cpp
REM   tools\ClangFormat.bat path\dir     dry-run on that path (file or directory)
REM
REM Env : CLANG_FORMAT = full path to clang-format.exe (optional override)

SET "ROOT=%~dp0.."
PUSHD "%ROOT%" >nul || EXIT /B 1

IF EXIST "C:\msys64\ucrt64\bin" SET "PATH=C:\msys64\ucrt64\bin;%PATH%"

SET "CF="
IF DEFINED CLANG_FORMAT IF EXIST "%CLANG_FORMAT%" SET "CF=%CLANG_FORMAT%"
IF NOT DEFINED CF IF EXIST "C:\msys64\ucrt64\bin\clang-format.exe" SET "CF=C:\msys64\ucrt64\bin\clang-format.exe"
IF NOT DEFINED CF (
    WHERE clang-format >nul 2>&1
    IF NOT ERRORLEVEL 1 FOR /F "delims=" %%A IN ('WHERE clang-format') DO IF NOT DEFINED CF SET "CF=%%A"
)

IF NOT DEFINED CF (
    ECHO [ERROR] clang-format not found.
    ECHO         Install: pacman -S mingw-w64-ucrt-x86_64-clang
    ECHO         Or set CLANG_FORMAT=C:\path\to\clang-format.exe
    POPD
    EXIT /B 1
)

ECHO [INFO] using "%CF%"
"%CF%" --version

SET "MODE=dry"
SET "TARGETS="
:Parse
IF "%~1"=="" GOTO :Parsed
IF /I "%~1"=="-i" (
    SET "MODE=write"
    SHIFT
    GOTO :Parse
)
IF /I "%~1"=="--apply" (
    SET "MODE=write"
    SHIFT
    GOTO :Parse
)
IF /I "%~1"=="-h" GOTO :Usage
IF /I "%~1"=="--help" GOTO :Usage
SET "TARGETS=!TARGETS! "%~1""
SHIFT
GOTO :Parse
:Parsed

IF "!TARGETS!"=="" (
    IF EXIST "src" SET "TARGETS=src"
    IF EXIST "tests" SET "TARGETS=!TARGETS! tests"
)

IF "!TARGETS!"=="" (
    ECHO [ERROR] no targets ^(pass a path or run from a tree with src\^)
    POPD
    EXIT /B 1
)

SET "FAILED=0"
SET "COUNT=0"

FOR %%T IN (!TARGETS!) DO (
    SET "T=%%~T"
    IF EXIST "!T!\*" (
        REM cmd FOR /R ignores delayed-expansion paths; PUSHD then FOR /R from cwd.
        REM Also loop extensions: FOR /R only honors the first wildcard in the set.
        PUSHD "!T!" >nul
        FOR %%E IN (cpp cxx cc h hpp hxx) DO (
            FOR /R %%F IN (*.%%E) DO CALL :OneFile "%%F"
        )
        POPD >nul
    ) ELSE IF EXIST "!T!" (
        CALL :OneFile "!T!"
    ) ELSE (
        ECHO [WARN] skip missing "!T!"
    )
)

ECHO [INFO] files checked/updated: !COUNT!
POPD
IF "!FAILED!"=="1" EXIT /B 1
EXIT /B 0

:OneFile
SET /A COUNT+=1
IF /I "!MODE!"=="write" (
    "%CF%" -style=file -i %1
    IF ERRORLEVEL 1 (
        ECHO [ERROR] format failed: %1
        SET "FAILED=1"
    )
) ELSE (
    "%CF%" -style=file --dry-run -Werror %1 >nul 2>&1
    IF ERRORLEVEL 1 (
        ECHO [DIFF] %1
        SET "FAILED=1"
    )
)
GOTO :EOF

:Usage
ECHO Usage: %~nx0 [-i^|--apply] [path ...]
ECHO   default paths: src\ and tests\ ^(if present^)
ECHO   default mode: dry-run ^(-Werror^); -i writes files
POPD
EXIT /B 0
