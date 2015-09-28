REM start medm -x -macro "P=13PS1:, R=cam1:" Photron.adl

REM Add caRepeater to the PATH
set PATH=%PATH%;D:\epics\base-3.14.12.5\bin\windows-x64-static

..\..\bin\win32-x86-static\photronApp st.cmd
