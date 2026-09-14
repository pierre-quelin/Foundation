@ECHO OFF
SETLOCAL enabledelayedexpansion
SET TARGET=IBSFoundation

REM Support Windows 32 bits/64 bits
IF ["%ProgramFiles(x86)%"] == [""] (
set PROG_FILES_X86=%ProgramFiles%
) ELSE (
set "PROG_FILES_X86=%ProgramFiles(x86)%"
)

REM CMake path
IF EXIST "%PROG_FILES_X86%\CMake 2.8" SET CMAKE="%PROG_FILES_X86%\CMake 2.8\bin\cmake"
IF EXIST "%PROG_FILES_X86%\CMake" SET CMAKE="%PROG_FILES_X86%\CMake\bin\cmake"

REM Description.xml check (validate + recurse)
IF EXIST "%~dp0..\Description.xml" (
    CALL "%~dp0ResolveDescription.bat"
    IF ERRORLEVEL 1 EXIT /B 1
    SET "_DEPS_PY="
    WHERE python >NUL 2>&1
    IF NOT ERRORLEVEL 1 (
        SET "_DEPS_PY=python"
    ) ELSE (
        WHERE py >NUL 2>&1
        IF ERRORLEVEL 1 (
            ECHO [ERROR] Python 3 required for Description check
            EXIT /B 1
        )
        SET "_DEPS_PY=py -3"
    )
    %_DEPS_PY% "%~dp0Dependencies.py" -d "%~dp0..\Description.xml" check
    IF ERRORLEVEL 1 EXIT /B 1
)

REM Construction de l'application
IF NOT EXIST "%~dp0..\build" MKDIR "%~dp0..\build"
IF NOT EXIST "%~dp0..\dist" MKDIR "%~dp0..\dist"

IF [%BUILD_CONFIG%] == [] (
ECHO unspecified BUILD_CONFIG using [DEBUG]
SET BUILD_CONFIG=DEBUG
)

REM Construction de l'application pour msvc
IF [%BUILD_TARGET:~0,4%] == [msvc] (
SET GENERATOR_MAP=msvc6:Visual Studio 6;msvc9:Visual Studio 9 2008;msvc12:Visual Studio 12 2013;msvc12-x86_64:Visual Studio 12 2013 Win64;msvc14:Visual Studio 14 2015;msvc15:Visual Studio 15 2017
FOR /F "delims=;" %%a IN ("!GENERATOR_MAP:*%BUILD_TARGET%:=!") DO SET GENERATOR=%%a
PUSHD "%~dp0..\build"
%CMAKE% -G "!GENERATOR!" ..
IF ERRORLEVEL 1 GOTO :EOF
POPD
GOTO :EOF
)

REM Construction de l'application pour mingw64
IF [%BUILD_TARGET%] == [mingw64] (
PUSHD %~dp0..\build
%CMAKE% -G "MSYS Makefiles" ..
IF ERRORLEVEL 1 EXIT /B 1
POPD
GOTO :EOF
)

IF EXIST "%PROG_FILES_X86%\Cppcheck\cppcheck.exe" (
    "%PROG_FILES_X86%\Cppcheck\cppcheck.exe" %~dp0..\Packages ^
       -I%~dp0..\build\include ^
       --enable=all --inconclusive --xml --xml-version=2 2> %~dp0..\dist\cppcheck-result.xml
    IF ERRORLEVEL 1 GOTO :EOF
    EXIT /B 0
)

ECHO Unknown or unspecified BUILD_TARGET = [%BUILD_TARGET%]
REM set ERRORLEVEL
VERIFY OTHER 2> NUL
