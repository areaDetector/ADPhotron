TOP=../
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

INC += PDCDEV.h
INC += PDCERROR.h
INC += PDCFUNC.h
INC += PDCLIB.h
INC += PDCSTR.h
INC += PDCVALUE.h
INC += PICCLIB.h

# Note, the following files were manually copied from the Perkin Elmer distribution XISL SDK32 and SKD64 directories
ifeq (win32-x86, $(findstring win32-x86, $(T_A)))
LIB_INSTALLS_WIN32    += ../os/win32-x86/PDCLIB.lib
BIN_INSTALLS_WIN32    += ../os/win32-x86/PDCLIB.dll

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

