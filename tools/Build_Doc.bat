@ECHO OFF
SETLOCAL

PUSHD "%~dp0.."

WHERE doxygen >NUL 2>&1
IF ERRORLEVEL 1 (
    ECHO ERROR: doxygen not found on PATH. Install Doxygen and retry.
    POPD
    EXIT /B 1
)

ECHO Generating documentation with Doxygen...
doxygen Doxyfile
IF ERRORLEVEL 1 (
    ECHO ERROR: Doxygen failed.
    POPD
    EXIT /B 1
)

IF EXIST "doc\html\index.html" (
    ECHO Documentation: doc\html\index.html
) ELSE (
    ECHO WARNING: doc\html\index.html not found.
)

POPD
EXIT /B 0
