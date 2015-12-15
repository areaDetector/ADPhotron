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
  // Initialize the bitDepth for asynReport in case the feature isn't supported
  this->bitDepth = 0;

  // If this is the first camera we need to initialize the camera list
  if (!cameraList) {
    cameraList = new ELLLIST;
    ellInit(cameraList);
  }
  pNode->pCamera = this;
  ellAdd(cameraList, (ELLNODE *)pNode);

  // CREATE PARAMS HERE
  createParam(PhotronStatusString,        asynParamInt32, &PhotronStatus);
  createParam(PhotronAcquireModeString,   asynParamInt32, &PhotronAcquireMode);
  createParam(PhotronMaxFramesString,     asynParamInt32, &PhotronMaxFrames);
  createParam(Photron8BitSelectString,    asynParamInt32, &Photron8BitSel);
  createParam(PhotronRecordRateString,    asynParamInt32, &PhotronRecRate);
  createParam(PhotronAfterFramesString,   asynParamInt32, &PhotronAfterFrames);
  createParam(PhotronRandomFramesString,  asynParamInt32, &PhotronRandomFrames);
  createParam(PhotronRecCountString,      asynParamInt32, &PhotronRecCount);
  createParam(PhotronSoftTrigString,      asynParamInt32, &PhotronSoftTrig);
  createParam(PhotronRecReadyString,      asynParamInt32, &PhotronRecReady);
  createParam(PhotronLiveString,          asynParamInt32, &PhotronLive);
  createParam(PhotronPlaybackString,      asynParamInt32, &PhotronPlayback);
  createParam(PhotronEndlessString,       asynParamInt32, &PhotronEndless);
  createParam(PhotronReadMemString,       asynParamInt32, &PhotronReadMem);

  
  if (!PDCLibInitialized) {
    /* Initialize the Photron PDC library */
    pdcStatus = PDC_Init(&errCode);
    if (pdcStatus == PDC_FAILED) {
      asynPrint(
          this->pasynUserSelf, ASYN_TRACE_ERROR, 
          "%s:%s: PDC_Init Error %d\n", driverName, functionName, errCode);
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
  
  /* Create the epicsEvents for signaling to the recording task when 
     to start watching the camera status */
  this->startRecEventId = epicsEventCreate(epicsEventEmpty);
  if (!this->startRecEventId) {
    printf("%s:%s epicsEventCreate failure for start rec event\n",
           driverName, functionName);
    return;
  }
  this->stopRecEventId = epicsEventCreate(epicsEventEmpty);
  if (!this->stopRecEventId) {
    printf("%s:%s epicsEventCreate failure for stop rec event\n",
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
  
  /* Create the thread that retrieves triggered recordings */
  status = (epicsThreadCreate("PhotronRecTask", epicsThreadPriorityMedium,
                epicsThreadGetStackSize(epicsThreadStackMedium),
                (EPICSTHREADFUNC)PhotronRecTaskC, this) == NULL);
  if (status) {
    printf("%s:%s epicsThreadCreate failure for record task\n",
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

  // Attempt to stop the recording thread
  epicsEventSignal(this->stopRecEventId);
  
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


static void PhotronRecTaskC(void *drvPvt) {
  Photron *pPvt = (Photron *)drvPvt;
  pPvt->PhotronRecTask();
}


/** This thread puts the camera in playback mode and reads recorded image data
  * from the camera after recording is done.
  */
void Photron::PhotronRecTask() {
  /*asynStatus imageStatus;
  int imageCounter;
  int numImages, numImagesCounter;
  int imageMode;
  int arrayCallbacks;
  NDArray *pImage;
  double acquirePeriod, delay;
  epicsTimeStamp startTime, endTime;
  double elapsedTime;*/
  
  unsigned long status;
  unsigned long nRet;
  unsigned long nErrorCode;
  int acqMode;
  //int recFlag;
  //PDC_FRAME_INFO FrameInfo;
  
  const char *functionName = "PhotronRecTask";

  
  this->lock();
  /* Loop forever */
  while (1) {
    /* Are we in record mode? */
    getIntegerParam(PhotronAcquireMode, &acqMode);
    //printf("is acquisition active?\n");

    /* If we are not in record mode then wait for a semaphore that is given when 
       record mode is requested */
    if (acqMode != 1) {
      /* Release the lock while we wait for an event that says acquire has 
         started, then lock again */
      asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                "%s:%s: waiting for acquire to start\n", driverName, 
                functionName);
      this->unlock();
      epicsEventWait(this->startRecEventId);
      this->lock();
      
    }
    
    // We're in record mode
    
    // Set Rec Ready
    /*nRet = PDC_SetRecReady(nDeviceNo, &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_SetRecready Error %d\n", nErrorCode);
    }
    
    // Reset the recording flag
    recFlag = 0;*/
    
    // Wait for triggered recording
    while (1) {
      // Get camera status
      nRet = PDC_GetStatus(this->nDeviceNo, &status, &nErrorCode);
      if (nRet == PDC_FAILED) {
        printf("PDC_GetStatus failed %d\n", nErrorCode);
      }
      setIntegerParam(PhotronStatus, status);
      callParamCallbacks();
      
      /*
      
      if (status == PDC_STATUS_REC) {
        // A trigger was received; camera is recording
        recFlag = 1;
      }
      
      if ((status == PDC_STATUS_RECREADY) && (recFlag == 1)) {
        // A recording is done; readback the images and clear the rec flag
        
        // Put the camera in playback mode
        nRet = PDC_SetStatus(this->nDeviceNo, PDC_STATUS_PLAYBACK, &nErrorCode);
        if (nRet == PDC_FAILED) {
          printf("PDC_SetStatus failed. error = %d\n", nErrorCode);
        }
        
        // Retrieves frame information 
        nRet = PDC_GetMemFrameInfo(nDeviceNo, this->nChildNo, &FrameInfo,
                                   &nErrorCode);
        if (nRet == PDC_FAILED) {
          printf("PDC_GetMemFrameInfo Error %d\n", nErrorCode);
        }
        
        // display frame info
        printf("Frame Info:\n");
        printf("\tFrame Start:\t%d\n", FrameInfo.m_nStart);
        printf("\tFrame End:\t%d\n", FrameInfo.m_nEnd);
        printf("\tFrame Trigger:\t%d\n", FrameInfo.m_nTrigger);
        printf("\tFrame Start:\t%d\n", FrameInfo.m_nStart);
        printf("\tEvent count:\t%d\n", FrameInfo.m_nEventCount);
        printf("\tRecorded Frames:\t%d\n", FrameInfo.m_nRecordedFrames);
        
        // PDC_GetMemResolution
        // PDC_GetMemRecordRate
        // PDC_GetMemTriggerMode
        
        // Retrieves a trigger frame 
        nRet = PDC_GetMemImageData(nDeviceNo, nChildNo, FrameInfo.m_nTrigger,
                                   8, pBuf, &nErrorCode);
        if (nRet == PDC_FAILED) {
          printf("PDC_GetMemImageData Error %d\n", nErrorCode);
          free(pBuf); 
        }
        
        // Put the camera in recready mode
        nRet = PDC_SetStatus(this->nDeviceNo, PDC_STATUS_LIVE, &nErrorCode);
        if (nRet == PDC_FAILED) {
          printf("PDC_SetStatus failed. error = %d\n", nErrorCode);
        }
        
        nRet = PDC_SetRecReady(nDeviceNo, &nErrorCode);
        if (nRet == PDC_FAILED) {
          printf("PDC_SetRecready Error %d\n", nErrorCode);
        }
        
        // Reset the rec flag
        recFlag = 0;
        
      }
      
      */
      
      // release the lock so the trigger PV can be used
      this->unlock();
      epicsThreadSleep(0.001);
      this->lock();
      
      
      
      
    }
  }
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
    //setShutter(ADShutterOpen);

    /* Call the callbacks to update any changes */
    callParamCallbacks();

    //printf("I should do something\n");
    
    /* Read the image */
    imageStatus = readImage();

    /* Close the shutter */
    //setShutter(ADShutterClosed);
    
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
        "%s:%s: Connecting to camera %s while PDC library is uninitialized.\n", 
        driverName, functionName, this->cameraId);
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
    "%s:%s: Camera disconnected; camera id: %s\n", 
    driverName, functionName, this->cameraId);

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
  
  /* default IP address is "192.168.0.10" */
  //IPList[0] = 0xC0A8000A;
  /* default IP for auto-detection is "192.168.0.0" */
  //IPList[0] = 0xC0A80000;
  
  /* Ensure that PDC library has been initialised */
  if (!PDCLibInitialized) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
      "%s:%s: Connecting to camera %s while PDC library is uninitialized.\n", 
      driverName, functionName, this->cameraId);
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
  
  /* PDC_GetStatus is also called in readParameters(), but it is called here
     so that the camera can be put into live mode--will remove this after
     making the mode a PV */
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
  /* Get information from the camera */
  status = getCameraInfo();
  if (status) {
    return((asynStatus)status);
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
              "%s:%s: unable to set camera parameters on camera %s\n",
              driverName, functionName, this->cameraId);
    return asynError;
  }
  
  /* Read the current camera settings */
  status = readParameters();
  if (status) {
    return((asynStatus)status);
  }
  
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
    "%s:%s: Camera connected; camera id: %ld\n", driverName,
    functionName, this->cameraId);
  return asynSuccess;
}


asynStatus Photron::getCameraInfo() {
  unsigned long nRet;
  unsigned long nErrorCode;
  int status = asynSuccess;
  char sensorBitChar;
  static const char *functionName = "getCameraInfo";
  //
  int index;
  char nFlag; /* Existing function flag */

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
  //printf("function queries succeeded\n");
  
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
  
  nRet = PDC_GetDeviceID(this->nDeviceNo, &(this->deviceID), &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetDeviceID failed %d\n", nErrorCode);
    return asynError;
  }
  
  nRet = PDC_GetLotID(this->nDeviceNo, 0, &(this->lotID), &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetLotID failed %d\n", nErrorCode);
    return asynError;
  }
  
  nRet = PDC_GetProductID(this->nDeviceNo, 0, &(this->productID), &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetProductID failed %d\n", nErrorCode);
    return asynError;
  }
  
  nRet = PDC_GetIndividualID(this->nDeviceNo, 0, &(this->individualID), &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetIndividualID failed %d\n", nErrorCode);
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
  
  /* This gets the dynamic range of the camera. The third argument is an 
     unsigned long in the SDK documentation but a char * in PDCFUNC.h.
     It appears that only a single char is returned. */
  nRet = PDC_GetMaxBitDepth(this->nDeviceNo, this->nChildNo, &sensorBitChar,
                            &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetMaxBitDepth failed %d\n", nErrorCode);
    return asynError;
  } else {
    this->sensorBits = (unsigned long) sensorBitChar;
  }
  
  // Is this always the same or should it be moved to readParameters?
  nRet = PDC_GetRecordRateList(this->nDeviceNo, this->nChildNo, 
                               &(this->RateListSize), this->RateList, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetRecordRateList failed %d\n", nErrorCode);
    return asynError;
  } 
  
  // This needs to be called once before readParameters is called, otherwise
  // updateResolution will crash the IOC
  nRet = PDC_GetResolutionList(this->nDeviceNo, this->nChildNo, 
                               &(this->ResolutionListSize),
                               this->ResolutionList, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetResolutionList failed %d\n", nErrorCode);
    return asynError;
  }
  
  return asynSuccess;
}

asynStatus Photron::readImage() {
  int status = asynSuccess;
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
  void *pBuf;  /* Memory sequence pointer for storing a live image */
  //
  NDDataType_t dataType;
  int pixelSize;
  size_t dataSize;
  static const char *functionName = "readImage";

  getIntegerParam(ADSizeX,  &sizeX);
  getIntegerParam(ADSizeY,  &sizeY);
  getDoubleParam (ADGain,   &gain);
  
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
  
  if (this->pixelBits == 8) {
    // 8 bits
    dataType = NDUInt8;
    pixelSize = 1;
  } else {
    // 12 bits (stored in 2 bytes)
    dataType = NDUInt16;
    pixelSize = 2;
  }
  
  //printf("sizeof(epicsUInt8) = %d\n", sizeof(epicsUInt8));
  //printf("sizeof(epicsUInt16) = %d\n", sizeof(epicsUInt16));
  
  dataSize = sizeX * sizeY * pixelSize;
  pBuf = malloc(dataSize);
  
  nRet = PDC_GetLiveImageData(this->nDeviceNo, this->nChildNo,
                              this->pixelBits,
                              pBuf, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetLiveImageData Failed. Error %d\n", nErrorCode);
    free(pBuf);
    return asynError;
  }

  /* We save the most recent image buffer so it can be used in the read() 
   * function. Now release it before getting a new version. */
  if (this->pArrays[0]) 
    this->pArrays[0]->release();
  
  /* Allocate the raw buffer */
  dims[0] = sizeX;
  dims[1] = sizeY;
  pImage = this->pNDArrayPool->alloc(2, dims, dataType, 0, NULL);
  if (!pImage) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: error allocating buffer\n", driverName, functionName);
    return(asynError);
  }
  
  memcpy(pImage->pData, pBuf, dataSize);
  
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
  int adstatus, acqMode;
  static const char *functionName = "writeInt32";

  /* Set the parameter and readback in the parameter library.  This may be 
   * overwritten when we read back the status at the end, but that's OK */
  status |= setIntegerParam(function, value);

  if ((function == ADBinX) || (function == ADBinY) || (function == ADMinX) ||
     (function == ADMinY)) {
    /* These commands change the chip readout geometry.  We need to cache them 
     * and apply them in the correct order */
    //printf("calling setGeometry. function=%d, value=%d\n", function, value);
    status |= setGeometry();
  } else if (function == ADSizeX) {
    status |= setValidWidth(value);
  } else if (function == ADSizeY) {
    status |= setValidHeight(value);
  } else if (function == ADAcquire) {
    getIntegerParam(PhotronAcquireMode, &acqMode);
    if (acqMode == 0) {
      // For Live mode, signal the PhotronTask
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
    } else {
      // For Rec mode, ignore and reset the acquire button
      setIntegerParam(ADAcquire, 0);
    }
  } else if (function == NDDataType) {
    status = setPixelFormat();
  } else if (function == PhotronAcquireMode) {
    // should the acquire state be checked?
    if (value == 0) {
      printf("Settings for returning to live mode should go here\n");
      
      // code to return to live mode goes here
      setLive();
      
      // Stop the PhotronRecTask (will it do one last read after returning to live?
      epicsEventSignal(this->stopRecEventId);
    } else {
      printf("Settings for entering recording mode should go here\n");
      
      // apply the trigger settings?
      
      // code to enter recording mode
      setRecReady();
      
      // Wake up the PhotronRecTask
      epicsEventSignal(this->startRecEventId);
    }
  } else if (function == Photron8BitSel) {
    /* Specifies the bit position during 8-bit transfer from a device of more 
       than 8 bits. */
    setTransferOption();
  } else if (function == PhotronRecRate) {
    setRecordRate(value);
  } else if (function == PhotronStatus) {
    setStatus(value);
  } else if (function == PhotronSoftTrig) {
    printf("Soft Trigger changed. value = %d\n", value);
    softwareTrigger();
    
  } else if (function == PhotronRecReady) {
    setRecReady();
    
  } else if (function == PhotronEndless) {
    setEndless();
    
  } else if (function == PhotronLive) {
    setLive();
    
  } else if (function == PhotronPlayback) {
    setPlayback();
    
  } else if (function == PhotronReadMem) {
    readMem();
    
  } else if ((function == ADTriggerMode) || (function == PhotronAfterFrames) ||
            (function == PhotronRandomFrames) || (function == PhotronRecCount)) {
    printf("function = %d\n", function);
    setTriggerMode();
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


asynStatus Photron::softwareTrigger() {
  asynStatus status = asynSuccess;
  int acqMode;
  unsigned long nRet, nErrorCode;
  static const char *functionName = "softwareTrigger";
  
  status = getIntegerParam(PhotronAcquireMode, &acqMode);
  
  // Only send a software trigger if in Record mode
  if (acqMode == 1) {
    nRet = PDC_TriggerIn(nDeviceNo, &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_TriggerIn failed. error = %d\n", nErrorCode);
      return asynError;
    }
  } else {
    printf("Ignoring software trigger\n");
  }
  
  return status;
}


asynStatus Photron::setRecReady() {
  asynStatus status = asynSuccess;
  int acqMode;
  unsigned long nRet, nErrorCode;
  static const char *functionName = "setRecReady";
  
  status = getIntegerParam(PhotronAcquireMode, &acqMode);
  
  // Only set rec ready if in record mode
  if (acqMode == 1) {
    nRet = PDC_SetRecReady(nDeviceNo, &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_SetRecReady failed. error = %d\n", nErrorCode);
      return asynError;
    }
  } else {
    printf("Ignoring set rec ready\n");
  }
  
  return status;
}


asynStatus Photron::setEndless() {
  asynStatus status = asynSuccess;
  int acqMode;
  unsigned long nRet, nErrorCode;
  static const char *functionName = "setEndless";
  
  status = getIntegerParam(PhotronAcquireMode, &acqMode);
  
  // Only set endless trigger if in record mode
  // TODO: add test for relevent trigger modes
  if (acqMode == 1) {
    nRet = PDC_SetEndless(nDeviceNo, &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_SetEndless failed. error = %d\n", nErrorCode);
      return asynError;
    }
  } else {
    printf("Ignoring endless trigger\n");
  }
  
  return status;
}


asynStatus Photron::setLive() {
  asynStatus status = asynSuccess;
  int acqMode;
  unsigned long nRet, nErrorCode;
  static const char *functionName = "setPlayback";
  
  status = getIntegerParam(PhotronAcquireMode, &acqMode);
  
  // Put the camera in live mode
  nRet = PDC_SetStatus(this->nDeviceNo, PDC_STATUS_LIVE, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_SetStatus failed. error = %d\n", nErrorCode);
    return asynError;
  }
  
  return status;
}


asynStatus Photron::setPlayback() {
  asynStatus status = asynSuccess;
  int acqMode;
  unsigned long nRet, nErrorCode;
  static const char *functionName = "setPlayback";
  
  status = getIntegerParam(PhotronAcquireMode, &acqMode);
  
  // Only set playback if in record mode
  if (acqMode == 1) {
    // Put the camera in playback mode
    nRet = PDC_SetStatus(this->nDeviceNo, PDC_STATUS_PLAYBACK, &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_SetStatus failed. error = %d\n", nErrorCode);
      return asynError;
    }
  } else {
    printf("Ignoring playback\n");
  }
  
  return status;
}


asynStatus Photron::readMem() {
  asynStatus status = asynSuccess;
  int acqMode, phostat, index;
  unsigned long nRet, nErrorCode;
  PDC_FRAME_INFO FrameInfo;
  static const char *functionName = "readMem";
  
  status = getIntegerParam(PhotronAcquireMode, &acqMode);
  status = getIntegerParam(PhotronStatus, &phostat);
  
  // Only read memory if in record mode
  // AND status is playback
  if (acqMode == 1) {
    if (phostat == PDC_STATUS_PLAYBACK) {
      // Retrieves frame information 
      nRet = PDC_GetMemFrameInfo(nDeviceNo, this->nChildNo, &FrameInfo,
                                 &nErrorCode);
      if (nRet == PDC_FAILED) {
        printf("PDC_GetMemFrameInfo Error %d\n", nErrorCode);
        return asynError;
      }
        
      // display frame info
      printf("Frame Info:\n");
      printf("\tFrame Start:\t%d\n", FrameInfo.m_nStart);
      printf("\tFrame Trigger:\t%d\n", FrameInfo.m_nTrigger);
      printf("\tFrame End:\t%d\n", FrameInfo.m_nEnd);
      printf("\t2S Low->High:\t%d\n", FrameInfo.m_nTwoStageLowToHigh);
      printf("\t2S High->Low:\t%d\n", FrameInfo.m_nTwoStageHighToLow);
      printf("\tEvent frame numbers:\n");
      for (index=0; index<10; index++) {
        printf("\t\ti=%d\tframe: %d\n", index, FrameInfo.m_nEvent[index]);
      }
      printf("\tEvent count:\t%d\n", FrameInfo.m_nEventCount);
      printf("\tRecorded Frames:\t%d\n", FrameInfo.m_nRecordedFrames);
        
      // PDC_GetMemResolution
      // PDC_GetMemRecordRate
      // PDC_GetMemTriggerMode
      
      // Retrieves a trigger frame 
      /*nRet = PDC_GetMemImageData(nDeviceNo, nChildNo, FrameInfo.m_nTrigger,
                                 8, pBuf, &nErrorCode);
      if (nRet == PDC_FAILED) {
        printf("PDC_GetMemImageData Error %d\n", nErrorCode);
        free(pBuf); 
      }*/
      
    } else {
      printf("status != playback; Ignoring read mem\n");
    }
  } else {
    printf("Mode != record; Ignoring read mem\n");
  }
  
  return status;
}

// IAMHERE


asynStatus Photron::getGeometry() {
  int status = asynSuccess;
  int binX, binY;
  unsigned long minY, minX, sizeX, sizeY;
  static const char *functionName = "getGeometry";

  // Photron cameras don't allow binning
  binX = binY = 1;
  // Assume the reduce resolution images use the upper-left corner of the chip
  minX = minY = 0;
  
  status |= updateResolution();
  
  sizeX = this->width;
  sizeY = this->height;
  
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


asynStatus Photron::updateResolution() {
  unsigned long nRet;
  unsigned long nErrorCode;
  int status = asynSuccess;
  unsigned long sizeX, sizeY;
  unsigned long numSizesX, numSizesY;
  unsigned long width, height, value;
  int index;
  static const char *functionName = "updateResolution";

  // Is this needed or can we trust the values returned by setIntegerParam?
  nRet = PDC_GetResolution(this->nDeviceNo, this->nChildNo, 
                              &sizeX, &sizeY, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetResolution Error %d\n", nErrorCode);
    return asynError;
  }
  
  this->width = sizeX;
  this->height = sizeY;
  
  // We assume the resolution list is up-to-date (it should be updated by 
  // readParameters after the recording rate is modified
  
  // Only changing one dimension that results in another valid mode
  // for the same recording rate will not change the recording rate.
  // Find valid options for the current X and Y sizes
  numSizesX = numSizesY = 0;
  for (index=0; index<this->ResolutionListSize; index++) {
    value = this->ResolutionList[index];
    // height is the lower 16 bits of value
    height = value & 0xFFFF;
    // width is the upper 16 bits of value
    width = value >> 16;
    
    if (sizeX == width) {
      // This mode contains a valid value for Y
      this->ValidHeightList[numSizesY] = height;
      numSizesY++;
    }
    
    if (sizeY == height) {
      // This mode contains a valid value for X
      this->ValidWidthList[numSizesX] = width;
      numSizesX++;
    }
  }
  
  this->ValidWidthListSize = numSizesX;
  this->ValidHeightListSize = numSizesY;
  
  return asynSuccess;
}


asynStatus Photron::setValidWidth(epicsInt32 value) {
  int status = asynSuccess;
  int index;
  epicsInt32 upperDiff, lowerDiff;
  static const char *functionName = "setValidWidth";
  
  // Update the list of valid X and Y sizes (these change with recording rate)
  updateResolution();
  
  if (this->ValidWidthListSize == 0) {
    printf("Error: ValidWidthListSize is ZERO\n");
    return asynError;
  }
  
  if (this->ValidWidthListSize == 1) {
    // Don't allow the value to be changed
    value = this->ValidWidthList[0];
  } else {
    /* Choose the closest allowed width 
       Note: this->ValidWidthList is in decending order */
    for (index=0; index<(this->ValidWidthListSize-1); index++) {
      if (value > this->ValidWidthList[index+1]) {
        upperDiff = (epicsInt32)this->ValidWidthList[index] - value;
        lowerDiff = value - (epicsInt32)this->ValidWidthList[index+1];
        // One of the widths (index or index+1) is the best choice
        if (upperDiff < lowerDiff) {
          printf("Replaced %d ", value);
          value = this->ValidWidthList[index];
          printf("with %d\n", value);
          break;
        } else {
          printf("Replaced %d ", value);
          value = this->ValidWidthList[index+1];
          printf("with %d\n", value);
          break;
        }
      } else {
        // Are we at the end of the list?
        if (index == this->ValidWidthListSize-2) {
          // Value is lower than the lowest rate
          printf("Replaced %d ", value);
          value = this->ValidWidthList[index+1];
          printf("with %d\n", value);
          break;
        } else {
          // We haven't found the closest width yet
          continue;
        }
      }
    }
  }
  
  status |= setIntegerParam(ADSizeX, value);
  status |= setGeometry();
  
  return (asynStatus)status;
}


asynStatus Photron::setValidHeight(epicsInt32 value) {
  int status = asynSuccess;
  int index;
  epicsInt32 upperDiff, lowerDiff;
  static const char *functionName = "setValidHeight";
  
  // Update the list of valid X and Y sizes (these change with recording rate)
  updateResolution();
  
  if (this->ValidHeightListSize == 0) {
    printf("Error: ValidHeightListSize is ZERO\n");
    return asynError;
  }
  
  if (this->ValidHeightListSize == 1) {
    // Don't allow the value to be changed
    value = this->ValidHeightList[0];
  } else {
    /* Choose the closest allowed width 
       Note: this->ValidHeightList is in decending order */
    for (index=0; index<(this->ValidHeightListSize-1); index++) {
      if (value > this->ValidHeightList[index+1]) {
        upperDiff = (epicsInt32)this->ValidHeightList[index] - value;
        lowerDiff = value - (epicsInt32)this->ValidHeightList[index+1];
        // One of the widths (index or index+1) is the best choice
        if (upperDiff < lowerDiff) {
          printf("Replaced %d ", value);
          value = this->ValidHeightList[index];
          printf("with %d\n", value);
          break;
        } else {
          printf("Replaced %d ", value);
          value = this->ValidHeightList[index+1];
          printf("with %d\n", value);
          break;
        }
      } else {
        // Are we at the end of the list?
        if (index == this->ValidHeightListSize-2) {
          // Value is lower than the lowest rate
          printf("Replaced %d ", value);
          value = this->ValidHeightList[index+1];
          printf("with %d\n", value);
          break;
        } else {
          // We haven't found the closest width yet
          continue;
        }
      }
    }
  }
  
  status |= setIntegerParam(ADSizeY, value);
  status |= setGeometry();
  
  return (asynStatus)status;
}


asynStatus Photron::setGeometry() {
  unsigned long nRet;
  unsigned long nErrorCode;
  int status = asynSuccess;
  int binX, binY, minY, minX, sizeX, sizeY, maxSizeX, maxSizeY;
  static const char *functionName = "setGeometry";
  
  // in the past updateResolution was called here
  
  /* Get all of the current geometry parameters from the parameter library */
  status = getIntegerParam(ADBinX, &binX);
  if (binX < 1)
    binX = 1;
  status = getIntegerParam(ADBinY, &binY);
  if (binY < 1)
    binY = 1;
  status = getIntegerParam(ADMinX, &minX);
  status = getIntegerParam(ADMinY, &minY);
  status = getIntegerParam(ADSizeX, &sizeX);
  status = getIntegerParam(ADSizeY, &sizeY);
  status = getIntegerParam(ADMaxSizeX, &maxSizeX);
  status = getIntegerParam(ADMaxSizeY, &maxSizeY);

  if (minX + sizeX > maxSizeX) {
    sizeX = maxSizeX - minX;
    setIntegerParam(ADSizeX, sizeX);
  }
  if (minY + sizeY > maxSizeY) {
    sizeY = maxSizeY - minY;
    setIntegerParam(ADSizeY, sizeY);
  }
  
  // There are fixed resolutions that can be used
  nRet = PDC_SetResolution(this->nDeviceNo, this->nChildNo, 
                           sizeX, sizeY, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_SetResolution Error %d\n", nErrorCode);
    return asynError;
  }

  if (status)
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s:%s: error, status=%d\n", driverName, functionName, status);

  return((asynStatus)status);
}


asynStatus Photron::setTriggerMode() {
  unsigned long nRet;
  unsigned long nErrorCode;
  int status = asynSuccess;
  int mode, apiMode, AFrames, RFrames, RCount, maxFrames;
  static const char *functionName = "setTriggerMode";
  
  status |= getIntegerParam(ADTriggerMode, &mode);
  status |= getIntegerParam(PhotronAfterFrames, &AFrames);
  status |= getIntegerParam(PhotronRandomFrames, &RFrames);
  status |= getIntegerParam(PhotronRecCount, &RCount);
  status |= getIntegerParam(PhotronMaxFrames, &maxFrames);
  
  // The mode isn't in the right format for the PDC_SetTriggerMode call
  switch (mode) {
    case 8:
      apiMode = PDC_TRIGGER_TWOSTAGE_HALF;
      break;
    case 9:
      apiMode = PDC_TRIGGER_TWOSTAGE_QUARTER;
      break;
    case 10:
      apiMode = PDC_TRIGGER_TWOSTAGE_ONEEIGHTH;
      break;
    default:
      apiMode = mode << 24;
      break;
  }
  
  // Set num random frames
  switch (apiMode) {
    case PDC_TRIGGER_RANDOM:
    case PDC_TRIGGER_RANDOM_RESET:
    case PDC_TRIGGER_RANDOM_CENTER:
    case PDC_TRIGGER_RANDOM_MANUAL:
      if (RFrames < 1) {
        RFrames = 1;
      } else if (RFrames > maxFrames) {
        RFrames = maxFrames;
      }
      break;
    default:
      // Non-random modes don't need random frames
      RFrames = 0;
      break;
  }
  
  // Set num after frames
  switch (apiMode) {
    case PDC_TRIGGER_MANUAL:
      if (AFrames < 1) {
        AFrames = 1;
      } else if (AFrames > maxFrames) {
        AFrames = maxFrames;
      }
      break;
    case PDC_TRIGGER_RANDOM_MANUAL:
      if (AFrames < 1) {
        AFrames = 1;
      } else if (AFrames > RFrames) {
        AFrames = RFrames;
      }
      break;
    default:
      AFrames = 0;
      break;
  }
  
  // TODO determine actual limits on RCount
  // PFV software limits num recordings to the following range: 1-10
  switch (apiMode) {
    case PDC_TRIGGER_RANDOM_CENTER:
    case PDC_TRIGGER_RANDOM_MANUAL:
      if (RCount < 1) {
        RCount = 1;
      } else if (RCount > 10) {
        RCount = 10;
      }
      break;
      
    default:
      RCount = 0;
      break;
  }
  
  nRet = PDC_SetTriggerMode(this->nDeviceNo, apiMode, AFrames, RFrames, RCount, 
                            &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_SetTriggerMode failed %d; apiMode = %x\n", nErrorCode, apiMode);
    return asynError;
  } else {
    printf("\tPDC_SetTriggerMode(-, %x, %d, %d, %d, -)\n", apiMode, AFrames, RFrames, RCount);
  }
  
  if (status)
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s:%s: error, status=%d\n", driverName, functionName, status);

  return((asynStatus)status);
}


asynStatus Photron::setPixelFormat() {
  int status = asynSuccess;
  int dataType;
  static const char *functionName = "setPixelFormat";
  
  status |= getIntegerParam(NDDataType, &dataType);
  
  if (dataType == NDUInt8) {
    this->pixelBits = 8;
  } else if (dataType == NDUInt16) {
    // The SA1.1 only has a 12-bit sensor
    this->pixelBits = 16;
  } else {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s:%s: error unsupported data type %d\n", 
              driverName, functionName, dataType);
    return asynError;
  }
  
  return asynSuccess;
}


asynStatus Photron::setTransferOption() {
  unsigned long nRet;
  unsigned long nErrorCode;
  int status = asynSuccess;
  int n8BitSel;
  
  static const char *functionName = "setTransferOption";
  
  status = getIntegerParam(Photron8BitSel, &n8BitSel);
  
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


asynStatus Photron::setRecordRate(epicsInt32 value) {
  unsigned long nRet;
  unsigned long nErrorCode;
  int status = asynSuccess;
  int index;
  epicsInt32 upperDiff, lowerDiff;
  
  static const char *functionName = "setRecordRate";
  
  if (this->RateListSize == 0) {
    printf("Error: RateListSize is ZERO\n");
    return asynError;
  }
  
  if (this->RateListSize == 1) {
    // Don't allow the value to be changed
    value = this->RateList[0];
  } else {
    /* Choose the closest allowed rate 
       NOTE: RateList is in ascending order */
    for (index=0; index<(this->RateListSize-1); index++) {
      if (value < this->RateList[index+1]) {
        upperDiff = (epicsInt32)this->RateList[index+1] - value;
        lowerDiff = value - (epicsInt32)this->RateList[index];
        // One of the rates (index or index+1) is the best choice
        if (upperDiff < lowerDiff) {
          value = this->RateList[index+1];
          break;
        } else {
          value = this->RateList[index];
          break;
        }
      } else {
        // Are we at the end of the list?
        if (index == this->RateListSize-2) {
          // value is higher than the highest rate
          value = this->RateList[index+1];
          break;
        } else {
          // We haven't found the closest rate yet
          continue;
        }
      }
    }
  }
  
  nRet = PDC_SetRecordRate(this->nDeviceNo, this->nChildNo, value, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_SetRecordRate Error %d\n", nErrorCode);
    return asynError;
  } else {
    printf("PDC_SetRecordRate succeeded. Rate = %d\n", value);
  }
  
  // Changing the record rate changes the current and available resolutions
  
  return asynSuccess;
}


asynStatus Photron::setStatus(epicsInt32 value) {
  unsigned long nRet;
  unsigned long nErrorCode;
  unsigned long desiredStatus;
  int status = asynSuccess;
  static const char *functionName = "setStatus";
  
  /* The Status PV is an mbbo with only two valid states
     The Photron FASTCAM SDK uses a bitmask with seven bits */
  // TODO: simplify this logic since only values of 1 and 0 will be written
  if (value <= 0 || value > 7) {
    desiredStatus = 0;
  } else {
    desiredStatus = 1 << (value - 1);
  }
  
  //printf("Output status = 0x%x\n", desiredStatus);
  nRet = PDC_SetStatus(this->nDeviceNo, desiredStatus, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_SetStatus Error %d\n", nErrorCode);
    return asynError;
  }
  
  return asynSuccess;
}


asynStatus Photron::readParameters() {
  unsigned long nRet;
  unsigned long nErrorCode;
  int status = asynSuccess;
  int tmode;
  char bitDepthChar;
  static const char *functionName = "readParameters";    
  
  //printf("Reading parameters...\n");
  
  //##############################################################################
  
  nRet = PDC_GetStatus(this->nDeviceNo, &(this->nStatus), &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetStatus failed %d\n", nErrorCode);
    return asynError;
  }
  status |= setIntegerParam(PhotronStatus, this->nStatus);
  
  nRet = PDC_GetRecordRate(this->nDeviceNo, this->nChildNo, &(this->nRate), &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetRecordRate failed %d\n", nErrorCode);
    return asynError;
  }
  status |= setIntegerParam(PhotronRecRate, this->nRate);
  
  nRet = PDC_GetMaxFrames(this->nDeviceNo, this->nChildNo, &(this->nMaxFrames),
                          &(this->nBlocks), &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetMaxFrames failed %d\n", nErrorCode);
    return asynError;
  }
  status |= setIntegerParam(PhotronMaxFrames, this->nMaxFrames);
  
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
  }
  
  // The raw trigger mode needs to be converted to the index of the mbbo/mbbi
  switch (this->triggerMode) {
    case PDC_TRIGGER_TWOSTAGE_HALF:
        tmode = 8;
      break;
      
    case PDC_TRIGGER_TWOSTAGE_QUARTER:
        tmode = 9;
      break;
      
    case PDC_TRIGGER_TWOSTAGE_ONEEIGHTH:
        tmode = 10;
      break;
    
    default:
        // this won't work for recon cmd and random loop modes
        tmode = this->triggerMode >> 24;
      break;
  }
  
  status |= setIntegerParam(ADTriggerMode, tmode);
  status |= setIntegerParam(PhotronAfterFrames, this->trigAFrames);
  status |= setIntegerParam(PhotronRandomFrames, this->trigRFrames);
  status |= setIntegerParam(PhotronRecCount, this->trigRCount);
  
  if (this->functionList[PDC_EXIST_BITDEPTH] == PDC_EXIST_SUPPORTED) {
    nRet = PDC_GetBitDepth(this->nDeviceNo, this->nChildNo, &bitDepthChar,
                           &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_GetBitDepth failed %d\n", nErrorCode);
      return asynError;
    } else {
      this->bitDepth = (unsigned long) bitDepthChar;
    }
  }
  
  // Does this ever change?
  nRet = PDC_GetRecordRateList(this->nDeviceNo, this->nChildNo, 
                               &(this->RateListSize), 
                               this->RateList, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetRecordRateList failed %d\n", nErrorCode);
    return asynError;
  } 
  
  // Can this be moved to the setRecordRate method? Does anything else effect it?
  nRet = PDC_GetResolutionList(this->nDeviceNo, this->nChildNo, 
                               &(this->ResolutionListSize),
                               this->ResolutionList, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetResolutionList failed %d\n", nErrorCode);
    return asynError;
  }
  
  // Does this ever change?
  nRet = PDC_GetTriggerModeList(this->nDeviceNo, &(this->TriggerModeListSize),
                                this->TriggerModeList, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetTriggerModeList failed %d\n", nErrorCode);
    return asynError;
  }
  
  if (functionList[PDC_EXIST_HIGH_SPEED_MODE] == PDC_EXIST_SUPPORTED) {
    nRet = PDC_GetHighSpeedMode(this->nDeviceNo, &(this->highSpeedMode),
                                &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_GetHighSpeedMode failed. Error %d\n", nErrorCode);
      return asynError;
    } 
  }
  
  // getGeometry needs to be called after the resolution list has been updated
  status |= getGeometry();
  
  /* Call the callbacks to update the values in higher layers */
  callParamCallbacks();

  if (status)
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: error, status=%d\n", driverName, functionName, status);
  return((asynStatus)status);
}


asynStatus Photron::parseResolutionList() {
  int index;
  unsigned long width, height, value;
  
  printf("  Available resolutions for rate=%d:\n", this->nRate);
  for (index=0; index<this->ResolutionListSize; index++) {
    value = this->ResolutionList[index];
    // height is the lower 16 bits of value
    height = value & 0xFFFF;
    // width is the upper 16 bits of value
    width = value >> 16;
    printf("\t%d\t%d x %d\n", index, width, height);
  }
  
  return asynSuccess;
}


void Photron::printResOptions() {
  int index;
  
  printf("  Valid heights for rate=%d and width=%d\n", this->nRate, this->width);
  for (index=0; index<this->ValidHeightListSize; index++) {
    printf("\t%d\n", this->ValidHeightList[index]);
  }
  
  printf("\n  Valid widths for rate=%d and height=%d\n", this->nRate, this->height);
  for (index=0; index<this->ValidWidthListSize; index++) {
    printf("\t%d\n", this->ValidWidthList[index]);
  }
}


void Photron::printTrigModes() {
  int index;
  int mode;
  
  printf("\n  Trigger Modes:\n");
  for (index=0; index<this->TriggerModeListSize; index++) {
    mode = this->TriggerModeList[index] >> 24;
    if (mode == 8) {
      printf("\t%d:\t%d", index, mode);
      printf("\t%d\n", (this->TriggerModeList[index] & 0xF));
    } else {
      printf("\t%d:\t%d\n", index, mode);
    }
    
  }
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
    fprintf(fp, "  Device name:       %s\n",  this->deviceName);
    fprintf(fp, "  Device code:       %d\n",  (int)this->deviceCode);
    if (details > 8) {
      fprintf(fp, "  Device ID:         %d\n",  (int)this->deviceID);
      fprintf(fp, "  Product ID:        %d\n",  (int)this->productID);
      fprintf(fp, "  Lot ID:            %d\n",  (int)this->lotID);
      fprintf(fp, "  Individual ID:     %d\n",  (int)this->individualID);
    }
    fprintf(fp, "  Version:           %0.2f\n",  (float)(this->version/100.0));
    fprintf(fp, "  Sensor width:      %d\n",  (int)this->sensorWidth);
    fprintf(fp, "  Sensor height:     %d\n",  (int)this->sensorHeight);
    fprintf(fp, "  Sensor bits:       %d\n",  (int)this->sensorBits);
    fprintf(fp, "  Max Child Dev #:   %d\n",  (int)this->maxChildDevCount);
    fprintf(fp, "  Child Dev #:       %d\n",  (int)this->childDevCount);
    fprintf(fp, "\n");
    fprintf(fp, "  Width:             %d\n",  (int)this->width);
    fprintf(fp, "  Height:            %d\n",  (int)this->height);
    fprintf(fp, "  Camera Status:     %d\n",  (int)this->nStatus);
    fprintf(fp, "  Max Frames:        %d\n",  (int)this->nMaxFrames);
    fprintf(fp, "  Record Rate:       %d\n",  (int)this->nRate);
    fprintf(fp, "  Bit Depth:         %d\n",  (int)this->bitDepth);
    fprintf(fp, "\n");
    fprintf(fp, "  Trigger mode:      %x\n",  (int)this->triggerMode);
    fprintf(fp, "    A Frames:        %d\n",  (int)this->trigAFrames);
    fprintf(fp, "    R Frames:        %d\n",  (int)this->trigRFrames);
    fprintf(fp, "    R Count:         %d\n",  (int)this->trigRCount);
  }
  
  if (details > 4) {
    fprintf(fp, "  Available functions:\n");
    for( index=2; index<98; index++) {
      fprintf(fp, "    %d:         %d\n", index, this->functionList[index]);
    }
  }
  
  if (details > 2) {
    fprintf(fp, "\n  Available recording rates:\n");
    for (index=0; index<this->RateListSize; index++) {
      printf("\t%d:\t%d FPS\n", (index + 1), this->RateList[index]);
    }
    
    fprintf(fp, "\n");
    
    // Turn the resolution list into a more-usable form
    parseResolutionList();
    
    fprintf(fp, "\n");
    
    // 
    printResOptions();
    
    //
    printTrigModes();
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
