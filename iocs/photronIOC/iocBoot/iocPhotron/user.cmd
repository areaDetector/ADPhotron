### Stuff for user programming ###
dbLoadRecords("$(CALC)/calcApp/Db/userCalcs10.db","P=$(PREFIX)")
dbLoadRecords("$(CALC)/calcApp/Db/userCalcOuts10.db","P=$(PREFIX)")
dbLoadRecords("$(CALC)/calcApp/Db/userStringCalcs10.db","P=$(PREFIX)")
dbLoadRecords("$(CALC)/calcApp/Db/userStringSeqs10.db","P=$(PREFIX)")
dbLoadRecords("$(CALC)/calcApp/Db/userArrayCalcs10.db","P=$(PREFIX),N=4000")
dbLoadRecords("$(CALC)/calcApp/Db/userTransforms10.db","P=$(PREFIX)")
dbLoadRecords("$(CALC)/calcApp/Db/userAve10.db","P=$(PREFIX)")
#
dbLoadRecords("$(BUSY)/busyApp/Db/busyRecord.db", "P=$(PREFIX),R=busy1")
dbLoadRecords("$(BUSY)/busyApp/Db/busyRecord.db", "P=$(PREFIX),R=busy2")
#
dbLoadRecords("$(TOP)/photronApp/Db/gp.db","P=$(PREFIX)")
