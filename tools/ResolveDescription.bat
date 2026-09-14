@ECHO OFF
REM Resolve variant from Description.xml into current cmd environment.
REM Usage: CALL "%~dp0ResolveDescription.bat"
REM Honors existing BUILD_TARGET; fails if Description.xml is invalid or no variant matches.

IF NOT EXIST "%~dp0..\Description.xml" EXIT /B 0

SET "_DEPS_PY="
WHERE python >NUL 2>&1
IF NOT ERRORLEVEL 1 (
    SET "_DEPS_PY=python"
) ELSE (
    WHERE py >NUL 2>&1
    IF ERRORLEVEL 1 (
        ECHO [ERROR] Python 3 required to resolve Description.xml
        EXIT /B 1
    )
    SET "_DEPS_PY=py -3"
)

REM Capture resolve output then apply SET lines
SET "_RESOLVE_TMP=%TEMP%\foundation_resolve_%RANDOM%.cmd"
%_DEPS_PY% "%~dp0Dependencies.py" -d "%~dp0..\Description.xml" resolve --shell bat > "%_RESOLVE_TMP%"
IF ERRORLEVEL 1 (
    DEL /Q "%_RESOLVE_TMP%" 2>NUL
    EXIT /B 1
)
CALL "%_RESOLVE_TMP%"
DEL /Q "%_RESOLVE_TMP%" 2>NUL
ECHO [INFO] Description resolve: BUILD_TARGET=%BUILD_TARGET% VARIANT_ID=%VARIANT_ID% ENV=%VARIANT_ENV% ENV_VERSION=%ENV_VERSION%
EXIT /B 0
