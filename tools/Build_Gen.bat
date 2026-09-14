@ECHO OFF
SETLOCAL enabledelayedexpansion
SET TARGET=foundation

REM Build libfoundation + utests (app lives in sibling Sample)
IF NOT EXIST "%~dp0..\build" MKDIR "%~dp0..\build"
IF NOT EXIST "%~dp0..\dist" MKDIR "%~dp0..\dist"

IF [%BUILD_CONFIG%] == [] (
    SET BUILD_CONFIG=Release
    ECHO unspecified BUILD_CONFIG using [!BUILD_CONFIG!]
)

IF EXIST "%~dp0..\Description.xml" (
    CALL "%~dp0ResolveDescription.bat"
    IF ERRORLEVEL 1 EXIT /B 1
) ELSE IF [%BUILD_TARGET%] == [] (
    SET BUILD_TARGET=msvc16-x86_64
    ECHO unspecified BUILD_TARGET using [!BUILD_TARGET!]
)

IF [%BUILD_TARGET%] == [] (
    ECHO [ERROR] BUILD_TARGET not set after Description resolve
    EXIT /B 1
)

IF EXIST "%PROGRAMFILES(X86)%\CMake" (
    SET CMAKE_PROGRAM="%PROGRAMFILES(X86)%\CMake\bin\cmake.exe"
) ELSE IF EXIST "%PROGRAMFILES%\CMake" (
    SET CMAKE_PROGRAM="%PROGRAMFILES%\CMake\bin\cmake.exe"
) ELSE (
    SET CMAKE_PROGRAM="cmake.exe"
)
ECHO CMAKE_PROGRAM=%CMAKE_PROGRAM%

IF [%BUILD_TARGET%] == [mingw64] (
    SET "PATH=C:/msys64/ucrt64/bin;%PATH%"
    SET TOOLCHAIN_FILE="%~dp0toolchain-windows-mingw64-gcc.cmake"
    ECHO TOOLCHAIN_FILE=!TOOLCHAIN_FILE!
)

IF [%BUILD_TARGET:~0,4%] == [msvc] (

    SET GENERATOR_MAP=msvc9-x86:"Visual Studio 9 2008" -A Win32;msvc10-x86:"Visual Studio 10 2010" -A Win32;msvc16-x86:"Visual Studio 16 2019" -A Win32;msvc16-x86_64:"Visual Studio 16 2019" -A x64
    FOR /F "delims=;" %%a IN ("!GENERATOR_MAP:*%BUILD_TARGET%:=!") DO SET GENERATOR=%%a

    PUSHD "%~dp0..\build"
    SET "_NEED_CMAKE=0"
    IF NOT EXIST "CMakeCache.txt" SET "_NEED_CMAKE=1"
    IF EXIST "%~dp0..\lib\mcp2221a_hid\deps\libusb-1.0.30\include\libusb.h" (
        IF NOT EXIST "%~dp0..\lib\mcp2221a_hid\deps\libusb-1.0.30\include\libusb-1.0\libusb.h" (
            SET "_NEED_CMAKE=1"
            ECHO [INFO] libusb extracted but nested include missing — reconfigure CMake
        )
    )
    REM Always reconfigure when cache exists so CMakeLists changes (slim) are picked up
    IF [!_NEED_CMAKE!]==[1] (
        IF NOT EXIST "CMakeCache.txt" (
            ECHO %CMAKE_PROGRAM% -G !GENERATOR! ..
            %CMAKE_PROGRAM% -G !GENERATOR! ..
        ) ELSE (
            ECHO %CMAKE_PROGRAM% ..
            %CMAKE_PROGRAM% ..
        )
    ) ELSE (
        ECHO %CMAKE_PROGRAM% ..
        %CMAKE_PROGRAM% ..
    )
    IF ERRORLEVEL 1 (
        POPD
        EXIT /B 1
    )

    %CMAKE_PROGRAM% --build . --target %TARGET% --config %BUILD_CONFIG%
    IF ERRORLEVEL 1 (
        POPD
        EXIT /B 1
    )

    %CMAKE_PROGRAM% --build . --target foundation_utests --config %BUILD_CONFIG%
    IF ERRORLEVEL 1 (
        POPD
        EXIT /B 1
    )

    %CMAKE_PROGRAM% --build . --target INSTALL --config %BUILD_CONFIG%
    IF ERRORLEVEL 1 (
        POPD
        EXIT /B 1
    )

    POPD
    EXIT /B 0
)

IF [%BUILD_TARGET%] == [mingw64] (
    PUSHD %~dp0..\build

    IF NOT EXIST "CMakeCache.txt" (
        ECHO %CMAKE_PROGRAM% -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE=!TOOLCHAIN_FILE! -DCMAKE_BUILD_TYPE=!BUILD_CONFIG! ..
        %CMAKE_PROGRAM% -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE=!TOOLCHAIN_FILE! -DCMAKE_BUILD_TYPE=!BUILD_CONFIG! ..
    ) ELSE (
        ECHO %CMAKE_PROGRAM% -DCMAKE_BUILD_TYPE=!BUILD_CONFIG! ..
        %CMAKE_PROGRAM% -DCMAKE_BUILD_TYPE=!BUILD_CONFIG! ..
    )
    IF ERRORLEVEL 1 (
        POPD
        EXIT /B 1
    )
    %CMAKE_PROGRAM% --build . --target %TARGET% --parallel %NUMBER_OF_PROCESSORS%
    IF ERRORLEVEL 1 (
        POPD
        EXIT /B 1
    )

    %CMAKE_PROGRAM% --build . --target foundation_utests --parallel %NUMBER_OF_PROCESSORS%
    IF ERRORLEVEL 1 (
        POPD
        EXIT /B 1
    )

    %CMAKE_PROGRAM% --install .
    IF ERRORLEVEL 1 (
        POPD
        EXIT /B 1
    )

    POPD
    EXIT /B 0
)

ECHO Unknown or unspecified BUILD_TARGET = [%BUILD_TARGET%]
EXIT /B 1
