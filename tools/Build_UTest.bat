@ECHO OFF
SETLOCAL enabledelayedexpansion

IF EXIST "%~dp0..\Description.xml" (
    CALL "%~dp0ResolveDescription.bat"
    IF ERRORLEVEL 1 EXIT /B 1
)

IF EXIST "%~dp0Build_Gen.bat" CALL "%~dp0Build_Gen.bat"
IF ERRORLEVEL 1 EXIT /B 1

IF [%BUILD_TARGET%] == [] SET BUILD_TARGET=msvc16-x86_64
IF [%BUILD_CONFIG%] == [] SET BUILD_CONFIG=Release

REM Runtime deps from regular install / toolchain (libusb etc.) — not copied next to the exe
IF [%BUILD_TARGET:~0,4%] == [msvc] (
    SET "PATH=%~dp0..\dist\%BUILD_TARGET%;%PATH%"
)
IF [%BUILD_TARGET%] == [mingw64] (
    SET "PATH=C:/msys64/ucrt64/bin;%PATH%"
)

IF EXIST "%PROGRAMFILES(X86)%\CMake" (
    SET CMAKE_PROGRAM="%PROGRAMFILES(X86)%\CMake\bin\cmake.exe"
) ELSE IF EXIST "%PROGRAMFILES%\CMake" (
    SET CMAKE_PROGRAM="%PROGRAMFILES%\CMake\bin\cmake.exe"
) ELSE (
    SET CMAKE_PROGRAM="cmake.exe"
)

PUSHD "%~dp0..\build"
IF NOT EXIST "CMakeCache.txt" (
    ECHO ERROR: build directory not configured. Run Build.bat gen first.
    POPD
    EXIT /B 1
)

IF [%BUILD_TARGET:~0,4%] == [msvc] (
    %CMAKE_PROGRAM% --build . --target foundation_utests --config %BUILD_CONFIG%
    IF ERRORLEVEL 1 GOTO :Failed
    ctest -C %BUILD_CONFIG% --output-on-failure
    IF ERRORLEVEL 1 GOTO :Failed
) ELSE (
    %CMAKE_PROGRAM% --build . --target foundation_utests --parallel %NUMBER_OF_PROCESSORS%
    IF ERRORLEVEL 1 GOTO :Failed
    ctest --output-on-failure
    IF ERRORLEVEL 1 GOTO :Failed
)

POPD
EXIT /B 0

:Failed
POPD
EXIT /B 1
