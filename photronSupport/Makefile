TOP=../
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

ifeq (windows-x64, $(findstring windows-x64, $(T_A)))

# Note, it is assumed that the SDK dir is extracted in the photronSupport dir
INC += SDK/Include/PDCDEV.h
INC += SDK/Include/PDCERROR.h
INC += SDK/Include/PDCFUNC.h
INC += SDK/Include/PDCLIB.h
INC += SDK/Include/PDCSTR.h
INC += SDK/Include/PDCVALUE.h
INC += SDK/Include/PICCLIB.h

LIB_INSTALLS_WIN32    += ../SDK/Lib/64bit(x64)/PDCLIB.lib
BIN_INSTALLS_WIN32    += ../SDK/Dll/64bit(x64)/PDCLIB.dll
BIN_INSTALLS_WIN32    += ../SDK/Dll/64bit(x64)/PICCLIB.dll
BIN_INSTALLS_WIN32    += ../SDK/Dll/64bit(x64)/IGETHER.dll
BIN_INSTALLS_WIN32    += ../SDK/Dll/64bit(x64)/GEthLib.dll
BIN_INSTALLS_WIN32    += ../SDK/Dll/64bit(x64)/DSA1.dll
BIN_INSTALLS_WIN32    += ../SDK/Dll/64bit(x64)/DSAZ.dll

endif

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

