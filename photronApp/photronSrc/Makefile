TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#======== DETECTOR LIBRARY ==============

# The following gets rid of the -fno-implicit-templates flag on vxWorks, 
# so we get automatic template instantiation.
# This is what we want for miscellaneous/asynPortDriver.cpp
ifeq (vxWorks,$(findstring vxWorks, $(T_A)))
CODE_CXXFLAGS=
endif

ifeq (windows-x64, $(findstring windows-x64, $(T_A)))

INC += Photron.h

LIBRARY_IOC = Photron
LIB_SRCS += Photron.cpp
LIB_LIBS += PDCLIB

Photron_SYS_LIBS_WIN32 += ws2_32

DBD += PhotronSupport.dbd

endif

include $(ADCORE)/ADApp/commonLibraryMakefile

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

