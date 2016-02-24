@echo ON

SETLOCAL

REM The following paths will need to be customized when the IOC is deployed
set EPICS_DISPLAY_PATH=D:\epics\AD-2-4\areaDetector-R2-4\ADPhotron\photronApp\op\adl
set EPICS_DISPLAY_PATH=%EPICS_DISPLAY_PATH%;D:\epics\AD-2-4\areaDetector-R2-4\ADCore-R2-4\ADApp\op\adl
set EPICS_DISPLAY_PATH=%EPICS_DISPLAY_PATH%;D:\epics\synApps\support\asyn-4-27\opi\medm

set PATH=%PATH%;C:\Program Files\EPICS Windows Tools

echo %EPICS_DISPLAY_PATH%

REM Launch medm
start medm -x -macro "P=pho:, R=cam1:" Photron.adl

ENDLOCAL
