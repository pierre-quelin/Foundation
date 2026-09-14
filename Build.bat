@ECHO OFF
SETLOCAL

SET _FETCH=0
SET _GEN=0
SET _CLEAN=0
SET _CHECK=0
SET _DOC=0
SET _UTEST=0
SET _PAUSE=0

REM ===================== Select Service =======================================
IF /I NOT [%1]==[] GOTO :NOT_INTERACTIVE
SET _PAUSE=1
ECHO.--------------------
ECHO.-^> Select service ^<-
ECHO.--------------------
ECHO.[ ] - all     (fetch + gen)
ECHO.[1] - fetch   (retrieve all dependencies)
ECHO.[2] - gen     (build the target)
ECHO.[3] - check   (run cppcheck)
ECHO.[4] - doc     (generate Doxygen documentation)
ECHO.[5] - utest   (build and run unit tests)
ECHO.[9] - clean
ECHO.
ECHO.[0] - Quit
ECHO.
SET /P _USRINPUT=
IF /I [%_USRINPUT%]==[0] (
   ECHO 0 -^> Bye
   GOTO :End
) ELSE (
IF /I [%_USRINPUT%]==[1] (
   SET _FETCH=1
) ELSE (
IF /I [%_USRINPUT%]==[2] (
   SET _GEN=1
) ELSE (
IF /I [%_USRINPUT%]==[3] (
   SET _CHECK=1
) ELSE (
IF /I [%_USRINPUT%]==[4] (
   SET _DOC=1
) ELSE (
IF /I [%_USRINPUT%]==[5] (
   SET _UTEST=1
) ELSE (
IF /I [%_USRINPUT%]==[9] (
   SET _CLEAN=1
) ELSE (
IF /I [%_USRINPUT%]==[] (
   SET _FETCH=1
   SET _GEN=1
) ELSE (
ECHO ERROR : %_USRINPUT% -^> Bad choice -^> Bye
))))))))
:NOT_INTERACTIVE

FOR %%a IN (%*) DO (
IF /I [%%a]==[all] (
SET _FETCH=1
SET _GEN=1
)
IF /I [%%a]==[clean] SET _CLEAN=1
IF /I [%%a]==[fetch] SET _FETCH=1
IF /I [%%a]==[gen] SET _GEN=1
IF /I [%%a]==[check] SET _CHECK=1
IF /I [%%a]==[doc] SET _DOC=1
IF /I [%%a]==[utest] SET _UTEST=1
)

REM ===================== Clean ================================================
IF [%_CLEAN%]==[1] IF EXIST tools/Build_Clean.bat CALL tools/Build_Clean.bat
IF [%_CLEAN%]==[1] IF ERRORLEVEL 1 GOTO :End

REM ===================== Fetch ================================================
IF [%_FETCH%]==[1] IF EXIST tools/Build_Fetch.bat CALL tools/Build_Fetch.bat
IF [%_FETCH%]==[1] IF ERRORLEVEL 1 GOTO :End

REM ===================== Gen ==================================================
IF [%_GEN%]==[1] IF EXIST tools/Build_Gen.bat CALL tools/Build_Gen.bat
IF [%_GEN%]==[1] IF ERRORLEVEL 1 GOTO :End

REM ===================== Check ==================================================
IF [%_CHECK%]==[1] IF EXIST tools/Build_Check.bat CALL tools/Build_Check.bat
IF [%_CHECK%]==[1] IF ERRORLEVEL 1 GOTO :End

REM ===================== Doc ====================================================
IF [%_DOC%]==[1] IF EXIST tools/Build_Doc.bat CALL tools/Build_Doc.bat
IF [%_DOC%]==[1] IF ERRORLEVEL 1 GOTO :End

REM ===================== UTest ==================================================
IF [%_UTEST%]==[1] IF EXIST tools/Build_UTest.bat CALL tools/Build_UTest.bat
IF [%_UTEST%]==[1] IF ERRORLEVEL 1 GOTO :End

:End
if [%_PAUSE%]==[1] PAUSE

