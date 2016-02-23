@echo ON

SETLOCAL

set EPICS_DISPLAY_PATH=D:\epics\AD-2-4\areaDetector-R2-4\ADPhotron\photronApp\op\adl
set EPICS_DISPLAY_PATH=%EPICS_DISPLAY_PATH%;D:\epics\AD-2-4\areaDetector-R2-4\ADCore-R2-4\ADApp\op\adl
set EPICS_DISPLAY_PATH=%EPICS_DISPLAY_PATH%;D:\epics\synApps\support\asyn-4-27\opi\medm

set PATH=%PATH%;C:\Program Files\EPICS Windows Tools

echo %EPICS_DISPLAY_PATH%

REM launch medm
start medm -x -macro "P=kmp5:, R=cam1:" Photron.adl

ENDLOCAL
