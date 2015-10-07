TOP=../
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

INC += SDK/Include/PDCDEV.h
INC += SDK/Include/PDCERROR.h
INC += SDK/Include/PDCFUNC.h
INC += SDK/Include/PDCLIB.h
INC += SDK/Include/PDCSTR.h
INC += SDK/Include/PDCVALUE.h
INC += SDK/Include/PICCLIB.h

# Note, it is assumed that the SDK dir is extracted in the photronSupport dir
ifeq (win32-x86, $(findstring win32-x86, $(T_A)))
LIB_INSTALLS_WIN32    += ../SDK/Lib/32bit(x86)/PDCLIB.lib
BIN_INSTALLS_WIN32    += ../SDK/Dll/32bit(x86)/PDCLIB.dll
BIN_INSTALLS_WIN32    += ../SDK/Dll/32bit(x86)/PICCLIB.dll
BIN_INSTALLS_WIN32    += ../SDK/Dll/32bit(x86)/IGETHER.dll
BIN_INSTALLS_WIN32    += ../SDK/Dll/32bit(x86)/GEthLib.dll
BIN_INSTALLS_WIN32    += ../SDK/Dll/32bit(x86)/DSA1.dll
BIN_INSTALLS_WIN32    += ../SDK/Dll/32bit(x86)/DSAZ.dll
#BIN_INSTALLS_WIN32    += C:/Windows/System32/vcomp120.dll

#!else ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
#!LIB_INSTALLS_WIN32    += ../os/windows-x64/XISL.lib
#!BIN_INSTALLS_WIN32    += ../os/windows-x64/XISL.dll
#!BIN_INSTALLS_WIN32    += ../os/windows-x64/gbif64.dll
#!BIN_INSTALLS_WIN32    += ../os/windows-x64/libxml2_x64.dll

endif

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

