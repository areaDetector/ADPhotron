@echo ON

SETLOCAL

set EPICS_CA_MAX_ARRAY_BYTES=100000000

set EPICS_HOST_ARCH=windows-x64

REM dlls to the PATH
call dllPath.bat

REM caQtDM puts conflicting dlls on the PATH
set PATH=D:/epics/base-3.14.12.5/bin/windows-x64;%PATH%

REM start the IOC
..\..\bin\%EPICS_HOST_ARCH%\photronApp st.cmd

pause

ENDLOCAL
