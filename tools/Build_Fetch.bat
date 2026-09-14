@ECHO OFF
SETLOCAL enabledelayedexpansion

REM Resolve BUILD_TARGET from Description.xml when unset
IF EXIST "%~dp0..\Description.xml" (
    CALL "%~dp0ResolveDescription.bat"
    IF ERRORLEVEL 1 EXIT /B 1
)

REM Fetch via Description.xml (libusb https, etc.)
IF EXIST "%~dp0..\Description.xml" (
    SET "_DEPS_PY="
    WHERE python >NUL 2>&1
    IF NOT ERRORLEVEL 1 (
        SET "_DEPS_PY=python"
    ) ELSE (
        WHERE py >NUL 2>&1
        IF ERRORLEVEL 1 (
            ECHO [ERROR] Python 3 required for Description fetch
            EXIT /B 1
        )
        SET "_DEPS_PY=py -3"
    )
    %_DEPS_PY% "%~dp0Dependencies.py" -d "%~dp0..\Description.xml" fetch
    IF ERRORLEVEL 1 EXIT /B 1
)

EXIT /B 0
