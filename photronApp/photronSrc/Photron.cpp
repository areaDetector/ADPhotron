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
#include <epicsExit.h>

#include "ADDriver.h"
#include <epicsExport.h>
#include "Photron.h"

#include <windows.h>

static const char *driverName = "Photron";

static int PDCLibInitialized=0;

static ELLLIST *cameraList;


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
               0, 0, /* ASYN_CANBLOCK=0, ASYN_MULTIDEVICE=0, autoConnect=1 */
               priority, stackSize),
      pRaw(NULL) {
  int status = asynSuccess;
  int pdcStatus = PDC_SUCCEEDED; // PDC_SUCCEEDED=1, PDC_FAILED=0
  const char *functionName = "Photron";
  unsigned long errCode;
  cameraNode *pNode = new cameraNode;
 
  this->cameraId = epicsStrDup(ipAddress);
  this->autoDetect = autoDetect;

  // If this is the first camera we need to initialize the camera list
  if (!cameraList) {
    cameraList = new ELLLIST;
    ellInit(cameraList);
  }
  pNode->pCamera = this;
  ellAdd(cameraList, (ELLNODE *)pNode);

  // CREATE PARAMS HERE
  createParam(PhotronStatusString,           asynParamInt32, &PStatus);
  createParam(Photron8BitSelectString,       asynParamInt32, &P8BitSel);
  createParam(PhotronRecordRateString,       asynParamInt32, &PRecRate);
  
  if (!PDCLibInitialized) {
    /* Initialize the Photron PDC library */
    pdcStatus = PDC_Init(&errCode);
    if (pdcStatus == PDC_FAILED) {
      printf("%s:%s: PDC_Init Error %d\n", driverName, functionName, errCode);
      return;
    }
    PDCLibInitialized = 1;
  }

  /* Create the epicsEvents for signaling to the acquisition task when 
     acquisition starts and stops */
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
  
  /* Register the shutdown function for epicsAtExit */
  epicsAtExit(shutdown, (void*)this);

  /* Create the thread that updates the images */
  status = (epicsThreadCreate("PhotronTask", epicsThreadPriorityMedium,
                epicsThreadGetStackSize(epicsThreadStackMedium),
                (EPICSTHREADFUNC)PhotronTaskC, this) == NULL);
  if (status) {
    printf("%s:%s epicsThreadCreate failure for image task\n",
           driverName, functionName);
    return;
  }

  /* Try to connect to the camera.  
   * It is not a fatal error if we cannot now, the camera may be off or owned by
   * someone else. It may connect later. */
  this->lock();
  status = connectCamera();
  this->unlock();
  if (status) {
    printf("%s:%s: cannot connect to camera %s, manually connect later\n", 
           driverName, functionName, cameraId);
    return;
  }
}


Photron::~Photron() {
  cameraNode *pNode = (cameraNode *)ellFirst(cameraList);
  static const char *functionName = "~Photron";

  this->lock();
  printf("Disconnecting camera %s\n", this->portName);
  disconnectCamera();
  this->unlock();

  // Find this camera in the list:
  while (pNode) {
    if (pNode->pCamera == this)
      break;
    pNode = (cameraNode *)ellNext(&pNode->node);
  }
  if (pNode) {
    ellDelete(cameraList, (ELLNODE *)pNode);
    delete pNode;
  }

  /* If this is the last camera in the IOC then unregister callbacks and 
     uninitialize */
  if (ellCount(cameraList) == 0) {
    delete cameraList;
  }
}


void Photron::shutdown (void* arg) {
  Photron *p = (Photron*)arg;
  if (p) delete p;
}


static void PhotronTaskC(void *drvPvt) {
  Photron *pPvt = (Photron *)drvPvt;
  pPvt->PhotronTask();
}


/** This thread calls readImage to retrieve new image data from the camera and 
  * does the callbacks to send it to higher layers. It implements the logic for 
  * single, multiple or continuous acquisition. */
void Photron::PhotronTask() {
  asynStatus imageStatus;
  int imageCounter;
  int numImages, numImagesCounter;
  int imageMode;
  int arrayCallbacks;
  int acquire;
  NDArray *pImage;
  double acquirePeriod, delay;
  epicsTimeStamp startTime, endTime;
  double elapsedTime;
  const char *functionName = "PhotronTask";

  this->lock();
  /* Loop forever */
  while (1) {
    /* Is acquisition active? */
    getIntegerParam(ADAcquire, &acquire);
    //printf("is acquisition active?\n");

    /* If we are not acquiring then wait for a semaphore that is given when 
       acquisition is started */
    if (!acquire) {
      setIntegerParam(ADStatus, ADStatusIdle);
      callParamCallbacks();
      /* Release the lock while we wait for an event that says acquire has 
         started, then lock again */
      asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                "%s:%s: waiting for acquire to start\n", driverName, 
                functionName);
      this->unlock();
      epicsEventWait(this->startEventId);
      this->lock();
      setIntegerParam(ADNumImagesCounter, 0);
    }

    /* We are acquiring. */
    /* Get the current time */
    epicsTimeGetCurrent(&startTime);

    /* Get the exposure parameters */
    getDoubleParam(ADAcquirePeriod, &acquirePeriod);

    setIntegerParam(ADStatus, ADStatusAcquire);

    /* Open the shutter */
    setShutter(ADShutterOpen);

    /* Call the callbacks to update any changes */
    callParamCallbacks();

    //printf("I should do something\n");
    
    /* Read the image */
    imageStatus = readImage();

    /* Close the shutter */
    setShutter(ADShutterClosed);
    /* Call the callbacks to update any changes */
    callParamCallbacks();

    if (imageStatus == asynSuccess) {
      pImage = this->pArrays[0];

      /* Get the current parameters */
      getIntegerParam(NDArrayCounter, &imageCounter);
      getIntegerParam(ADNumImages, &numImages);
      getIntegerParam(ADNumImagesCounter, &numImagesCounter);
      getIntegerParam(ADImageMode, &imageMode);
      getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
      imageCounter++;
      numImagesCounter++;
      setIntegerParam(NDArrayCounter, imageCounter);
      setIntegerParam(ADNumImagesCounter, numImagesCounter);

      /* Put the frame number and time stamp into the buffer */
      pImage->uniqueId = imageCounter;
      pImage->timeStamp = startTime.secPastEpoch + startTime.nsec / 1.e9;
      updateTimeStamp(&pImage->epicsTS);

      /* Get any attributes that have been defined for this driver */
      this->getAttributes(pImage->pAttributeList);

      if (arrayCallbacks) {
        /* Call the NDArray callback */
        /* Must release the lock here, or we can get into a deadlock, because we
         * can block on the plugin lock, and the plugin can be calling us */
        this->unlock();
        asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                  "%s:%s: calling imageData callback\n", driverName,
                  functionName);
        doCallbacksGenericPointer(pImage, NDArrayData, 0);
        this->lock();
      }
    }

    /* See if acquisition is done */
    if ((imageStatus != asynSuccess) || (imageMode == ADImageSingle) ||
        ((imageMode == ADImageMultiple) && (numImagesCounter >= numImages))) {
      setIntegerParam(ADAcquire, 0);
      asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                "%s:%s: acquisition completed\n", driverName, functionName);
    }

    /* Call the callbacks to update any changes */
    callParamCallbacks();
    getIntegerParam(ADAcquire, &acquire);

    /* If acquiring then sleep for the acquire period minus elapsed time. */
    if (acquire) {
      epicsTimeGetCurrent(&endTime);
      elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);
      delay = acquirePeriod - elapsedTime;
      asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                "%s:%s: delay=%f\n", driverName, functionName, delay);
      if (delay >= 0.0) {
        /* Set the status to readOut to indicate we are in the period delay */
        setIntegerParam(ADStatus, ADStatusWaiting);
        callParamCallbacks();
        this->unlock();
        epicsEventWaitWithTimeout(this->stopEventId, delay);
        this->lock();
      }
    }
  }
}


/* From asynPortDriver: Disconnects driver from device; */
asynStatus Photron::disconnect(asynUser* pasynUser) {
  return disconnectCamera();
}


asynStatus Photron::disconnectCamera() {
  int status = asynSuccess;
  static const char *functionName = "disconnectCamera";
  unsigned long nRet;
  unsigned long nErrorCode;

  /* Ensure that PDC library has been initialised */
  if (!PDCLibInitialized) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
        "%s:%s: Connecting to camera %ld while PDC library is uninitialized.\n", 
        driverName, functionName, this->uniqueId);
    return asynError;
  }
  
  nRet = PDC_CloseDevice(this->nDeviceNo, &nErrorCode);
  if (nRet == PDC_FAILED){
    printf("PDC_CloseDevice for device #%d did not succeed. Error code = %d\n", 
           this->nDeviceNo, nErrorCode);
  } else {
    printf("PDC_CloseDevice succeeded for device #%d\n", this->nDeviceNo);
  }
  
  /* Camera is disconnected. Signal to asynManager that it is disconnected. */
  status = pasynManager->exceptionDisconnect(this->pasynUserSelf);
  if (status) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
      "%s:%s: error calling pasynManager->exceptionDisconnect, error=%s\n",
      driverName, functionName, pasynUserSelf->errorMessage);
  }
  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
    "%s:%s: Camera disconnected; unique id: %ld\n", 
    driverName, functionName, this->uniqueId);

  return((asynStatus)status);
}


/* From asynPortDriver: Connects driver to device; */
asynStatus Photron::connect(asynUser* pasynUser) {
  return connectCamera();
}


asynStatus Photron::connectCamera() {
  int status = asynSuccess;
  static const char *functionName = "connectCamera";
  //
  struct in_addr ipAddr;
  unsigned long ipNumWire;
  unsigned long ipNumHost;
  //
  unsigned long nRet;
  unsigned long nErrorCode;
  PDC_DETECT_NUM_INFO DetectNumInfo;     /* Search result */
  unsigned long IPList[PDC_MAX_DEVICE];   /* IP ADDRESS being searched */
  //
  int index;
  char nFlag; /* Existing function flag */
  
  /* default IP address is "192.168.0.10" */
  //IPList[0] = 0xC0A8000A;
  /* default IP for auto-detection is "192.168.0.0" */
  //IPList[0] = 0xC0A80000;
  
  /* Ensure that PDC library has been initialised */
  if (!PDCLibInitialized) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
      "%s:%s: Connecting to camera %ld while PDC library is uninitialized.\n", 
      driverName, functionName, this->uniqueId);
    return asynError;
  }
  
  // The Prosilica driver does this, but it isn't obvious why
  /* First disconnect from the camera */
  //disconnectCamera();

  /* We have been given an IP address or IP name */
  status = hostToIPAddr(this->cameraId, &ipAddr);
  if (status) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
      "%s:%s: Cannot find IP address %s\n", 
      driverName, functionName, this->cameraId);
    //return asynError;
  }
  ipNumWire = (unsigned long) ipAddr.s_addr;
  /* The Photron SDK needs the ip address in host byte order */
  ipNumHost = ntohl(ipNumWire);

  IPList[0] = ipNumHost;
  
  // Attempt to detect the type of detector at the specified ip addr
  nRet = PDC_DetectDevice(
              PDC_INTTYPE_G_ETHER, /* Gigabit ethernet interface */
              IPList,              /* IP address */
              1,                   /* Max number of searched devices */
              this->autoDetect,    /* 0=PDC_DETECT_NORMAL;1=PDC_DETECT_AUTO */
              &DetectNumInfo,
              &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_DetectDevice Error %d\n", nErrorCode);
    return asynError;
  }

  printf("PDC_DetectDevice \"Successful\"\n");

  printf("\tdevice index: %d\n", DetectNumInfo.m_nDeviceNum);
  printf("\tdevice code: %d\n", DetectNumInfo.m_DetectInfo[0].m_nDeviceCode);
  printf("\tnRet = %d\n", nRet);

  if (DetectNumInfo.m_nDeviceNum == 0) {
    printf("No devices detected\n");
    return asynError;
  }

  /* only do this if not auto-searching for devices */
  if ((this->autoDetect == PDC_DETECT_NORMAL) && 
     (DetectNumInfo.m_DetectInfo[0].m_nTmpDeviceNo != IPList[0])) {
    printf("The specified and detected IP addresses differ:\n");
    printf("\tIPList[0] = %x\n", IPList[0]);
    printf("\tm_nTmpDeviceNo = %x\n", 
           DetectNumInfo.m_DetectInfo[0].m_nTmpDeviceNo);
    return asynError;
  }

  nRet = PDC_OpenDevice(&(DetectNumInfo.m_DetectInfo[0]), &(this->nDeviceNo),
                        &nErrorCode);
  /* When should PDC_OpenDevice2 be used instead of PDC_OpenDevice? */
  //nRet = PDC_OpenDevice2(&(DetectNumInfo.m_DetectInfo[0]), 
  //            10,  /* nMaxRetryCount */
  //            0,  /* nConnectMode -- 1=normal, 0=safe */
  //            &(this->nDeviceNo),
  //            &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_OpenDeviceError %d\n", nErrorCode);
    return asynError;
  } else {
    printf("Device #%i opened successfully\n", this->nDeviceNo);
  }

  /* Assume only one child, for now */
  this->nChildNo = 1;
  
  /* Determine which functions are supported by the camera */
  for( index=2; index<98; index++) {
    nRet = PDC_IsFunction(this->nDeviceNo, this->nChildNo, index, &nFlag, 
                          &nErrorCode);
    if (nRet == PDC_FAILED) {
      if (nErrorCode == PDC_ERROR_NOT_SUPPORTED) {
        this->functionList[index] = PDC_EXIST_NOTSUPPORTED;
      } else {
        printf("PDC_IsFunction failed for function %d, error = %d\n", 
               index, nErrorCode);
        return asynError;
      }
    } else {
      this->functionList[index] = nFlag;
    }
  }
  printf("function queries succeeded\n");
  
  /* query the controller for info */
  
  nRet = PDC_GetDeviceCode(this->nDeviceNo, &(this->deviceCode), &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetDeviceCode failed %d\n", nErrorCode);
    return asynError;
  }  
  
  nRet = PDC_GetDeviceName(this->nDeviceNo, 0, this->deviceName, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetDeviceName failed %d\n", nErrorCode);
    return asynError;
  }
  
  nRet = PDC_GetVersion(this->nDeviceNo, 0, &(this->version), &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetVersion failed %d\n", nErrorCode);
    return asynError;
  }  

  nRet = PDC_GetMaxChildDeviceCount(this->nDeviceNo, &(this->maxChildDevCount), 
                                    &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetMaxChildDeviceCount failed %d\n", nErrorCode);
    return asynError;
  }  

  nRet = PDC_GetChildDeviceCount(this->nDeviceNo, &(this->childDevCount), 
                                 &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetChildDeviceCount failed %d\n", nErrorCode);
    return asynError;
  }  
  
  nRet = PDC_GetMaxResolution(this->nDeviceNo, this->nChildNo, 
                              &(this->sensorWidth), &(this->sensorHeight), 
                              &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetMaxResolution failed %d\n", nErrorCode);
    return asynError;
  }  

  if (this->functionList[PDC_EXIST_BITDEPTH] == PDC_EXIST_SUPPORTED) {
    nRet = PDC_GetMaxBitDepth(this->nDeviceNo, this->nChildNo, this->sensorBits,
                              &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_GetMaxBitDepth failed %d\n", nErrorCode);
      return asynError;
    } else {
      printf("PDC_GetMaxBitDepth succeeded. maxBitDepth = %s\n", 
             this->sensorBits);
    }
    
    nRet = PDC_GetBitDepth(this->nDeviceNo, this->nChildNo, this->bitDepth,
                           &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_GetBitDepth failed %d\n", nErrorCode);
      return asynError;
    } else {
      printf("PDC_GetBitDepth succeeded. bitDepth = %s\n", this->bitDepth);
    }
  } else {
      this->sensorBits = "N/A";
  }
  
  /* PDC_GetStatus is also called in readParameters(), but it is called here
     so that the camera can be put into live mode */
  nRet = PDC_GetStatus(this->nDeviceNo, &(this->nStatus), &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetStatus failed %d\n", nErrorCode);
    return asynError;
  } else {
    if (this->nStatus == PDC_STATUS_PLAYBACK) {
      nRet = PDC_SetStatus(this->nDeviceNo, PDC_STATUS_LIVE, &nErrorCode);
      if (nRet == PDC_FAILED) {
        printf("PDC_SetStatus failed. error = %d\n", nErrorCode);
      }
    }
  }

  /*
  PDC_GetTriggerMode succeeded
        Mode = 0
        AFrames = 5457
        RFrames = 0
        RCount = 0
  */
  
  nRet = PDC_GetTriggerMode(this->nDeviceNo, &(this->triggerMode),
                            &(this->trigAFrames), &(this->trigRFrames),
                            &(this->trigRCount), &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetTriggerMode failed %d\n", nErrorCode);
    return asynError;
  } else {
    printf("PDC_GetTriggerMode succeeded\n");
    printf("\tMode = %d\n", this->triggerMode);
    printf("\tAFrames = %d\n", this->trigAFrames);
    printf("\tRFrames = %d\n", this->trigRFrames);
    printf("\tRCount = %d\n", this->trigRCount);
  }

  nRet = PDC_GetRecordRateList(this->nDeviceNo, this->nChildNo, 
                               &(this->RateListSize), this->RateList, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetRecordRateList failed %d\n", nErrorCode);
    return asynError;
  } else {
    printf("PDC_GetRecordRateList succeeded. Num =%d\n", this->RateListSize);
    for (index=0; index<this->RateListSize; index++) {
      printf("\t%d:\t%d FPS\n", (index + 1), this->RateList[index]);
    }
  }
  
  /* Set some initial values for other parameters */
  status =  setStringParam (ADManufacturer, "Photron");
  status |= setStringParam (ADModel, this->deviceName);
  status |= setIntegerParam(ADSizeX, this->sensorWidth);
  status |= setIntegerParam(ADSizeY, this->sensorHeight);
  status |= setIntegerParam(ADMaxSizeX, this->sensorWidth);
  status |= setIntegerParam(ADMaxSizeY, this->sensorHeight);
  
  if (status) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s:%s: unable to set camera parameters on camera %lu\n",
              driverName, functionName, this->uniqueId);
    return asynError;
  }
  
  /* Read the current camera settings */
  status = readParameters();
  if (status) 
    return((asynStatus)status);
  
  /* We found the camera. Everything is OK. Signal to asynManager that we are 
     connected. */
  status = pasynManager->exceptionConnect(this->pasynUserSelf);
  if (status) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
      "%s:%s: error calling pasynManager->exceptionConnect, error=%s\n",
      driverName, functionName, pasynUserSelf->errorMessage);
    return asynError;
  }
  asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
    "%s:%s: Camera connected; unique id: %ld\n", driverName,
    functionName, this->uniqueId);
  return asynSuccess;
}


asynStatus Photron::readImage() {
  int status = asynSuccess;
  int index, jndex;
  //
  int sizeX, sizeY;
  size_t dims[2];
  double gain;
  //
  NDArray *pImage;
  NDArrayInfo_t arrayInfo;
  int colorMode = NDColorModeMono;
  //
  unsigned long nRet;
  unsigned long nErrorCode;
  int numBytes;
  epicsUInt8 *pBuf;  /* Memory sequence pointer for storing a live image */
  //
  static const char *functionName = "readImage";

  getIntegerParam(ADSizeX,  &sizeX);
  getIntegerParam(ADSizeX,  &sizeY);
  getDoubleParam (ADGain,   &gain);
  
  //-------
  // DEBUG print the BitDepth
  //-------
  
  //-------
  // DEBUG print the status - will I need to check the status before acquiring in the future?
  /*nRet = PDC_GetStatus(this->nDeviceNo, &(this->nStatus), &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetStatus failed %d\n", nErrorCode);
    return asynError;
  } else {
    printf("PDC_GetStatus succeeded. status = %d\n", this->nStatus);
  }*/
  //-------
  
  numBytes = this->sensorWidth * this->sensorHeight * sizeof(epicsUInt8);
  pBuf = (epicsUInt8*) malloc(numBytes);
  
  nRet = PDC_GetLiveImageData(this->nDeviceNo, this->nChildNo,
                              8, /* 8 bits */
                              pBuf, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetLiveImageData Failed. Error %d\n", nErrorCode);
    free(pBuf);
    return asynError;
  }

  printf("\n");
  for (index=0; index<10; index++) {
    for (jndex=0; jndex<10; jndex++) {
      printf("%d ", pBuf[(this->sensorWidth * index) + jndex]);
    }
    printf("\n");
  }

  /* We save the most recent image buffer so it can be used in the read() 
   * function. Now release it before getting a new version. */
  if (this->pArrays[0]) 
    this->pArrays[0]->release();
  
  /* Allocate the raw buffer we use to compute images. */
  dims[0] = sizeX;
  dims[1] = sizeY;
  pImage = this->pNDArrayPool->alloc(2, dims, NDUInt8, 0, NULL);
  if (!pImage) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: error allocating buffer\n", driverName, functionName);
    return(asynError);
  }
  
  memcpy(pImage->pData, pBuf, numBytes);
  
  this->pArrays[0] = pImage;
  pImage->pAttributeList->add("ColorMode", "Color mode", NDAttrInt32, 
                              &colorMode);
  pImage->getInfo(&arrayInfo);
  setIntegerParam(NDArraySize,  (int)arrayInfo.totalBytes);
  setIntegerParam(NDArraySizeX, (int)pImage->dims[0].size);
  setIntegerParam(NDArraySizeY, (int)pImage->dims[1].size);

  free(pBuf);
  
  return asynSuccess;
}


/** Called when asyn clients call pasynInt32->write().
  * This function performs actions for some parameters, including ADAcquire, ADBinX, etc.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus Photron::writeInt32(asynUser *pasynUser, epicsInt32 value) {
  int function = pasynUser->reason;
  int status = asynSuccess;
  int adstatus;
  static const char *functionName = "writeInt32";

  /* Set the parameter and readback in the parameter library.  This may be 
   * overwritten when we read back the status at the end, but that's OK */
  status |= setIntegerParam(function, value);

  if ((function == ADBinX) || (function == ADBinY) || (function == ADMinX) ||
     (function == ADSizeX) || (function == ADMinY) || (function == ADSizeY)) {
    /* These commands change the chip readout geometry.  We need to cache them 
     * and apply them in the correct order */
    //printf("calling setGeometry. function=%d, value=%d\n", function, value);
    status |= setGeometry();
  } else if (function == ADAcquire) {
    getIntegerParam(ADStatus, &adstatus);
    if (value && (adstatus == ADStatusIdle)) {
      /* Send an event to wake up the acquisition task.
       * It won't actually start generating new images until we release the lock
       * below */
      epicsEventSignal(this->startEventId);
    }
    if (!value && (adstatus != ADStatusIdle)) {
      /* This was a command to stop acquisition */
      /* Send the stop event */
      epicsEventSignal(this->stopEventId);
    }
  } else if (function == P8BitSel) {
    /* Specifies the bit position during 8-bit transfer from a device of more 
       than 8 bits. */
    setTransferOption();
  } else if (function == PRecRate) {
    /* Specifies the bit position during 8-bit transfer from a device of more 
       than 8 bits. */
    setRecordRate();
  } else {
    /* If this is not a parameter we have handled call the base class */
    status = ADDriver::writeInt32(pasynUser, value);
  }
  
  /* Read the camera parameters and do callbacks */
  status |= readParameters();
  
  if (status) 
    asynPrint(pasynUser, ASYN_TRACE_ERROR, 
              "%s:%s: error, status=%d function=%d, value=%d\n", 
              driverName, functionName, status, function, value);
  else        
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, value=%d\n",
              driverName, functionName, function, value);
  return((asynStatus)status);
}


asynStatus Photron::getGeometry() {
  int status = asynSuccess;
  int binX, binY, minY, minX, sizeX, sizeY, maxSizeX, maxSizeY;
  static const char *functionName = "getGeometry";

  // Eventually this will read from the detector; for now, hardcode values
  binX = binY = 1;
  minX = minY = 0;
  sizeX = sizeY = maxSizeX = maxSizeY = 1024;
  
  status |= setIntegerParam(ADBinX,  binX);
  status |= setIntegerParam(ADBinY,  binY);
  status |= setIntegerParam(ADMinX,  minX*binX);
  status |= setIntegerParam(ADMinY,  minY*binY);
  status |= setIntegerParam(ADSizeX, sizeX*binX);
  status |= setIntegerParam(ADSizeY, sizeY*binY);
  status |= setIntegerParam(NDArraySizeX, sizeX);
  status |= setIntegerParam(NDArraySizeY, sizeY);

  if (status)
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s:%s: error, status=%d\n", driverName, functionName, status);

  return((asynStatus)status);
}

asynStatus Photron::setGeometry() {
  int status = asynSuccess;
  int binX, binY, minY, minX, sizeX, sizeY, maxSizeX, maxSizeY;
  static const char *functionName = "setGeometry";
  
  /* Get all of the current geometry parameters from the parameter library */
  status = getIntegerParam(ADBinX, &binX);
  //printf("\tBinX status = %d\n", status);
  if (binX < 1)
    binX = 1;
  status = getIntegerParam(ADBinY, &binY);
  //printf("\tBinY status = %d\n", status);
  if (binY < 1)
    binY = 1;
  status = getIntegerParam(ADMinX, &minX);
  //printf("\tMinX status = %d\n", status);
  status = getIntegerParam(ADMinY, &minY);
  //printf("\tMinY status = %d\n", status);
  status = getIntegerParam(ADSizeX, &sizeX);
  //printf("\tSizeX status = %d\n", status);
  status = getIntegerParam(ADSizeY, &sizeY);
  //printf("\tSizeY status = %d\n", status);
  status = getIntegerParam(ADMaxSizeX, &maxSizeX);
  //printf("\tMaxSizeX status = %d\n", status);
  status = getIntegerParam(ADMaxSizeY, &maxSizeY);
  //printf("\tMAxSizeY status = %d\n", status);

  if (minX + sizeX > maxSizeX) {
    sizeX = maxSizeX - minX;
    setIntegerParam(ADSizeX, sizeX);
  }
  if (minY + sizeY > maxSizeY) {
    sizeY = maxSizeY - minY;
    setIntegerParam(ADSizeY, sizeY);
  }
  
  if (status)
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s:%s: error, status=%d\n", driverName, functionName, status);

  return((asynStatus)status);
}


asynStatus Photron::setTransferOption() {
  unsigned long nRet;
  unsigned long nErrorCode;
  int status = asynSuccess;
  int n8BitSel;
  
  static const char *functionName = "setTransferOption";
  
  status = getIntegerParam(P8BitSel, &n8BitSel);
  
  // TODO: confirm that we are in 8-bit acquisition mode, 
  //       otherwise this isn't necessary
  nRet = PDC_SetTransferOption(this->nDeviceNo, this->nChildNo, n8BitSel,
                               PDC_FUNCTION_OFF, PDC_FUNCTION_OFF, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetMaxResolution failed %d\n", nErrorCode);
    return asynError;
  }  
  
  return asynSuccess;
}


asynStatus Photron::setRecordRate() {
  unsigned long nRet;
  unsigned long nErrorCode;
  int status = asynSuccess;
  int recRate;
  
  static const char *functionName = "setRecordRate";
   
  status = getIntegerParam(PRecRate, &recRate);
  
  //TODO: enforce valid record rates
  nRet = PDC_SetRecordRate(this->nDeviceNo, this->nChildNo, recRate, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_SetRecordRate Error %d\n", nErrorCode);
    return asynError;
  } else {
    printf("PDC_SetRecordRate succeeded\n");
  }
  
  return asynSuccess;
}


asynStatus Photron::readParameters() {
  unsigned long nRet;
  unsigned long nErrorCode;
  int status = asynSuccess;
  static const char *functionName = "readParameters";    
  
  status |= getGeometry();
  
  
  //##############################################################################
  
  
  nRet = PDC_GetStatus(this->nDeviceNo, &(this->nStatus), &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetStatus failed %d\n", nErrorCode);
    return asynError;
  }
  
  nRet = PDC_GetRecordRate(this->nDeviceNo, this->nChildNo, &(this->nRate), &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetRecordRate failed %d\n", nErrorCode);
    return asynError;
  }
  
  
  
  




  //
  status |= setIntegerParam(PRecRate, this->nRate);
  status |= setIntegerParam(PRecRate, this->nRate);
  
  /* Call the callbacks to update the values in higher layers */
  callParamCallbacks();

  if (status)
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: error, status=%d\n", driverName, functionName, status);
  return((asynStatus)status);
}


/** Report status of the driver.
  * Prints details about the driver if details>0.
  * It then calls the ADDriver::report() method.
  * \param[in] fp File pointed passed by caller where the output is written to.
  * \param[in] details If >0 then driver details are printed.
  */
void Photron::report(FILE *fp, int details) {
  int index;

  fprintf(fp, "Photron detector %s\n", this->portName);
  if (details > 0) {
    // put useful info here
    fprintf(fp, "  Camera Id:         %s\n",  this->cameraId);
    fprintf(fp, "  Auto-detect:       %d\n",  (int)this->autoDetect);
    fprintf(fp, "  Device code:       %d\n",  (int)this->deviceCode);
    fprintf(fp, "  Device name:       %s\n",  this->deviceName);
    fprintf(fp, "  Version:           %0.2f\n",  (float)(this->version/100.0));
    fprintf(fp, "  Sensor bits:       %s\n",  this->sensorBits);
    fprintf(fp, "  Sensor width:      %d\n",  (int)this->sensorWidth);
    fprintf(fp, "  Sensor height:     %d\n",  (int)this->sensorHeight);
    fprintf(fp, "  Max Child Dev #:   %d\n",  (int)this->maxChildDevCount);
    fprintf(fp, "  Child Dev #:       %d\n",  (int)this->childDevCount);
    fprintf(fp, "  Camera Status:     %d\n",  (int)this->nStatus);

    /*
    fprintf(fp, "  ID:                %lu\n", pInfo->UniqueId);
    fprintf(fp, "  IP address:        %s\n",  this->IPAddress);
    fprintf(fp, "  Serial number:     %s\n",  pInfo->SerialNumber);
    fprintf(fp, "  Camera name:       %s\n",  pInfo->CameraName);
    fprintf(fp, "  Model:             %s\n",  pInfo->ModelName);
    fprintf(fp, "  Firmware version:  %s\n",  pInfo->FirmwareVersion);
    fprintf(fp, "  Access flags:      %lx\n", pInfo->PermittedAccess);
    fprintf(fp, "  Sensor type:       %s\n",  this->sensorType);
    fprintf(fp, "  Time stamp freq:   %d\n",  (int)this->timeStampFrequency);
    fprintf(fp, "  maxPvAPIFrames:    %d\n",  (int)this->maxPvAPIFrames_);
    */

    }
  
  if (details > 4) {
    fprintf(fp, "  Available functions:\n");
    for( index=2; index<98; index++) {
      fprintf(fp, "    %d:         %d\n", index, this->functionList[index]);
    }
  }
  
  if (details > 8) {
    /* Invoke the base class method */
    ADDriver::report(fp, details);
  }
}


/** Configuration command, called directly or from iocsh */
extern "C" int PhotronConfig(const char *portName, const char *ipAddress,
                             int autoDetect, int maxBuffers, int maxMemory,
                             int priority, int stackSize) {
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
static const iocshFuncDef configPhotron = {"PhotronConfig", 7, 
                                           PhotronConfigArgs};
static void configPhotronCallFunc(const iocshArgBuf *args) {
    PhotronConfig(args[0].sval, args[1].sval, args[2].ival, args[3].ival,
                  args[4].ival, args[5].ival, args[6].ival);
}

static void PhotronRegister(void) {
    iocshRegister(&configPhotron, configPhotronCallFunc);
}

extern "C" {
epicsExportRegistrar(PhotronRegister);
}
