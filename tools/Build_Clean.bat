@ECHO OFF
SETLOCAL

REM Remove Description-fetched trees, then build/ and dist/
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
            ECHO [ERROR] Python 3 required for Description clean
            EXIT /B 1
        )
        SET "_DEPS_PY=py -3"
    )
    %_DEPS_PY% "%~dp0Dependencies.py" -d "%~dp0..\Description.xml" clean
    IF ERRORLEVEL 1 EXIT /B 1
)

IF EXIST "%~dp0..\build" (
    ECHO Deleting "%~dp0..\build"
    RMDIR /Q /S "%~dp0..\build"
)
IF EXIST "%~dp0..\dist" (
    ECHO Deleting "%~dp0..\dist"
    RMDIR /Q /S "%~dp0..\dist"
)

EXIT /B 0
