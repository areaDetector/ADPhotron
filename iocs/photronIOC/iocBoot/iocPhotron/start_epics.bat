REM start medm -x -macro "P=13PS1:, R=cam1:" Photron.adl

SETLOCAL

REM dlls to the PATH
call dllPath.bat

REM start the IOC
..\..\bin\%EPICS_HOST_ARCH%\photronApp st.cmd

ENDLOCAL
