/* Photron.cpp
 *
 * This is a driver for Photron Detectors
 *
 * Author: Kevin Peterson
 *         Argonne National Laboratory
 *
 * Created:  September 25, 2015
 *
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <epicsMutex.h>
#include <cantProceed.h>
#include <osiSock.h>
#include <iocsh.h>

#include "ADDriver.h"
#include <epicsExport.h>
#include "Photron.h"

#include <windows.h>
#include "SDK/Include/PDCLIB.h"

static const char *driverName = "Photron";

//static int PDCLibInitialized;

//static ELLLIST *cameraList;


/** Report status of the driver.
  * Prints details about the driver if details>0.
  * It then calls the ADDriver::report() method.
  * \param[in] fp File pointed passed by caller where the output is written to.
  * \param[in] details If >0 then driver details are printed.
  */
void Photron::report(FILE *fp, int details)
{

    fprintf(fp, "Photron detector %s\n", this->portName);
    if (details > 0) {
		// put useful info here
    }
    /* Invoke the base class method */
    ADDriver::report(fp, details);
}

/* From asynPortDriver: Connects driver to device; */
/*asynStatus Photron::connect(asynUser* pasynUser) {

    return connectCamera();
}*/


/* From asynPortDriver: Disconnects driver from device; */
/*asynStatus Photron::disconnect(asynUser* pasynUser) {

    return disconnectCamera();
}*/

/** Constructor for Photron; most parameters are simply passed to ADDriver::ADDriver.
  * After calling the base class constructor this method creates a thread to compute the simulated detector data,
  * and sets reasonable default values for parameters defined in this class, asynNDArrayDriver and ADDriver.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] ipAddress The IP address of the camera or starting IP address for auto-detection
  * \param[in] autoDetect Enable auto-detection of camera. Set this to 0 to specify the IP address manually
  * \param[in] maxBuffers The maximum number of NDArray buffers that the NDArrayPool for this driver is
  *            allowed to allocate. Set this to -1 to allow an unlimited number of buffers.
  * \param[in] maxMemory The maximum amount of memory that the NDArrayPool for this driver is
  *            allowed to allocate. Set this to -1 to allow an unlimited amount of memory.
  * \param[in] priority The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  * \param[in] stackSize The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  */
Photron::Photron(const char *portName, const char *ipAddress, int autoDetect,
                         int maxBuffers, size_t maxMemory, int priority, int stackSize)

    : ADDriver(portName, 1, NUM_PHOTRON_PARAMS, maxBuffers, maxMemory,
               0, 0, /* No interfaces beyond those set in ADDriver.cpp */
               0, 1, /* ASYN_CANBLOCK=0, ASYN_MULTIDEVICE=0, autoConnect=1 */
               priority, stackSize),
      pRaw(NULL)

{
    int status = asynSuccess;
    const char *functionName = "Photron";

	struct in_addr ipAddr;
	unsigned long ipNum;
	//char * defaultIP = "192.168.0.10";

	unsigned long nRet;
	unsigned long nErrorCode;
	unsigned long nDeviceNo;				/* Device number */
	PDC_DETECT_NUM_INFO DetectNumInfo; 		/* Search result */
	unsigned long IPList[PDC_MAX_DEVICE]; 	/* IP ADDRESS being searched */

	this->cameraId = epicsStrDup(ipAddress);

	IPList[0] = 0xC0A8000A;
	//IPList[0] = 0xC0A80000;
	
    /* We have been given an IP address or IP name */
    status = hostToIPAddr(this->cameraId, &ipAddr);
    if (status) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s:%s: Cannot find IP address %s\n", 
              driverName, functionName, this->cameraId);
        //return asynError;
    }
    ipNum = (unsigned long) ipAddr.s_addr;
	/* The Photron SDK needs the ip address in host byte order */
	ipNum = ntohl(ipNum);

	if (ipNum == IPList[0])
		printf("IP address is 192.168.0.10\n");
	else
	{
		printf("IP address is NOT 192.168.0.10\n");
		printf("\tipNum = %x\n", ipNum);
		printf("\nIPList[0] = %x\n", IPList[0]);
	}

	nRet = PDC_Init(&nErrorCode);
	if (nRet == PDC_FAILED)
	{
		printf("PDC_Init Error %d\n", nErrorCode);
	}
	else
	{
		printf("PDC_Init Successful\n");
	
		nRet = PDC_DetectDevice(PDC_INTTYPE_G_ETHER,	/* Gigabit ethernet interface */
								IPList,					/* IP address */
								1,						/* Max number of searched devices */
								autoDetect,		/* 0=PDC_DETECT_NORMAL ; 1=PDC_DETECT_AUTO */
								&DetectNumInfo,
								&nErrorCode);


		if (nRet == PDC_FAILED)
		{
			printf("PDC_DetectDevice Error %d\n", nErrorCode);
		}
		else
		{
			printf("PDC_DetectDevice \"Successful\"\n");
			
			if (DetectNumInfo.m_nDeviceNum == 0)
			{
				printf("No devices detected\n");
			}
			/* only do this if not auto-searching for devices */
			else if (DetectNumInfo.m_DetectInfo[0].m_nTmpDeviceNo != IPList[0])
			{
				printf("The search result IP address is different\n");
				printf("\tm_nTmpDeviceNo = %x\n", DetectNumInfo.m_DetectInfo[0].m_nTmpDeviceNo);
				printf("\tIPList[0] = %x\n", IPList[0]);
			}
			else
			{
				printf("\tdevice index: %d\n", DetectNumInfo.m_nDeviceNum);
				printf("\tdevice code: %d\n", DetectNumInfo.m_DetectInfo[0].m_nDeviceCode);
				printf("nRet = %d\n", nRet);
			
				//nRet = PDC_OpenDevice(&(DetectNumInfo.m_DetectInfo[0]), /* Subject device information */
				//						&nDeviceNo,
				//						&nErrorCode);
				nRet = PDC_OpenDevice2(&(DetectNumInfo.m_DetectInfo[0]), /* Subject device information */
										10,	/* nMaxRetryCount */
										0,  /* nConnectMode -- 1=normal, 0=safe */
										&nDeviceNo,
										&nErrorCode);
			
				if (nRet == PDC_FAILED)
				{
					printf("PDC_OpenDeviceError %d\n", nErrorCode);
				}
				else
				{
					printf("Device #%i opened successfully\n", nDeviceNo);
				}
			
			}
		}

	}
	
    /* Create the epicsEvents for signaling to the simulate task when acquisition starts and stops */
    this->startEventId = epicsEventCreate(epicsEventEmpty);
    if (!this->startEventId) {
        printf("%s:%s epicsEventCreate failure for start event\n",
            driverName, functionName);
        return;
    }
    this->stopEventId = epicsEventCreate(epicsEventEmpty);
    if (!this->stopEventId) {
        printf("%s:%s epicsEventCreate failure for stop event\n",
            driverName, functionName);
        return;
    }

	// CREATE PARAMS HERE
    // createParam(SimGainXString,       asynParamFloat64, &SimGainX);

    /* Set some default values for parameters */
    //status =  setStringParam (ADManufacturer, "Simulated detector");
    //status |= setStringParam (ADModel, "Basic simulator");

    //if (status) {
    //    printf("%s: unable to set camera parameters\n", functionName);
    //    return;
    //}

    /* Create the thread that updates the images */
    //status = (epicsThreadCreate("SimDetTask",
    //                            epicsThreadPriorityMedium,
    //                            epicsThreadGetStackSize(epicsThreadStackMedium),
    //                            (EPICSTHREADFUNC)simTaskC,
    //                            this) == NULL);
    //if (status) {
    //    printf("%s:%s epicsThreadCreate failure for image task\n",
    //        driverName, functionName);
    //    return;
    //}
}

/** Configuration command, called directly or from iocsh */
extern "C" int PhotronConfig(const char *portName, const char *ipAddress, int autoDetect,
                                 int maxBuffers, int maxMemory, int priority, int stackSize)
{
    new Photron(portName, ipAddress, autoDetect,
                    (maxBuffers < 0) ? 0 : maxBuffers,
                    (maxMemory < 0) ? 0 : maxMemory, 
                    priority, stackSize);
    return(asynSuccess);
}

/** Code for iocsh registration */
static const iocshArg PhotronConfigArg0 = {"Port name", iocshArgString};
static const iocshArg PhotronConfigArg1 = {"IP address", iocshArgString};
static const iocshArg PhotronConfigArg2 = {"Auto-detect", iocshArgInt};
static const iocshArg PhotronConfigArg3 = {"maxBuffers", iocshArgInt};
static const iocshArg PhotronConfigArg4 = {"maxMemory", iocshArgInt};
static const iocshArg PhotronConfigArg5 = {"priority", iocshArgInt};
static const iocshArg PhotronConfigArg6 = {"stackSize", iocshArgInt};
static const iocshArg * const PhotronConfigArgs[] =  {&PhotronConfigArg0,
                                                          &PhotronConfigArg1,
                                                          &PhotronConfigArg2,
                                                          &PhotronConfigArg3,
                                                          &PhotronConfigArg4,
                                                          &PhotronConfigArg5,
                                                          &PhotronConfigArg6};
static const iocshFuncDef configPhotron = {"PhotronConfig", 7, PhotronConfigArgs};
static void configPhotronCallFunc(const iocshArgBuf *args)
{
    PhotronConfig(args[0].sval, args[1].sval, args[2].ival, args[3].ival,
                      args[4].ival, args[5].ival, args[6].ival);
}


static void PhotronRegister(void)
{

    iocshRegister(&configPhotron, configPhotronCallFunc);
}

extern "C" {
epicsExportRegistrar(PhotronRegister);
}
