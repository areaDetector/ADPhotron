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
#include <iocsh.h>

#include "ADDriver.h"
#include <epicsExport.h>
#include "Photron.h"

static const char *driverName = "Photron";


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

/** Constructor for Photron; most parameters are simply passed to ADDriver::ADDriver.
  * After calling the base class constructor this method creates a thread to compute the simulated detector data,
  * and sets reasonable default values for parameters defined in this class, asynNDArrayDriver and ADDriver.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] maxSizeX The maximum X dimension of the images that this driver can create.
  * \param[in] maxSizeY The maximum Y dimension of the images that this driver can create.
  * \param[in] dataType The initial data type (NDDataType_t) of the images that this driver will create.
  * \param[in] maxBuffers The maximum number of NDArray buffers that the NDArrayPool for this driver is
  *            allowed to allocate. Set this to -1 to allow an unlimited number of buffers.
  * \param[in] maxMemory The maximum amount of memory that the NDArrayPool for this driver is
  *            allowed to allocate. Set this to -1 to allow an unlimited amount of memory.
  * \param[in] priority The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  * \param[in] stackSize The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  */
Photron::Photron(const char *portName, int maxSizeX, int maxSizeY, NDDataType_t dataType,
                         int maxBuffers, size_t maxMemory, int priority, int stackSize)

    : ADDriver(portName, 1, NUM_PHOTRON_PARAMS, maxBuffers, maxMemory,
               0, 0, /* No interfaces beyond those set in ADDriver.cpp */
               0, 1, /* ASYN_CANBLOCK=0, ASYN_MULTIDEVICE=0, autoConnect=1 */
               priority, stackSize),
      pRaw(NULL)

{
    int status = asynSuccess;
    const char *functionName = "Photron";

	printf("Kevin was here\n");
	
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
extern "C" int PhotronConfig(const char *portName, int maxSizeX, int maxSizeY, int dataType,
                                 int maxBuffers, int maxMemory, int priority, int stackSize)
{
    new Photron(portName, maxSizeX, maxSizeY, (NDDataType_t)dataType,
                    (maxBuffers < 0) ? 0 : maxBuffers,
                    (maxMemory < 0) ? 0 : maxMemory, 
                    priority, stackSize);
    return(asynSuccess);
}

/** Code for iocsh registration */
static const iocshArg PhotronConfigArg0 = {"Port name", iocshArgString};
static const iocshArg PhotronConfigArg1 = {"Max X size", iocshArgInt};
static const iocshArg PhotronConfigArg2 = {"Max Y size", iocshArgInt};
static const iocshArg PhotronConfigArg3 = {"Data type", iocshArgInt};
static const iocshArg PhotronConfigArg4 = {"maxBuffers", iocshArgInt};
static const iocshArg PhotronConfigArg5 = {"maxMemory", iocshArgInt};
static const iocshArg PhotronConfigArg6 = {"priority", iocshArgInt};
static const iocshArg PhotronConfigArg7 = {"stackSize", iocshArgInt};
static const iocshArg * const PhotronConfigArgs[] =  {&PhotronConfigArg0,
                                                          &PhotronConfigArg1,
                                                          &PhotronConfigArg2,
                                                          &PhotronConfigArg3,
                                                          &PhotronConfigArg4,
                                                          &PhotronConfigArg5,
                                                          &PhotronConfigArg6,
                                                          &PhotronConfigArg7};
static const iocshFuncDef configPhotron = {"PhotronConfig", 8, PhotronConfigArgs};
static void configPhotronCallFunc(const iocshArgBuf *args)
{
    PhotronConfig(args[0].sval, args[1].ival, args[2].ival, args[3].ival,
                      args[4].ival, args[5].ival, args[6].ival, args[7].ival);
}


static void PhotronRegister(void)
{

    iocshRegister(&configPhotron, configPhotronCallFunc);
}

extern "C" {
epicsExportRegistrar(PhotronRegister);
}
