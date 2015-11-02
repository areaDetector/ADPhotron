@echo ON
REM start medm -x -macro "P=13PS1:, R=cam1:" Photron.adl

SETLOCAL

set EPICS_CA_MAX_ARRAY_BYTES=100000000

set EPICS_HOST_ARCH=windows-x64-debug

REM dlls to the PATH
call dllPath.bat

REM start the IOC
..\..\bin\%EPICS_HOST_ARCH%\photronApp st.cmd

pause

ENDLOCAL
