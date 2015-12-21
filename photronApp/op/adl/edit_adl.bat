@echo ON

SETLOCAL

set PATH=%PATH%;C:\Program Files\EPICS Windows Tools

echo %EPICS_DISPLAY_PATH%

REM launch medm
start medm PhotronDevel.adl

ENDLOCAL
