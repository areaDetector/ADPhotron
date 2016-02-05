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
               asynEnumMask, asynEnumMask, /* asynEnum interface for dynamic mbbi/o */
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
  createParam(PhotronStatusNameString,    asynParamInt32, &PhotronStatusName);
  createParam(PhotronCamModeString,       asynParamInt32, &PhotronCamMode);
  createParam(PhotronAcquireModeString,   asynParamInt32, &PhotronAcquireMode);
  createParam(PhotronOpModeString,        asynParamInt32, &PhotronOpMode);
  createParam(PhotronMaxFramesString,     asynParamInt32, &PhotronMaxFrames);
  createParam(Photron8BitSelectString,    asynParamInt32, &Photron8BitSel);
  createParam(PhotronRecordRateString,    asynParamInt32, &PhotronRecRate);
  createParam(PhotronChangeRecRateString, asynParamInt32, &PhotronChangeRecRate);
  createParam(PhotronResIndexString,      asynParamInt32, &PhotronResIndex);
  createParam(PhotronChangeResIdxString,  asynParamInt32, &PhotronChangeResIdx);
  createParam(PhotronShutterFpsString,    asynParamInt32, &PhotronShutterFps);
  createParam(PhotronChangeShutterFpsString, asynParamInt32, &PhotronChangeShutterFps);
  createParam(PhotronJumpShutterFpsString, asynParamInt32, &PhotronJumpShutterFps);
  createParam(PhotronVarChanString,       asynParamInt32, &PhotronVarChan);
  createParam(PhotronChangeVarChanString, asynParamInt32, &PhotronChangeVarChan);
  createParam(PhotronVarChanRateString,   asynParamInt32, &PhotronVarChanRate);
  createParam(PhotronVarChanXSizeString,  asynParamInt32, &PhotronVarChanXSize);
  createParam(PhotronVarChanYSizeString,  asynParamInt32, &PhotronVarChanYSize);
  createParam(PhotronVarChanXPosString,   asynParamInt32, &PhotronVarChanXPos);
  createParam(PhotronVarChanYPosString,   asynParamInt32, &PhotronVarChanYPos);
  createParam(PhotronVarChanWStepString,  asynParamInt32, &PhotronVarChanWStep);
  createParam(PhotronVarChanHStepString,  asynParamInt32, &PhotronVarChanHStep);
  createParam(PhotronVarChanXPosStepString,  asynParamInt32, &PhotronVarChanXPosStep);
  createParam(PhotronVarChanYPosStepString,  asynParamInt32, &PhotronVarChanYPosStep);
  createParam(PhotronVarChanWMinString,  asynParamInt32, &PhotronVarChanWMin);
  createParam(PhotronVarChanHMinString,  asynParamInt32, &PhotronVarChanHMin);
  createParam(PhotronVarChanFreePosString,  asynParamInt32, &PhotronVarChanFreePos);
  createParam(PhotronVarEditRateString,  asynParamInt32, &PhotronVarEditRate);
  createParam(PhotronVarEditXSizeString,  asynParamInt32, &PhotronVarEditXSize);
  createParam(PhotronVarEditYSizeString,  asynParamInt32, &PhotronVarEditYSize);
  createParam(PhotronVarEditXPosString,  asynParamInt32, &PhotronVarEditXPos);
  createParam(PhotronVarEditYPosString,  asynParamInt32, &PhotronVarEditYPos);
  createParam(PhotronVarEditApplyString,  asynParamInt32, &PhotronVarEditApply);
  createParam(PhotronVarEditEraseString,  asynParamInt32, &PhotronVarEditErase);
  createParam(PhotronChangeVarEditRateString,  asynParamInt32, &PhotronChangeVarEditRate);
  createParam(PhotronChangeVarEditXSizeString,  asynParamInt32, &PhotronChangeVarEditXSize);
  createParam(PhotronChangeVarEditYSizeString,  asynParamInt32, &PhotronChangeVarEditYSize);
  createParam(PhotronChangeVarEditXPosString,  asynParamInt32, &PhotronChangeVarEditXPos);
  createParam(PhotronChangeVarEditYPosString,  asynParamInt32, &PhotronChangeVarEditYPos);
  createParam(PhotronAfterFramesString,   asynParamInt32, &PhotronAfterFrames);
  createParam(PhotronRandomFramesString,  asynParamInt32, &PhotronRandomFrames);
  createParam(PhotronRecCountString,      asynParamInt32, &PhotronRecCount);
  createParam(PhotronSoftTrigString,      asynParamInt32, &PhotronSoftTrig);
  createParam(PhotronFrameStartString,    asynParamInt32, &PhotronFrameStart);
  createParam(PhotronFrameEndString,      asynParamInt32, &PhotronFrameEnd);
  createParam(PhotronLiveModeString,      asynParamInt32, &PhotronLiveMode);
  createParam(PhotronPreviewModeString,   asynParamInt32, &PhotronPreviewMode);
  createParam(PhotronPMStartString,       asynParamInt32, &PhotronPMStart);
  createParam(PhotronPMEndString,         asynParamInt32, &PhotronPMEnd);
  createParam(PhotronPMIndexString,       asynParamInt32, &PhotronPMIndex);
  createParam(PhotronChangePMIndexString, asynParamInt32, &PhotronChangePMIndex);
  createParam(PhotronPMFirstString,       asynParamInt32, &PhotronPMFirst);
  createParam(PhotronPMLastString,        asynParamInt32, &PhotronPMLast);
  createParam(PhotronPMPlayString,        asynParamInt32, &PhotronPMPlay);
  createParam(PhotronPMPlayRevString,     asynParamInt32, &PhotronPMPlayRev);
  createParam(PhotronPMPlayFPSString,     asynParamInt32, &PhotronPMPlayFPS);
  createParam(PhotronPMPlayMultString,    asynParamInt32, &PhotronPMPlayMult);
  createParam(PhotronPMRepeatString,      asynParamInt32, &PhotronPMRepeat);
  createParam(PhotronPMSaveString,        asynParamInt32, &PhotronPMSave);
  createParam(PhotronPMCancelString,      asynParamInt32, &PhotronPMCancel);
  createParam(PhotronIRIGString,          asynParamInt32, &PhotronIRIG);
  createParam(PhotronMemIRIGDayString,    asynParamInt32, &PhotronMemIRIGDay);
  createParam(PhotronMemIRIGHourString,   asynParamInt32, &PhotronMemIRIGHour);
  createParam(PhotronMemIRIGMinString,    asynParamInt32, &PhotronMemIRIGMin);
  createParam(PhotronMemIRIGSecString,    asynParamInt32, &PhotronMemIRIGSec);
  createParam(PhotronMemIRIGUsecString,   asynParamInt32, &PhotronMemIRIGUsec);
  createParam(PhotronMemIRIGSigExString,  asynParamInt32, &PhotronMemIRIGSigEx);
  createParam(PhotronSyncPriorityString,  asynParamInt32, &PhotronSyncPriority);
  createParam(PhotronExtIn1SigString,     asynParamInt32, &PhotronExtIn1Sig);
  createParam(PhotronExtIn2SigString,     asynParamInt32, &PhotronExtIn2Sig);
  createParam(PhotronExtIn3SigString,     asynParamInt32, &PhotronExtIn3Sig);
  createParam(PhotronExtIn4SigString,     asynParamInt32, &PhotronExtIn4Sig);
  createParam(PhotronExtOut1SigString,    asynParamInt32, &PhotronExtOut1Sig);
  createParam(PhotronExtOut2SigString,    asynParamInt32, &PhotronExtOut2Sig);
  createParam(PhotronExtOut3SigString,    asynParamInt32, &PhotronExtOut3Sig);
  createParam(PhotronExtOut4SigString,    asynParamInt32, &PhotronExtOut4Sig);
  
  PhotronExtInSig[0] = &PhotronExtIn1Sig;
  PhotronExtInSig[1] = &PhotronExtIn2Sig;
  PhotronExtInSig[2] = &PhotronExtIn3Sig;
  PhotronExtInSig[3] = &PhotronExtIn4Sig;
  PhotronExtOutSig[0] = &PhotronExtOut1Sig;
  PhotronExtOutSig[1] = &PhotronExtOut2Sig;
  PhotronExtOutSig[2] = &PhotronExtOut3Sig;
  PhotronExtOutSig[3] = &PhotronExtOut4Sig;
  
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
  this->resumeRecEventId = epicsEventCreate(epicsEventEmpty);
  if (!this->resumeRecEventId) {
    printf("%s:%s epicsEventCreate failure for resume rec event\n",
           driverName, functionName);
    return;
  }
  
  // Create epicsEvents for signaling the play task when to start playback of
  // recorded images
  this->startPlayEventId = epicsEventCreate(epicsEventEmpty);
  if (!this->startPlayEventId) {
    printf("%s:%s epicsEventCreate failure for start play event\n",
           driverName, functionName);
    return;
  }
  
  this->stopPlayEventId = epicsEventCreate(epicsEventEmpty);
  if (!this->stopPlayEventId) {
    printf("%s:%s epicsEventCreate failure for stop play event\n",
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
  
  /* Create the thread that plays back recordings from memory */
  status = (epicsThreadCreate("PhotronPlayTask", epicsThreadPriorityMedium,
                epicsThreadGetStackSize(epicsThreadStackMedium),
                (EPICSTHREADFUNC)PhotronPlayTaskC, this) == NULL);
  if (status) {
    printf("%s:%s epicsThreadCreate failure for play task\n",
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
  
  // Does this need to be called before readParameters reads the trigger mode?
  createStaticEnums();
  createDynamicEnums();
}


Photron::~Photron() {
  cameraNode *pNode = (cameraNode *)ellFirst(cameraList);
  static const char *functionName = "~Photron";

  // Attempt to stop the recording thread
  this->stopRecFlag = 1;
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


void Photron::timeDataToSec(PPDC_IRIG_INFO tData, double *seconds) {
  *seconds = (((((((*tData).m_nDayOfYear * 24) + (*tData).m_nHour) * 60) + (*tData).m_nMinute) * 60) + (*tData).m_nSecond) * 1.0;
  *seconds += ((*tData).m_nMicroSecond / 1.0e6);
}


static void PhotronPlayTaskC(void *drvPvt) {
  Photron *pPvt = (Photron *)drvPvt;
  pPvt->PhotronPlayTask();
}

/** This thread retrieves the image data efficiently when play is pressed in
  * playback mode.
  */
void Photron::PhotronPlayTask() {
  //unsigned long status;
  unsigned long nRet;
  unsigned long nErrorCode;
  epicsInt32 phostat, start, end, repeat, current;
  epicsInt32 fps, multiplier;
  int index, nextIndex, stop;
  //
  int transferBitDepth;
  PDC_IRIG_INFO tData;
  //
  NDArray *pImage;
  NDArrayInfo_t arrayInfo;
  int colorMode = NDColorModeMono;
  //
  void *pBuf;  /* Memory sequence pointer for storing a live image */
  //
  NDDataType_t dataType;
  int pixelSize;
  size_t dims[2];
  size_t dataSize;
  //
  int imageCounter;
  int numImagesCounter;
  int arrayCallbacks;
  double updatePeriod, delay;
  double elapsedTime;
  double tRel, tStart, tNow;
  epicsTimeStamp startTime, endTime;
  //
  const char *functionName = "PhotronPlayTask";
  
  this->lock();
  /* Loop forever */
  while (1) {
    /* Release the lock while we wait for an play event, then lock again */
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
              "%s:%s: waiting for play to be requested\n", driverName, 
              functionName);
    this->unlock();
    printf("PhtronPlayTask is SLEEPING!!!\n");
    epicsEventWait(this->startPlayEventId);
    this->lock();
    
    printf("PhotronPlayTask is ALIVE!!!\n");
    
    getIntegerParam(PhotronStatus, &phostat);
    
    // Only play images if we're in playback mode
    if (phostat == 1) {
      getIntegerParam(PhotronPMStart, &start);
      getIntegerParam(PhotronPMEnd, &end);
      getIntegerParam(PhotronPMIndex, &current);
      
      if (this->pixelBits == 8) {
        // 8 bits
        dataType = NDUInt8;
        pixelSize = 1;
      } else {
        // 12 bits (stored in 2 bytes)
        dataType = NDUInt16;
        pixelSize = 2;
      }
      //
      dims[0] = this->memWidth;
      dims[1] = this->memHeight;
      
      //
      transferBitDepth = 8 * pixelSize;
      dataSize = this->memWidth * this->memHeight * pixelSize;
      pBuf = malloc(dataSize);
      
      // Start with the current start frame. If we're at the end, restart from
      // the beginning.
      if ((this->dirFlag == 1) && (current == end)) {
        index = start;
      } else if ((this->dirFlag) == 0 && (current == start)) {
        index = end;
      } else {
        index = current;
      }
      
      // Preload the first frame
      nRet = PDC_GetMemImageDataStart(this->nDeviceNo, this->nChildNo, index,
                                      transferBitDepth, pBuf, &nErrorCode);
      if (nRet == PDC_FAILED) {
        printf("PDC_GetMemImageDataStart Error %d; index = %d\n", nErrorCode, index);
      }
      
      epicsTimeGetCurrent(&startTime);
      
      while (1) {
        // Acquire the image data
        nRet = PDC_GetMemImageDataEnd(this->nDeviceNo, this->nChildNo,
                                        transferBitDepth, pBuf, &nErrorCode);
        if (nRet == PDC_FAILED) {
          printf("PDC_GetMemImageDataEnd Error %d\n", nErrorCode);
        }
        
        setIntegerParam(PhotronPMIndex, index);
        
        // Retrieve frame time
        if (this->tMode == 1) {
          //
          nRet = PDC_GetMemIRIGData(this->nDeviceNo, this->nChildNo, index,
                                    &tData, &nErrorCode);
          if (nRet == PDC_FAILED) {
            printf("PDC_GetMemIRIGData Error %d\n", nErrorCode);
          }
          
          setIntegerParam(PhotronMemIRIGDay, tData.m_nDayOfYear);
          setIntegerParam(PhotronMemIRIGHour, tData.m_nHour);
          setIntegerParam(PhotronMemIRIGMin, tData.m_nMinute);
          setIntegerParam(PhotronMemIRIGSec, tData.m_nSecond);
          setIntegerParam(PhotronMemIRIGUsec, tData.m_nMicroSecond);
          setIntegerParam(PhotronMemIRIGSigEx, tData.m_ExistSignal);
        }
        
        /* We save the most recent image buffer so it can be used in the read() 
         * function. Now release it before getting a new version. */
        if (this->pArrays[0]) 
          this->pArrays[0]->release();
  
        /* Allocate the raw buffer */
        pImage = this->pNDArrayPool->alloc(2, dims, dataType, 0, NULL);
        if (!pImage) {
          asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: error allocating buffer\n", driverName, functionName);
        }
        
        //
        memcpy(pImage->pData, pBuf, dataSize);
        
        // Allow repeat and multiplier to be changed during playback
        getIntegerParam(PhotronPMRepeat, &repeat);
        getIntegerParam(PhotronPMPlayMult, &multiplier);
        
        // Determine if another frame should start preloading based on index
        if (this->dirFlag == 1) {
          // forward direction
          if (index == end) {
            if (repeat == 1) {
              printf("It is time to REPEAT\n");
              printf("\tindex=%d, start=%d, end=%d\n", index, start, end);
              nextIndex = start;
              stop = 0;
            } else {
              nextIndex = end;
              stop = 1;
            }
          } else {
            nextIndex = index + multiplier;
            if (nextIndex > end) {
              nextIndex = end;
            }
            stop = 0;
          }
        } else {
          // reverse direction
          if (index == start) {
            if (repeat == 1) {
              printf("It is time to REPEAT\n");
              printf("\tindex=%d, start=%d, end=%d\n", index, start, end);
              nextIndex = end;
              stop = 0;
            } else {
              nextIndex = start;
              stop = 1;
            }
          } else {
            nextIndex = index - multiplier;
            if (nextIndex < start) {
              nextIndex = start;
            }
            stop = 0;
          }
        }
        
        // Allow the speed to be changed during playback
        getIntegerParam(PhotronPMPlayFPS, &fps);
        updatePeriod = 1.0 / fps;
        
        // delay if possible and necessary
        epicsTimeGetCurrent(&endTime);
        elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);
        delay = updatePeriod - elapsedTime;
        if (delay > 0) {
          //printf("delay = %f\n", delay);
          this->unlock();
          epicsEventWaitWithTimeout(this->stopPlayEventId, delay);
          this->lock();
        }
        epicsTimeGetCurrent(&startTime);
        
        // Check to see if the user requested playback to stop
        if (this->stopFlag == 1) {
          stop = 1;
        }
        
        //
        if (stop == 0) {
          // Start preloading the next frame
          nRet = PDC_GetMemImageDataStart(this->nDeviceNo, this->nChildNo, nextIndex,
                                          transferBitDepth, pBuf, &nErrorCode);
          if (nRet == PDC_FAILED) {
            printf("PDC_GetMemImageDataStart Error %d; nextIndex = %d\n", nErrorCode, nextIndex);
          }
        } else {
          printf("Stopping after posting this last image to plugins\n");
        }
        
        this->pArrays[0] = pImage;
        pImage->pAttributeList->add("ColorMode", "Color mode", NDAttrInt32, 
                                    &colorMode);
        pImage->getInfo(&arrayInfo);
        setIntegerParam(NDArraySize,  (int)arrayInfo.totalBytes);
        setIntegerParam(NDArraySizeX, (int)pImage->dims[0].size);
        setIntegerParam(NDArraySizeY, (int)pImage->dims[1].size);
        
        /* Call the callbacks to update any changes */
        callParamCallbacks();
        
         // Get params
        getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
        
        // Set the image counters during playback to the values they would have
        // if the frames were saved with the current settings
        imageCounter = this->NDArrayCounterBackup + index - start;
        setIntegerParam(NDArrayCounter, imageCounter);
        numImagesCounter = index - start;
        setIntegerParam(ADNumImagesCounter, numImagesCounter);
        
        /* Put the frame number and time stamp into the buffer */
        pImage->uniqueId = imageCounter;
        if (tMode == 1) {
          // Absolute time
          //irigSeconds = (((((tData.m_nDayOfYear * 24) + tData.m_nHour) * 60) + tData.m_nMinute) * 60) + tData.m_nSecond;
          //pImage->timeStamp = (this->postIRIGStartTime).secPastEpoch + irigSeconds + (this->postIRIGStartTime).nsec / 1.e9 + tData.m_nMicroSecond / 1.e6;
          // Relative time
          this->timeDataToSec(&tData, &tNow);
          this->timeDataToSec(&(this->tDataStart), &tStart);
          tRel = tNow - tStart;
          pImage->timeStamp = tRel;
        }
        else {
          // Use theoretical time
          pImage->timeStamp = 1.0 * index / this->memRate;
        }
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
        
        if (stop == 1) {
          printf("Breaking\n");
          break;
        } else {
          index = nextIndex;
        }
      }
      
      free(pBuf);
      
    } else {
      printf("Play was request but camera isn't in playback mode!\n");
    }
  }
}


static void PhotronRecTaskC(void *drvPvt) {
  Photron *pPvt = (Photron *)drvPvt;
  pPvt->PhotronRecTask();
}


/** This thread puts the camera in playback mode and reads recorded image data
  * from the camera after recording is done.
  */
void Photron::PhotronRecTask() {
  unsigned long status;
  unsigned long nRet;
  unsigned long nErrorCode;
  int acqMode, previewMode;
  int eStatus;
  
  const char *functionName = "PhotronRecTask";

  
  this->lock();
  /* Loop forever */
  while (1) {
    /* Are we in record mode? */
    getIntegerParam(PhotronAcquireMode, &acqMode);
    //printf("is acquisition active?\n");

    /* If we are not in record mode then wait for a semaphore that is given when 
       record mode is requested */
    if ((acqMode != 1) || (this->stopRecFlag == 1)) {
      /* Release the lock while we wait for an event that says acquire has 
         started, then lock again */
      asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                "%s:%s: waiting for acquire to start\n", driverName, 
                functionName);
      this->unlock();
      epicsEventWait(this->startRecEventId);
      this->lock();
      
      // Reset the stopRecFlag
      this->stopRecFlag = 0;
    }
    
    if (this->stopRecFlag == 1) {
      this->stopRecFlag = 0;
      
    }
    
    // Wait for triggered recording
    while (acqMode == 1) {
      // Get camera status
      nRet = PDC_GetStatus(this->nDeviceNo, &status, &nErrorCode);
      if (nRet == PDC_FAILED) {
        printf("PDC_GetStatus failed %d\n", nErrorCode);
      }
      setIntegerParam(PhotronStatus, status);
      if (status == PDC_STATUS_REC) {
          setIntegerParam(ADStatus, ADStatusAcquire);
      }
      eStatus = statusToEPICS(status);
      setIntegerParam(PhotronStatusName, eStatus);
      callParamCallbacks();
      
      // Triggered acquisition is done when camera status returns to live
      if (status == PDC_STATUS_LIVE) {
        //
        printf("!!!\tAcquisition is done\n");
        //epicsThreadSleep(1.0);
        //
        printf("Put camera in playback mode\n");
        setPlayback();
        //
        printf("Read info from camera\n");
        // readMem should set the readout params to the max?
        readMem();
        
        getIntegerParam(PhotronPreviewMode, &previewMode);
        
        // Optionally enter preview mode here
        if (previewMode) {
          // Wait until user is done previewing the data
          printf("Entering PREVIEW mode\n");
          this->unlock();
          epicsEventWait(this->resumeRecEventId);
          this->lock();
        }
        
        // Re-zero the num images complete (num will = total saved this acq)
        setIntegerParam(ADNumImagesCounter, 0);
        // Restore the image counter (num will = total saved since last reset)
        setIntegerParam(NDArrayCounter, this->NDArrayCounterBackup);
        callParamCallbacks();
        
        // Read specified image range here
        this->readImageRange();
        
        // Reset Acquire
        setIntegerParam(ADAcquire, 0);
        callParamCallbacks();
        
        //
        printf("Return camera to ready-to-trigger state\n");
        setRecReady();
      }
      
      // release the lock so the trigger PV can be used
      this->unlock();
      //epicsThreadSleep(0.001);
      epicsEventWaitWithTimeout(this->stopRecEventId, 0.001);
      this->lock();
      
      if (this->stopRecFlag == 1) {
        break;
      }
      
      // Update the acq mode
      getIntegerParam(PhotronAcquireMode, &acqMode);
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
  
  //printf("Connecting to the camera\n");
  
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
  //printf("\tnRet = %d\n", nRet);

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
  //printf("Getting camera info\n");
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
  //
  status |= setIntegerParam(PhotronVarChan, 1);
  
  if (status) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, 
              "%s:%s: unable to set camera parameters on camera %s\n",
              driverName, functionName, this->cameraId);
    return asynError;
  }
  
  /* Read the current camera settings */
  //printf("Reading camera parameters\n");
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
  
  nRet = PDC_GetExternalCount(this->nDeviceNo, &(this->inPorts), 
                              &(this->outPorts), &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetExternalCount failed %d\n", nErrorCode);
    return asynError;
  }
  
  // Do these mode lists need to be called from readParameters?
  // If the same mode is available on two ports, can it only be used with one?
  // PDC_EXTIO_MAX_PORT is defined in PDCVALUE.h
  for (index=0; index<PDC_EXTIO_MAX_PORT; index++) {
    // Input port
    if (index < (int)this->inPorts) {
      // Port exists, query the input list
      nRet = PDC_GetExternalInModeList(this->nDeviceNo, index+1,
                                       &(this->ExtInModeListSize[index]),
                                       this->ExtInModeList[index], &nErrorCode);
    } else {
      // Port doesn't exist; zero the list size
      this->ExtInModeListSize[index] = 0;
    }
    
    // Output port
    if (index < (int)this->outPorts) {
      // Port exists, query the input list
      nRet = PDC_GetExternalOutModeList(this->nDeviceNo, index+1,
                                       &(this->ExtOutModeListSize[index]),
                                       this->ExtOutModeList[index], &nErrorCode);
    } else {
      // Port doesn't exist; zero the list size
      this->ExtOutModeListSize[index] = 0;
    }
  }
  
  // Is this always the same or should it be moved to readParameters?
  nRet = PDC_GetSyncPriorityList(this->nDeviceNo, &(this->SyncPriorityListSize),
                                 this->SyncPriorityList, &nErrorCode);
  
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
  
  // Read variable restrictions (and print some info)
  readVariableInfo();
  
  // Query the trigger mode list
  // Is this necessary here?
  /*nRet = PDC_GetTriggerModeList(this->nDeviceNo, &(this->TriggerModeListSize),
                                this->TriggerModeList, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetTriggerModeList failed %d\n", nErrorCode);
    return asynError;
  } else {
    printf("\t!!! FIRST num trig modes = %d\n", this->TriggerModeListSize);
  }*/
  
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


/** Sets an float64 parameter.
  * \param[in] pasynUser asynUser structure that contains the function code in pasynUser->reason. 
  * \param[in] value The value for this parameter 
  *
  * Takes action if the function code requires it.  The PGPropertyValueAbs
  * function code makes calls to the Firewire library from this function. */
asynStatus Photron::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    asynStatus status = asynSuccess;
    int function = pasynUser->reason;
    double tempVal;
    static const char *functionName = "writeFloat64";
    
    /* Set the value in the parameter library.  This may change later but that's OK */
    status = setDoubleParam(function, value);
    
    if (function == ADAcquireTime) {
      // setRecordRate already does what we want
      if (value == 0.0) {
        tempVal = 1.0 / 1e-9;
      }
      else {
        tempVal = 1.0 / value;
      }
      setRecordRate((int)tempVal);
    } else {
      /* If this parameter belongs to a base class call its method */
      if (function < FIRST_PHOTRON_PARAM) status = ADDriver::writeFloat64(pasynUser, value);
    }
    
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s::%s function=%d, value=%f, status=%d\n",
              driverName, functionName, function, value, status);
    
    /* Read the camera parameters and do callbacks */
    readParameters();
    
    return status;
}

/** Called when asyn clients call pasynInt32->write().
  * This function performs actions for some parameters, including ADAcquire, ADBinX, etc.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus Photron::writeInt32(asynUser *pasynUser, epicsInt32 value) {
  int function = pasynUser->reason;
  int status = asynSuccess;
  int adstatus, acqMode, chan;
  int index;
  int skipReadParams = 0;
  static const char *functionName = "writeInt32";
  
  //printf("FUNCTION: %d - VALUE: %d\n", function, value);
  
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
  } else if (function == PhotronResIndex) {
    status |= setResolution(value);
  } else if (function == PhotronChangeResIdx) {
    status |= changeResIndex(value);
  } else if (function == ADAcquire) {
    getIntegerParam(PhotronAcquireMode, &acqMode);
    getIntegerParam(ADStatus, &adstatus);
    if (acqMode == 0) {
      // For Live mode, signal the PhotronTask
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
      // For Record mode
      if (value) {
        // Send a software trigger to start acquisition
        softwareTrigger();
        setIntegerParam(ADAcquire, 1);
      } else {
        // Ignore the stop request if status == waiting
        if (adstatus != ADStatusWaiting) {
          // Stop current (or next) readout
          this->abortFlag = 1;
          setIntegerParam(ADAcquire, 0);
          if (adstatus == ADStatusAcquire) {
            // Abort acquisition if it is in progress
            setLive();
          }
        }
      }
    }
  } else if (function == NDDataType) {
    status = setPixelFormat();
  } else if (function == PhotronAcquireMode) {
    // should the acquire state be checked?
    if (value == 0) {
      // code to return to live mode goes here
      setLive();
      
      // Stop the PhotronRecTask (will it do one last read after returning to live?)
      this->stopRecFlag = 1;
      epicsEventSignal(this->stopRecEventId);
    } else {
      // code to enter recording mode
      setRecReady();
      
      // Wake up the PhotronRecTask
      epicsEventSignal(this->startRecEventId);
    }
  } else if (function == PhotronOpMode) {
    // What should be done when this mode is changed?
    if (value == 1) {
      // Switch to Variable mode by applying the currently selected variable channel
      getIntegerParam(PhotronVarChan, &chan);
      setVariableChannel(chan);
    } else {
      // Switch to Default mode
      // TODO: remember desired rec rate
      setRecordRate(this->desiredRate);
    }
  } else if (function == PhotronVarChan) {
    setVariableChannel(value);
  } else if (function == PhotronChangeVarChan) {
    changeVariableChannel(value);
  } else if (function == PhotronVarEditRate) {
    setVariableRecordRate(value);
  } else if (function == PhotronChangeVarEditRate) {
    changeVariableRecordRate(value);
  } else if (function == PhotronVarEditXSize) {
    // IAMHERE
    //setVariableXSize(value);
  } else if (function == Photron8BitSel) {
    /* Specifies the bit position during 8-bit transfer from a device of more 
       than 8 bits. */
    setTransferOption();
  } else if (function == PhotronRecRate) {
    setRecordRate(value);
  } else if (function == PhotronChangeRecRate) {
    changeRecordRate(value);
  } else if (function == PhotronShutterFps) {
    setShutterSpeedFps(value);
  } else if (function == PhotronChangeShutterFps) {
    changeShutterSpeedFps(value);
  } else if (function == PhotronJumpShutterFps) {
    jumpShutterSpeedFps(value);
  } else if (function == PhotronStatus) {
    setStatus(value);
  } else if (function == PhotronSoftTrig) {
    //printf("Soft Trigger changed. value = %d\n", value);
    softwareTrigger();
  } else if (function == PhotronLiveMode) {
    // Manually returning to live mode is necessary when eternally triggering
    // random modes and it is desirable to readout data before the internal
    // memory is full.
    if (value == 1) {
      setLive();
    }
  } else if ((function == ADTriggerMode) || (function == PhotronAfterFrames) ||
            (function == PhotronRandomFrames) || (function == PhotronRecCount)) {
    //printf("function = %d\n", function);
    setTriggerMode();
  } else if (function == PhotronPreviewMode) {
    // Do nothing
  } else if (function == PhotronPMIndex) {
    // grab and display an image from memory
    setPMIndex(value);
    skipReadParams = 1;
  } else if (function == PhotronChangePMIndex) {
    changePMIndex(value);
    skipReadParams = 1;
  } else if (function == PhotronPMFirst) {
    getIntegerParam(PhotronPMStart, &index);
    setPMIndex(index);
    setIntegerParam(PhotronPMIndex, index);
    skipReadParams = 1;
  } else if (function == PhotronPMLast) {
    getIntegerParam(PhotronPMEnd, &index);
    setPMIndex(index);
    setIntegerParam(PhotronPMEnd, index);
    skipReadParams = 1;
  } else if (function == PhotronPMStart) {
    setPreviewRange(function, value);
    skipReadParams = 1;
  } else if (function == PhotronPMEnd) {
    setPreviewRange(function, value);
    skipReadParams = 1;
  } else if (function == PhotronPMPlay) {
    if (value == 1) {
      printf("Playing Preview\n");
      
      // Reset the stop flag
      this->stopFlag = 0;
      
      // Set the dir flag
      this->dirFlag = 1;
      
      // Wake up the PhotronPlayTask
      epicsEventSignal(this->startPlayEventId);
    } else {
      printf("Stopping Preview\n");
      // Set a flag to stop playback to allow the play task to stop appropriately
      this->stopFlag = 1;
      
      // Wake up the PhotronPlayTask
      epicsEventSignal(this->stopPlayEventId);
    }
    skipReadParams = 1;
  } else if (function == PhotronPMPlayRev) {
    if (value == 1) {
      printf("Playing reverse preview\n");
      
      // Reset the stop flag
      this->stopFlag = 0;
      
      // Set the dir flag
      this->dirFlag = 0;
      
      // Wake up the PhotronPlayTask
      epicsEventSignal(this->startPlayEventId);
    } else {
      printf("Stopping reverse preview\n");
      // Set a flag to stop playback to allow the play task to stop appropriately
      this->stopFlag = 1;
      
      // Wake up the PhotronPlayTask
      epicsEventSignal(this->stopPlayEventId);
    }
    skipReadParams = 1;
  } else if (function == PhotronPMPlayFPS) {
    if (value < 1) {
      setIntegerParam(PhotronPMPlayFPS, 1);
    }
    skipReadParams = 1;
  } else if (function == PhotronPMPlayMult) {
    if (value < 1) {
      setIntegerParam(PhotronPMPlayMult, 1);
    }
    skipReadParams = 1;
  } else if (function == PhotronPMRepeat) {
    // Do nothing
    printf("PhotronPMRepeat: value = %d\n", value);
    skipReadParams = 1;
  } else if (function == PhotronPMCancel) {
    // TODO: do nothing if not it playback mode
    // Set the abort flag then resume the recording task
    printf("PMCancel %d\n", value);
    this->abortFlag = 1;
    epicsEventSignal(this->resumeRecEventId);
    skipReadParams = 1;
  } else if (function == PhotronPMSave) {
    // TODO: do nothing if not it playback mode
    // Send the signal to resume the recording task
    printf("PMSave %d\n", value);
    epicsEventSignal(this->resumeRecEventId);
    skipReadParams = 1;
  } else if (function == PhotronIRIG) {
  } else if (function == PhotronIRIG) {
    setIRIG(value);
  } else if (function == PhotronSyncPriority) {
    setSyncPriority(value);
  } else if (function == PhotronExtIn1Sig) {
    setExternalInMode(1, value);
  } else if (function == PhotronExtIn2Sig) {
    setExternalInMode(2, value);
  } else if (function == PhotronExtIn3Sig) {
    setExternalInMode(3, value);
  } else if (function == PhotronExtIn4Sig) {
    setExternalInMode(4, value);
  } else if (function == PhotronExtOut1Sig) {
    setExternalOutMode(1, value);
  } else if (function == PhotronExtOut2Sig) {
    setExternalOutMode(2, value);
  } else if (function == PhotronExtOut3Sig) {
    setExternalOutMode(3, value);
  } else if (function == PhotronExtOut4Sig) {
    setExternalOutMode(4, value);
  } else {
    /* If this is not a parameter we have handled call the base class */
    status = ADDriver::writeInt32(pasynUser, value);
  }
  
  if (skipReadParams == 1) {
    // Don't call readParameters() for PVs that can be changed during preview
    // Calling readParameters here results in locking issues
    callParamCallbacks();
  } else {
    // Read the camera parameters and do callbacks
    status |= readParameters();
  }
  
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


asynStatus Photron::readEnum(asynUser *pasynUser, char *strings[], int values[],
                             int severities[], size_t nElements, size_t *nIn) {
  int function = pasynUser->reason;
  enumStruct_t *pEnum;
  int numEnums;
  int i;
  
  if (function == ADTriggerMode) {
    pEnum = triggerModeEnums_;
    numEnums = numValidTriggerModes_;
  } else if (function == PhotronExtIn1Sig) {
    pEnum = inputModeEnums_[0];
    numEnums = numValidInputModes_[0];
  } else if (function == PhotronExtIn2Sig) {
    pEnum = inputModeEnums_[1];
    numEnums = numValidInputModes_[1];
  } else if (function == PhotronExtIn3Sig) {
    pEnum = inputModeEnums_[2];
    numEnums = numValidInputModes_[2];
  } else if (function == PhotronExtIn4Sig) {
    pEnum = inputModeEnums_[3];
    numEnums = numValidInputModes_[3];
  } else if (function == PhotronExtOut1Sig) {
    pEnum = outputModeEnums_[0];
    numEnums = numValidOutputModes_[0];
  } else if (function == PhotronExtOut2Sig) {
    pEnum = outputModeEnums_[1];
    numEnums = numValidOutputModes_[1];
  } else if (function == PhotronExtOut3Sig) {
    pEnum = outputModeEnums_[2];
    numEnums = numValidOutputModes_[2];
  } else if (function == PhotronExtOut4Sig) {
    pEnum = outputModeEnums_[3];
    numEnums = numValidOutputModes_[3];
  } else {
    *nIn = 0;
    return asynError;
  }
  
  for (i=0; ((i<numEnums) && (i<(int)nElements)); i++) {
    if (strings[i]) free(strings[i]);
    strings[i] = epicsStrDup(pEnum->string);
    values[i] = pEnum->value;
    severities[i] = 0;
    pEnum++;
  }
  *nIn = i;
  return asynSuccess;
}


asynStatus Photron::createDynamicEnums() {
  int index, mode;
  enumStruct_t *pEnum;
  unsigned long nRet, nErrorCode;
  char *enumStrings[NUM_TRIGGER_MODES];
  int enumValues[NUM_TRIGGER_MODES];
  int enumSeverities[NUM_TRIGGER_MODES];
  
  static const char *functionName = "createDynamicEnums";
  
  /* Trigger mode enums */
  nRet = PDC_GetTriggerModeList(this->nDeviceNo, &(this->TriggerModeListSize),
                                this->TriggerModeList, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetTriggerModeList failed %d\n", nErrorCode);
    return asynError;
  }
  
  //printf("\t!!! number of trigger modes detected: %d\n", this->TriggerModeListSize);
  // Is the following line necessary?
  //setIntegerParam(ADTriggerMode, this->triggerMode);
  numValidTriggerModes_ = 0;
  /* Loop over modes */
  for (index=0; index<(int)this->TriggerModeListSize; index++) {
    // get a pointer an element in the triggerModeEnums_ array
    pEnum = triggerModeEnums_ + numValidTriggerModes_;
    // convert the trigger mode
    mode = this->trigModeToEPICS(this->TriggerModeList[index]);
    strcpy(pEnum->string, triggerModeStrings[mode]);
    pEnum->value = mode;
    numValidTriggerModes_++;
  }
  for (index=0; index<numValidTriggerModes_; index++) {
    enumStrings[index] = triggerModeEnums_[index].string;
    enumValues[index] = triggerModeEnums_[index].value;
    enumSeverities[index] = 0;
  }
  doCallbacksEnum(enumStrings, enumValues, enumSeverities, 
                  numValidTriggerModes_, ADTriggerMode, 0);
  
  return asynSuccess;
}


asynStatus Photron::createStaticEnums() {
  /* This function creates enum strings and values for all enums that are fixed
  for a given camera.  It is only called once at startup */
  int index, port, mode;
  enumStruct_t *pEnum;
  //unsigned long nRet, nErrorCode;
  static const char *functionName = "createStaticEnums";
  
  //printf("Creating static enums\n");
  
  // I/O port lists were already acquired when getCameraInfo was called
  // Assume I/O port lists are static for now
  for (port=0; port<PDC_EXTIO_MAX_PORT; port++) {
    //
    numValidInputModes_[port] = 0;
    // ExtInModeList has values in hex; need to convert them to epics index
    for (index=0; index<(int)this->ExtInModeListSize[port]; index++) {
      pEnum = inputModeEnums_[port] + numValidInputModes_[port];
      mode = inputModeToEPICS(this->ExtInModeList[port][index]);
      strcpy(pEnum->string, inputModeStrings[mode]);
      pEnum->value = mode;
      numValidInputModes_[port]++;
    }

    numValidOutputModes_[port] = 0;
    // ExtOutModeList has values in hex; need to convert them to epics index
    for (index=0; index<(int)this->ExtOutModeListSize[port]; index++) {
      pEnum = outputModeEnums_[port] + numValidOutputModes_[port];
      mode = outputModeToEPICS(this->ExtOutModeList[port][index]);
      strcpy(pEnum->string, outputModeStrings[mode]);
      pEnum->value = mode;
      numValidOutputModes_[port]++;
    }
  }
  
  return asynSuccess;
}


int Photron::statusToEPICS(int apiStatus) {
  int status;
  
  switch (apiStatus) {
    case PDC_STATUS_LIVE:
      status = 0;
      break;
    case PDC_STATUS_PLAYBACK:
      status = 1;
      break;
    case PDC_STATUS_RECREADY:
      status = 2;
      break;
    case PDC_STATUS_ENDLESS:
      status = 3;
      break;
    case PDC_STATUS_REC:
      status = 4;
      break;
    case PDC_STATUS_SAVE:
      status = 5;
      break;
    case PDC_STATUS_LOAD:
      status = 6;
      break;
    case PDC_STATUS_PAUSE:
      status = 7;
      break;
    default:
      status = 0;
      break;
  }
  
  return status;
}


int Photron::inputModeToEPICS(int apiMode) {
  int mode;
  
  switch (apiMode) {
    case PDC_EXT_IN_ENCODER_POSI:
      mode = 15;
      break;
    case PDC_EXT_IN_ENCODER_NEGA:
      mode = 16;
      break;
    default:
      mode = apiMode - 1;
      break;
  }
  
  return mode;
}


int Photron::inputModeToAPI(int mode) {
  int apiMode;
  
  switch (mode) {
    case 15:
      apiMode = PDC_EXT_IN_ENCODER_POSI;
      break;
    case 16:
      apiMode = PDC_EXT_IN_ENCODER_NEGA;
      break;
    default:
      apiMode = mode + 1;
      break;
  }
  
  return apiMode;
}


int Photron::outputModeToEPICS(int apiMode) {
  int mode;
  
  if (apiMode < 0xF) {
    // 0x01 => 0 ; 0x0E => 13
    mode = apiMode - 1;
  } else if (apiMode < 0x4F) {
    // 0x1D => 14 ; 0x4E => 21
    mode = ((((apiMode & 0xF0) >> 4) - 1) * 2) + (apiMode & 0xF) + 1;
  } else if (apiMode < 0xFF) {
    // 0x50 => 22 ; 0x59 => 31
    mode = (apiMode & 0xF) + 22;
  } else {
    // 0x100 => 32 ; 0x102 => 34
    mode = (apiMode & 0xF) + 32;
  }
  
  return mode;
}


int Photron::outputModeToAPI(int mode) {
  int apiMode;
  
  if (mode <= 13) {
    apiMode = mode + 1;
  } else if (mode <= 21) {
    switch (mode) {
      case 14:
        apiMode = PDC_EXT_OUT_EXPOSE_H1_POSI;
        break;
      case 15:
        apiMode = PDC_EXT_OUT_EXPOSE_H1_NEGA;
        break;
      case 16:
        apiMode = PDC_EXT_OUT_EXPOSE_H2_POSI;
        break;
      case 17:
        apiMode = PDC_EXT_OUT_EXPOSE_H2_NEGA;
        break;
      case 18:
        apiMode = PDC_EXT_OUT_EXPOSE_H3_POSI;
        break;
      case 19:
        apiMode = PDC_EXT_OUT_EXPOSE_H3_NEGA;
        break;
      case 20:
        apiMode = PDC_EXT_OUT_EXPOSE_H4_POSI;
        break;
      case 21:
        apiMode = PDC_EXT_OUT_EXPOSE_H3_NEGA;
        break;
      default:
        // This can never happen
        apiMode = 0;
        break;
    }
  } else if (mode <= 31) {
    // The following calc could be simplified but it makes more sense in its
    // unsimplified state (see PDCValue.h)
    apiMode = mode - 22 + 0x50;
  } else if (mode <= 34) {
    apiMode = mode - 32 + 0x100;
  } else {
    // This can never happen
    apiMode = 0;
  }
  
  return apiMode;
}


int Photron::trigModeToEPICS(int apiMode) {
  int mode;
  
  // TODO: add final modes
  switch (apiMode) {
    case PDC_TRIGGER_TWOSTAGE_HALF:
        mode = 8;
      break;
      
    case PDC_TRIGGER_TWOSTAGE_QUARTER:
        mode = 9;
      break;
      
    case PDC_TRIGGER_TWOSTAGE_ONEEIGHTH:
        mode = 10;
      break;
    
    default:
        // this won't work for recon cmd and random loop modes
        mode = apiMode >> 24;
      break;
  }
  
  return mode;
}

int Photron::trigModeToAPI(int mode) {
  int apiMode;
  
  // TODO: add final two modes
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
  
  return apiMode;
}

asynStatus Photron::softwareTrigger() {
  asynStatus status = asynSuccess;
  int acqMode;
  unsigned long nRet, nErrorCode;
  static const char *functionName = "softwareTrigger";
  
  status = getIntegerParam(PhotronAcquireMode, &acqMode);
  
  // Only send a software trigger if in Record mode
  if (acqMode == 1) {
    nRet = PDC_TriggerIn(this->nDeviceNo, &nErrorCode);
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
  int acqMode, mode, apiMode;
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
    
    // This code is duplicated in setTriggerMode
    getIntegerParam(ADTriggerMode, &mode);
    
    // The mode isn't in the right format for the PDC_SetTriggerMode call
    apiMode = this->trigModeToAPI(mode);
    
    // Set endless for trigger modes that need it
    switch (apiMode) {
      case PDC_TRIGGER_CENTER:
      case PDC_TRIGGER_END:
      case PDC_TRIGGER_MANUAL:
      // Setting endless mode for random modes generates an extra recording
      // but only if fewer than the specified number of recordings are generated
      case PDC_TRIGGER_RANDOM_CENTER:
      case PDC_TRIGGER_RANDOM_MANUAL:
        //
        setEndless();
        break;
      default:
        //
        break;
    }
    
    //
    setIntegerParam(ADStatus, ADStatusWaiting);
    callParamCallbacks();
    
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
    nRet = PDC_SetEndless(this->nDeviceNo, &nErrorCode);
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
  static const char *functionName = "setLive";
  
  status = getIntegerParam(PhotronAcquireMode, &acqMode);
  
  // Put the camera in live mode
  nRet = PDC_SetStatus(this->nDeviceNo, PDC_STATUS_LIVE, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_SetStatus failed. error = %d\n", nErrorCode);
    return asynError;
  }
  
  setIntegerParam(ADStatus, ADStatusIdle);
  callParamCallbacks();
  
  return status;
}


asynStatus Photron::setIRIG(epicsInt32 value) {
  asynStatus status = asynSuccess;
  unsigned long nRet, nErrorCode;
  epicsUInt32 secDiff, nsecDiff;
  static const char *functionName = "setIRIG";

  if (this->functionList[PDC_EXIST_IRIG] == PDC_EXIST_SUPPORTED) {
    if (value) {
      // Enabling IRIG resets the internal clock
      epicsTimeGetCurrent(&(this->preIRIGStartTime));
      nRet = PDC_SetIRIG(this->nDeviceNo, PDC_FUNCTION_ON, &nErrorCode);
      epicsTimeGetCurrent(&(this->postIRIGStartTime));
      secDiff = (this->postIRIGStartTime).secPastEpoch - (this->preIRIGStartTime).secPastEpoch;
      nsecDiff = (this->postIRIGStartTime).nsec - (this->preIRIGStartTime).nsec;
      // Note: The time spent executing epicsTimeGetCurrent is negligible
      //   time to execute epicsTimeGetCurrent = 285 nsec
      //   time to execute PDC_SetIRIG = 40.57 msec
      // TODO: make the following printf an optional asyn trace message
      printf("IRIG clock correlation uncertainty: %d seconds and %d nanoseconds\n", secDiff, nsecDiff);
    } else {
      nRet = PDC_SetIRIG(this->nDeviceNo, PDC_FUNCTION_OFF, &nErrorCode);
    }
    if (nRet == PDC_FAILED) {
      printf("PDC_SetIRIG failed %d\n", nErrorCode);
      status = asynError;
    }
    else {
      //printf("PDC_SetIRIG succeeded. value=%d\n", value);
      
      // Changing the IRIG state can cause a change in the trigger mode
      createDynamicEnums();
    }
  }
  
  return status;
}


asynStatus Photron::setSyncPriority(epicsInt32 value) {
  asynStatus status = asynSuccess;
  unsigned long nRet, nErrorCode;
  static const char *functionName = "setSyncPriority";
  
  // PDC_SetSyncPriorityList
  if (this->functionList[PDC_EXIST_SYNC_PRIORITY] == PDC_EXIST_SUPPORTED) {
    nRet = PDC_SetSyncPriority(this->nDeviceNo, value, &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_SetSyncPriority failed %d\n", nErrorCode);
      status = asynError;
    }
  }
  
  return status;
}


asynStatus Photron::setExternalInMode(epicsInt32 port, epicsInt32 value) {
  asynStatus status = asynSuccess;
  unsigned long nRet, nErrorCode;
  int apiMode;
  static const char *functionName = "setExternalInMode";
  
  // Convert mbbo index to api
  apiMode = this->inputModeToAPI(value);
  
  //
  if ((port-1) < (int)this->inPorts) {
    //printf("\t\tPDC_SetExternalInMode( port = %d, apiMode = %d\n", port, apiMode);
    nRet = PDC_SetExternalInMode(this->nDeviceNo, port, apiMode, &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_SetExternalInMode failed %d\n", nErrorCode);
      status = asynError;
    }
  } else {
    //printf("Don't actually call PDC_SetExternalInMode; port %d doesn't exist\n", port);
  }
  return status;
}


asynStatus Photron::setExternalOutMode(epicsInt32 port, epicsInt32 value) {
  asynStatus status = asynSuccess;
  unsigned long nRet, nErrorCode;
  int apiMode;
  static const char *functionName = "setExternalOutMode";
  
  // Convert mbbo index to api
  apiMode = this->outputModeToAPI(value);
  
  //
  if ((port-1) < (int)this->outPorts) {
    //printf("\t\tPDC_SetExternalOutMode( port = %d, apiMode = %d\n", port, apiMode);
    nRet = PDC_SetExternalOutMode(this->nDeviceNo, port, apiMode, &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_SetExternalOutMode failed %d\n", nErrorCode);
      status = asynError;
    }
  } else {
    //printf("Don't actually call PDC_SetExternalOutMode; port %d doesn't exist\n", port);
  }
  
  return status;
}


asynStatus Photron::setPlayback() {
  asynStatus status = asynSuccess;
  int acqMode, eStatus;
  unsigned long nRet, nErrorCode, phostat;
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
    
    // Confirm that the camera is in playback mode
    nRet = PDC_GetStatus(this->nDeviceNo, &phostat, &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_GetStatus failed. error = %d\n", nErrorCode);
      return asynError;
    }
    
    if (phostat == PDC_STATUS_PLAYBACK) {
      setIntegerParam(PhotronStatus, phostat);
      eStatus = statusToEPICS(phostat);
      setIntegerParam(PhotronStatusName, eStatus);
      setIntegerParam(ADStatus, ADStatusReadout);
      callParamCallbacks();
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
  unsigned long memRate, memWidth, memHeight;
  unsigned long memTrigMode, memAFrames, memRFrames, memRCount;
  unsigned long tMode;
  PDC_IRIG_INFO tDataStart, tDataEnd;
  //
  int colorMode = NDColorModeMono;
  //
  static const char *functionName = "readMem";
  
  status = getIntegerParam(PhotronAcquireMode, &acqMode);
  status = getIntegerParam(PhotronStatus, &phostat);
  
  // Zero image counter
  setIntegerParam(ADNumImagesCounter, 0);
  callParamCallbacks();
  
  // Save the image counter (user can reset it whenever they want)
  getIntegerParam(NDArrayCounter, &(this->NDArrayCounterBackup));
  
  // Only read memory if in record mode
  // AND status is playback
  if (acqMode == 1) {
    if (phostat == PDC_STATUS_PLAYBACK) {
      // Retrieves frame information 
      nRet = PDC_GetMemFrameInfo(this->nDeviceNo, this->nChildNo, &FrameInfo,
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
      this->FrameInfo = FrameInfo;
      
      setIntegerParam(PhotronFrameStart, FrameInfo.m_nStart);
      setIntegerParam(PhotronFrameEnd, FrameInfo.m_nEnd);
      setIntegerParam(PhotronPMIndex, FrameInfo.m_nStart);
      setIntegerParam(PhotronPMStart, FrameInfo.m_nStart);
      setIntegerParam(PhotronPMEnd, FrameInfo.m_nEnd);
      
      // PDC_GetMemResolution
      nRet = PDC_GetMemResolution(this->nDeviceNo, this->nChildNo, &memWidth,
                                  &memHeight, &nErrorCode);
      if (nRet == PDC_FAILED) {
        printf("PDC_GetMemResolution Error %d\n", nErrorCode);
        return asynError;
      }
      printf("Memory Resolution: %d x %d\n", memWidth, memHeight);
      this->memWidth = memWidth;
      this->memHeight = memHeight;
      
      // PDC_GetMemRecordRate
      nRet = PDC_GetMemRecordRate(this->nDeviceNo, this->nChildNo, &memRate,
                                  &nErrorCode);
      if (nRet == PDC_FAILED) {
        printf("PDC_GetMemRecordRate Error %d\n", nErrorCode);
        return asynError;
      }
      printf("Memory Record Rate = %d Hz\n", memRate);
      this->memRate = memRate;
      
      // PDC_GetMemTriggerMode
      nRet = PDC_GetMemTriggerMode(this->nDeviceNo, this->nChildNo, 
                                   &memTrigMode, &memAFrames, &memRFrames, 
                                   &memRCount, &nErrorCode);
      if (nRet == PDC_FAILED) {
        printf("PDC_GetMemTriggerMode Error %d\n", nErrorCode);
        return asynError;
      }
      printf("Memory Trigger Mode = %d\n", memTrigMode);
      printf("Memory After Frames = %d\n", memAFrames);
      printf("Memory Random Frames = %d\n", memRFrames);
      printf("Memory Record Count = %d\n", memRCount);
      
      // PDC_GetMemIRIG
      nRet = PDC_GetMemIRIG(this->nDeviceNo, this->nChildNo, &tMode, &nErrorCode);
      if (nRet == PDC_FAILED) {
        printf("PDC_GetMemIRIG Error %d\n", nErrorCode);
        tMode = 0;
      } 
      printf("Memory IRIG mode: %d\n", tMode);
      if (tMode == 0) {
        setIntegerParam(PhotronMemIRIGDay, 0);
        setIntegerParam(PhotronMemIRIGHour, 0);
        setIntegerParam(PhotronMemIRIGMin, 0);
        setIntegerParam(PhotronMemIRIGSec, 0);
        setIntegerParam(PhotronMemIRIGUsec, 0);
        setIntegerParam(PhotronMemIRIGSigEx, 0);
      }
      this->tMode = tMode;
      
      // Retrieve frame time
      if (this->tMode == 1) {
        //
        nRet = PDC_GetMemIRIGData(this->nDeviceNo, this->nChildNo,
                                  FrameInfo.m_nStart, &tDataStart, &nErrorCode);
        if (nRet == PDC_FAILED) {
          printf("PDC_GetMemIRIGData Error %d\n", nErrorCode);
        }
        this->tDataStart = tDataStart;
        
        nRet = PDC_GetMemIRIGData(this->nDeviceNo, this->nChildNo,
                                  FrameInfo.m_nEnd, &tDataEnd, &nErrorCode);
        if (nRet == PDC_FAILED) {
          printf("PDC_GetMemIRIGData Error %d\n", nErrorCode);
        }
        this->tDataEnd = tDataEnd;
      
      }
      
      
    } else {
      printf("status != playback; Ignoring read mem\n");
    }
  } else {
    printf("Mode != record; Ignoring read mem\n");
  }
  
  callParamCallbacks();
  
  return status;
}


asynStatus Photron::setPreviewRange(epicsInt32 function, epicsInt32 value) {
  asynStatus status = asynSuccess;
  //int index;
  int start, end, frameStart, frameEnd;
  static const char *functionName = "setPreviewRange";
  
  // Should the index be changed here or left to be checked the next time it is changed?
  //getIntegerParam(PhotronPMIndex, &index);
  getIntegerParam(PhotronPMStart, &start);
  getIntegerParam(PhotronPMEnd, &end);
  getIntegerParam(PhotronFrameStart, &frameStart);
  getIntegerParam(PhotronFrameEnd, &frameEnd);
  
  if (function == PhotronPMStart) {
    printf("PhotronPMStart: value = %d\n", value);
    if (start > end) {
      start = end;
    }
    if (start < frameStart) {
      start = frameStart;
    }
    setIntegerParam(PhotronPMStart, start);
  } else if (function == PhotronPMEnd) {
    printf("PhotronPMEnd: value = %d\n", value);
    if (end < start) {
      end = start;
    }
    if (end > frameEnd) {
      end = frameEnd;
    }
    setIntegerParam(PhotronPMEnd, end);
  }
  
  return asynSuccess;
}


asynStatus Photron::setPMIndex(epicsInt32 value) {
  int status = asynSuccess;
  int start, end;
  static const char *functionName = "setPMIndex";
  
  status |= getIntegerParam(PhotronPMStart, &start);
  status |= getIntegerParam(PhotronPMEnd, &end);
  
  if (value < start) {
    value = start;
  }
  if (value > end) {
    value = end;
  }
  status |= setIntegerParam(PhotronPMIndex, value);
  
  status |= this->readMemImage(value);
  
  return (asynStatus)status;
}


asynStatus Photron::changePMIndex(epicsInt32 value) {
  int status = asynSuccess;
  epicsInt32 index;
  static const char *functionName = "changePMIndex";
  
  // check for playback mode?
  
  status |= getIntegerParam(PhotronPMIndex, &index);
  
  // check for valid index here?
  if (value > 0) {
    // Increase the preview mode index
    index++;
  } else {
    // Decrease the preview mode index
    index--;
  }
  
  // setPMIndex calls setIntegerParam, then calls readMemImage
  // readMemImage calls callParamCallbacks
  status |= this->setPMIndex(index);
  
  return (asynStatus)status;
}


// This is called during playback (preview) mode
// value has already been validated
asynStatus Photron::readMemImage(epicsInt32 value) {
  asynStatus status = asynSuccess;
  int transferBitDepth;
  unsigned long nRet, nErrorCode;
  PDC_IRIG_INFO tData;
  double tRel, tStart, tNow;
  //
  NDArray *pImage;
  NDArrayInfo_t arrayInfo;
  int colorMode = NDColorModeMono;
  //
  void *pBuf;  /* Memory sequence pointer for storing a live image */
  //
  NDDataType_t dataType;
  int pixelSize;
  size_t dims[2];
  size_t dataSize;
  //
  int imageCounter;
  int numImagesCounter;
  int arrayCallbacks;
  //double acquirePeriod, delay;
  epicsTimeStamp startTime;
  //epicsUInt32 irigSeconds;
  epicsInt32 start;
  //
  static const char *functionName = "readMemImage";
  
  // TODO: check that value is within range
  
  printf("readMemImage %d\n", value);
  
  if (this->pixelBits == 8) {
    // 8 bits
    dataType = NDUInt8;
    pixelSize = 1;
  } else {
    // 12 bits (stored in 2 bytes)
    dataType = NDUInt16;
    pixelSize = 2;
  }
  
  transferBitDepth = 8 * pixelSize;
  dataSize = this->memWidth * this->memHeight * pixelSize;
  pBuf = malloc(dataSize);
  
  epicsTimeGetCurrent(&startTime);
  
  // Retrieve a frame
  nRet = PDC_GetMemImageData(this->nDeviceNo, this->nChildNo, value,
                             transferBitDepth, pBuf, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetMemImageData Error %d\n", nErrorCode);
  } else {
    printf("PDC_GetMemImageData Succeeded\n");
  }
    
  // Retrieve frame time
  if (this->tMode == 1) {
    nRet = PDC_GetMemIRIGData(this->nDeviceNo, this->nChildNo, value,
                              &tData, &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_GetMemIRIGData Error %d\n", nErrorCode);
    }
    
    setIntegerParam(PhotronMemIRIGDay, tData.m_nDayOfYear);
    setIntegerParam(PhotronMemIRIGHour, tData.m_nHour);
    setIntegerParam(PhotronMemIRIGMin, tData.m_nMinute);
    setIntegerParam(PhotronMemIRIGSec, tData.m_nSecond);
    setIntegerParam(PhotronMemIRIGUsec, tData.m_nMicroSecond);
    setIntegerParam(PhotronMemIRIGSigEx, tData.m_ExistSignal);
  }
  
  /* We save the most recent image buffer so it can be used in the read() 
   * function. Now release it before getting a new version. */
  if (this->pArrays[0]) 
    this->pArrays[0]->release();
  
  /* Allocate the raw buffer */
  dims[0] = memWidth;
  dims[1] = memHeight;
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
  
  /* Call the callbacks to update any changes */
  callParamCallbacks();
  
  // Get the current parameters
  getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
  getIntegerParam(PhotronPMStart, &start);
  
  // Set the image counters during playback to the values they would have
  // if the frames were saved with the current settings
  imageCounter = this->NDArrayCounterBackup + value - start;
  setIntegerParam(NDArrayCounter, imageCounter);
  numImagesCounter = value - start;
  setIntegerParam(ADNumImagesCounter, numImagesCounter);
  
  /* Put the frame number and time stamp into the buffer */
  pImage->uniqueId = imageCounter;
  if (tMode == 1) {
    // Absolute time
    //irigSeconds = (((((tData.m_nDayOfYear * 24) + tData.m_nHour) * 60) + tData.m_nMinute) * 60) + tData.m_nSecond;
    //pImage->timeStamp = (this->postIRIGStartTime).secPastEpoch + irigSeconds + (this->postIRIGStartTime).nsec / 1.e9 + tData.m_nMicroSecond / 1.e6;
    // Relative time
    this->timeDataToSec(&tData, &tNow);
    this->timeDataToSec(&(this->tDataStart), &tStart);
    tRel = tNow - tStart;
    pImage->timeStamp = tRel;
  }
  else {
    // Use theoretical time
    pImage->timeStamp = 1.0 * value / this->memRate;
  }
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
  
  free(pBuf);
  printf("Returning...\n");
  return asynSuccess;
}




asynStatus Photron::readImageRange() {
  asynStatus status = asynSuccess;
  int index, transferBitDepth;
  unsigned long nRet, nErrorCode;
  PDC_IRIG_INFO tData;
  //
  NDArray *pImage;
  NDArrayInfo_t arrayInfo;
  int colorMode = NDColorModeMono;
  //
  void *pBuf;  /* Memory sequence pointer for storing a live image */
  //
  NDDataType_t dataType;
  int pixelSize;
  size_t dims[2];
  size_t dataSize;
  //
  int imageCounter;
  int numImages, numImagesCounter;
  int imageMode;
  int arrayCallbacks;
  //double acquirePeriod, delay;
  epicsTimeStamp startTime, endTime;
  double elapsedTime;
  epicsUInt32 irigSeconds;
  //
  int start, end;
  static const char *functionName = "readImageRange";
  
  if (this->pixelBits == 8) {
    // 8 bits
    dataType = NDUInt8;
    pixelSize = 1;
  } else {
    // 12 bits (stored in 2 bytes)
    dataType = NDUInt16;
    pixelSize = 2;
  }
  
  transferBitDepth = 8 * pixelSize;
  dataSize = this->memWidth * this->memHeight * pixelSize;
  pBuf = malloc(dataSize);
  
  epicsTimeGetCurrent(&startTime);
  
  getIntegerParam(PhotronPMStart, &start);
  getIntegerParam(PhotronPMEnd, &end);
  
  // TODO: Catch random trigger modes, see if fewer than the specified
  // number of recordings have occurred, then omit the first acquisition
  
  //for (index=this->FrameInfo.m_nStart; index<(this->FrameInfo.m_nEnd+1); index++) {
  for (index=start; index<(end+1); index++) {
    // Allow user to abort acquisition
    if (this->abortFlag == 1) {
      printf("Aborting data readout!d\n");
      this->abortFlag = 0;
      break;
    }
    
    // Retrieve a frame
    nRet = PDC_GetMemImageData(this->nDeviceNo, this->nChildNo, index,
                               transferBitDepth, pBuf, &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_GetMemImageData Error %d\n", nErrorCode);
    }
    
    // Retrieve frame time
    if (this->tMode == 1) {
    
      nRet = PDC_GetMemIRIGData(this->nDeviceNo, this->nChildNo, index,
                                &tData, &nErrorCode);
      if (nRet == PDC_FAILED) {
        printf("PDC_GetMemIRIGData Error %d\n", nErrorCode);
      }
      
      setIntegerParam(PhotronMemIRIGDay, tData.m_nDayOfYear);
      setIntegerParam(PhotronMemIRIGHour, tData.m_nHour);
      setIntegerParam(PhotronMemIRIGMin, tData.m_nMinute);
      setIntegerParam(PhotronMemIRIGSec, tData.m_nSecond);
      setIntegerParam(PhotronMemIRIGUsec, tData.m_nMicroSecond);
      setIntegerParam(PhotronMemIRIGSigEx, tData.m_ExistSignal);
    }
    
    /* We save the most recent image buffer so it can be used in the read() 
     * function. Now release it before getting a new version. */
    if (this->pArrays[0]) 
      this->pArrays[0]->release();
  
    /* Allocate the raw buffer */
    dims[0] = memWidth;
    dims[1] = memHeight;
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
    
    /* Call the callbacks to update any changes */
    callParamCallbacks();
  
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
    if (tMode == 1) {
      irigSeconds = (((((tData.m_nDayOfYear * 24) + tData.m_nHour) * 60) + tData.m_nMinute) * 60) + tData.m_nSecond;
      pImage->timeStamp = (this->postIRIGStartTime).secPastEpoch + irigSeconds + (this->postIRIGStartTime).nsec / 1.e9 + tData.m_nMicroSecond / 1.e6;
    }
    else {
      pImage->timeStamp = startTime.secPastEpoch + startTime.nsec / 1.e9;
    }
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
  
  epicsTimeGetCurrent(&endTime);
  elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);
  printf("Elapsed time: %f\n", elapsedTime);
  
  free(pBuf);
  
  return asynSuccess;
}


asynStatus Photron::getGeometry() {
  int status = asynSuccess;
  int binX, binY;
  unsigned long minY, minX, sizeX, sizeY;
  int resIndex;
  static const char *functionName = "getGeometry";

  // Photron cameras don't allow binning
  binX = binY = 1;
  
  status |= updateResolution();
  
  minX = this->xPos;
  minY = this->yPos;
  sizeX = this->width;
  sizeY = this->height;
  resIndex = this->resolutionIndex;
  
  status |= setIntegerParam(ADBinX,  binX);
  status |= setIntegerParam(ADBinY,  binY);
  status |= setIntegerParam(ADMinX,  minX*binX);
  status |= setIntegerParam(ADMinY,  minY*binY);
  status |= setIntegerParam(ADSizeX, sizeX*binX);
  status |= setIntegerParam(ADSizeY, sizeY*binY);
  status |= setIntegerParam(NDArraySizeX, sizeX);
  status |= setIntegerParam(NDArraySizeY, sizeY);
  status |= setIntegerParam(PhotronResIndex, resIndex);

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
  unsigned long xPos, yPos;
  unsigned long numSizesX, numSizesY;
  unsigned long width, height, value;
  int index;
  int resIndex;
  static const char *functionName = "updateResolution";
  
  // Get latest resolution list
  
  
  // Is this needed or can we trust the values returned by setIntegerParam?
  nRet = PDC_GetResolution(this->nDeviceNo, this->nChildNo, 
                              &sizeX, &sizeY, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetResolution Error %d\n", nErrorCode);
    return asynError;
  }
  
  //printf("RESOLUTION: %d x %d\n", sizeX, sizeY);
  this->width = sizeX;
  this->height = sizeY;
  
  nRet = PDC_GetSegmentPosition(this->nDeviceNo, this->nChildNo, &xPos, &yPos,
                                &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetSegmentPosition Error %d\n", nErrorCode);
  }
  
  this->xPos = xPos;
  this->yPos = yPos;
  
  // We assume the resolution list is up-to-date (it should be updated by 
  // readParameters after the recording rate is modified
  
  // Only changing one dimension that results in another valid mode
  // for the same recording rate will not change the recording rate.
  // Find valid options for the current X and Y sizes
  numSizesX = numSizesY = 0;
  resIndex = -1;
  for (index=0; index<(int)this->ResolutionListSize; index++) {
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
    
    if (sizeX == width && sizeY == height) {
      resIndex = index;
    }
  }
  
  this->ValidWidthListSize = numSizesX;
  this->ValidHeightListSize = numSizesY;
  this->resolutionIndex = resIndex;
  
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
    for (index=0; index<((int)this->ValidWidthListSize-1); index++) {
      if (value > (int)this->ValidWidthList[index+1]) {
        upperDiff = (epicsInt32)this->ValidWidthList[index] - value;
        lowerDiff = value - (epicsInt32)this->ValidWidthList[index+1];
        // One of the widths (index or index+1) is the best choice
        if (upperDiff < lowerDiff) {
          //printf("Replaced %d ", value);
          value = this->ValidWidthList[index];
          //printf("with %d\n", value);
          break;
        } else {
          //printf("Replaced %d ", value);
          value = this->ValidWidthList[index+1];
          //printf("with %d\n", value);
          break;
        }
      } else {
        // Are we at the end of the list?
        if (index == this->ValidWidthListSize-2) {
          // Value is lower than the lowest rate
          //printf("Replaced %d ", value);
          value = this->ValidWidthList[index+1];
          //printf("with %d\n", value);
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
    for (index=0; index<((int)this->ValidHeightListSize-1); index++) {
      if (value > (int)this->ValidHeightList[index+1]) {
        upperDiff = (epicsInt32)this->ValidHeightList[index] - value;
        lowerDiff = value - (epicsInt32)this->ValidHeightList[index+1];
        // One of the widths (index or index+1) is the best choice
        if (upperDiff < lowerDiff) {
          //printf("Replaced %d ", value);
          value = this->ValidHeightList[index];
          //printf("with %d\n", value);
          break;
        } else {
          //printf("Replaced %d ", value);
          value = this->ValidHeightList[index+1];
          //printf("with %d\n", value);
          break;
        }
      } else {
        // Are we at the end of the list?
        if (index == this->ValidHeightListSize-2) {
          // Value is lower than the lowest rate
          //printf("Replaced %d ", value);
          value = this->ValidHeightList[index+1];
          //printf("with %d\n", value);
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


asynStatus Photron::setResolution(epicsInt32 value) {
  int status = asynSuccess;
  int res, height, width;
  static const char *functionName = "setResolution";
  
  // Is this necessary? Is it possible that values were changed without
  // updateResolution already having been called at the end of readParameters?
  updateResolution();
  
  //printf("setResolution: value=%i\n", value);
  
  // Currently invalid selections are ignored.  Should the max or min value be 
  // chosen instead in the event of an invalid selection?
  if ((value >= 0) && (value < (int)this->ResolutionListSize)) {
    // Selection is valid
    
    res = this->ResolutionList[value];
    // height is the lower 16 bits of value
    height = res & 0xFFFF;
    // width is the upper 16 bits of value
    width = res >> 16;
    
    //printf("setResolution: %dx%d\n", width, height);
    
    status |= setIntegerParam(ADSizeX, width);
    status |= setIntegerParam(ADSizeY, height);
    
  } else {
    // Selection is invalid
    //printf("!!! Invalid resolution index! %d\n", value);
  }
  
  status |= setGeometry();
  
  return (asynStatus)status;
}


asynStatus Photron::changeResIndex(epicsInt32 value) {
  int status = asynSuccess;
  int resIndex;
  static const char *functionName = "changeResIndex";
  
  // The resolution list is in order of decreasing resolution, so increasing the
  // index reduces the resolution
  
  getIntegerParam(PhotronResIndex, &resIndex);
  
  // Only attempt to to change the index if the list has 2 or more elements
  if (this->ResolutionListSize > 1) {
    if (value > 0) {
      // Increase the res index
      if (resIndex < ((int)this->ResolutionListSize-1)) {
        resIndex++;
      }
    } else {
      // Decrease the res index
      if (resIndex > 0) {
        resIndex--;
      }
    }
    // Is this necessary?
    //setIntegerParam(PhotronResIndex, resIndex);
    this->setResolution(resIndex);
  }
  
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
    minX = maxSizeX - sizeX;
    setIntegerParam(ADMinX, minX);
  }
  if (minY + sizeY > maxSizeY) {
    minY = maxSizeY - sizeY;
    setIntegerParam(ADMinY, minY);
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
  int mode, apiMode, AFrames, RFrames, RCount, maxFrames, acqMode, phostat;
  static const char *functionName = "setTriggerMode";
  
  status |= getIntegerParam(PhotronStatus, &phostat);
  status |= getIntegerParam(ADTriggerMode, &mode);
  status |= getIntegerParam(PhotronAfterFrames, &AFrames);
  status |= getIntegerParam(PhotronRandomFrames, &RFrames);
  status |= getIntegerParam(PhotronRecCount, &RCount);
  status |= getIntegerParam(PhotronMaxFrames, &maxFrames);
  status |= getIntegerParam(PhotronAcquireMode, &acqMode);
  
  // Put the camera in live mode before changing the trigger mode
  if (phostat != PDC_STATUS_LIVE) {
    setLive();
  }
  
  // The mode isn't in the right format for the PDC_SetTriggerMode call
  apiMode = this->trigModeToAPI(mode);
  
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
  
  // Return camera to rec ready state if in record mode
  if (acqMode == 1) {
    setRecReady();
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


asynStatus Photron::findNearestValue(epicsInt32* pValue, int* pListIndex,
                                     unsigned long listSize,
                                     unsigned long* listName) {
  int index;
  epicsInt32 upperDiff, lowerDiff;
  static const char *functionName = "findNearestValue";
  
  if (listSize == 0) {
    printf("Error: List size is ZERO\n");
    return asynError;
  }
  
  if (listSize == 1) {
    // Don't allow the value to be changed
    *pValue = listName[0];
    *pListIndex = 0;
  } else {
    /* Choose the closest allowed rate 
       NOTE: listName must be in ascending order */
    for (index=0; index<((int)listSize-1); index++) {
      if (*pValue < (int)listName[index+1]) {
        upperDiff = (epicsInt32)listName[index+1] - *pValue;
        lowerDiff = *pValue - (epicsInt32)listName[index];
        // One of the values (index or index+1) is the best choice
        if (upperDiff < lowerDiff) {
          *pValue = listName[index+1];
          *pListIndex = index + 1;
          break;
        } else {
          *pValue = listName[index];
          *pListIndex = index;
          break;
        }
      } else {
        // Are we at the end of the list?
        if (index == listSize-2) {
          // value is higher than the highest rate
          *pValue = listName[index+1];
          *pListIndex = index + 1;
          break;
        } else {
          // We haven't found the closest rate yet
          continue;
        }
      }
    }
  }
  
  return asynSuccess;
}


asynStatus Photron::setVariableRecordRate(epicsInt32 value) {
  asynStatus status;
  static const char *functionName = "setVariableRecordRate";
  
  status = findNearestValue(&value, &(this->varRecRateIndex),
                            this->VariableRateListSize,
                            this->VariableRateList);
  
  if (status == asynSuccess) {
    // Update the param now that a valid value has been selected
    setIntegerParam(PhotronVarEditRate, value);
  }
  
  // Update max width/height here?
  
  return status;
}


asynStatus Photron::changeVariableRecordRate(epicsInt32 value) {
  int newVarRecRateIndex;
  int newVarRecRate;
  static const char *functionName = "changeVariableRecordRate";
  
  // The record rate list is in order of incresting rate
  // Assumption: this->varRecRateIndex is up-to-date
  
  newVarRecRateIndex = changeListIndex(value, this->varRecRateIndex,
                                            this->VariableRateListSize);
  
  if (newVarRecRateIndex != (int)this->varRecRateIndex) {
    // A valid change has been requested
    newVarRecRate = this->VariableRateList[newVarRecRateIndex];
    this->setVariableRecordRate(newVarRecRate);
  }
  
  return asynSuccess;
}

/*
asynStatus Photron::setVariableXSize(epicsInt32 value) {
  static const char *functionName = "setVariableXSize";
  
  // IAMHERE
}
*/

asynStatus Photron::setShutterSpeedFps(epicsInt32 value) {
  unsigned long nRet;
  unsigned long nErrorCode;
  asynStatus status;
  static const char *functionName = "setShutterSpeedFps";
  
  //printf("setShutterSpeedFps: value = %d\n", value);
  
  status = findNearestValue(&value, &(this->shutterSpeedFpsIndex),
                            this->ShutterSpeedFpsListSize,
                            this->ShutterSpeedFpsList);
  
  if (status == asynSuccess) {
    nRet = PDC_SetShutterSpeedFps(this->nDeviceNo, this->nChildNo, value, &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_SetShutterSpeedFps Error %d\n", nErrorCode);
      return asynError;
    } else {
      //printf("PDC_SetShutterSpeedFps succeeded. Rate = %d\n", value);
    }
  }
  
  return status;
}


// Given a request to increment or decrement a list index, a list, a list size,
// and a current list index, return the new list index
int Photron::changeListIndex(epicsInt32 value, unsigned long listIndex,
                             unsigned long listSize) {
  epicsInt32 retVal;
  
  if (listSize > 1) {
    if (value > 0) {
      // Increase the index
      if (listIndex < (listSize - 1)) {
        // There is room to increment the index
        retVal = listIndex + 1;
      } else {
        // We're already on the last element, return the current value
        retVal = listIndex;
      }
    } else {
      // Decrease the index
      if (listIndex > 0) {
        // There is room to decrement the index
        retVal = listIndex - 1;
      } else {
        // We're already on the first element, return the current value
        retVal = listIndex;
      }
    }
  } else {
    // list isn't long enough to change index, return the current value
    retVal = listIndex;
  }
  
  return retVal;
}


asynStatus Photron::changeShutterSpeedFps(epicsInt32 value) {
  int newShutterSpeedFpsIndex;
  int newShutterSpeedFps;
  static const char *functionName = "changeShutterSpeedFps";
  
  //printf("changeShutterSpeedFps: value = %d\n", value);
  
  // The record rate list is in order of incresting rate
  // Assumption: this->shutterSpeedFpsIndex is up-to-date
  
  newShutterSpeedFpsIndex = changeListIndex(value, this->shutterSpeedFpsIndex,
                                            this->ShutterSpeedFpsListSize);
  
  if (newShutterSpeedFpsIndex != (int)this->shutterSpeedFpsIndex) {
    // A valid change has been requested
    newShutterSpeedFps = this->ShutterSpeedFpsList[newShutterSpeedFpsIndex];
    this->setShutterSpeedFps(newShutterSpeedFps);
  }
  
  return asynSuccess;
}


asynStatus Photron::jumpShutterSpeedFps(epicsInt32 value) {
  int status = asynSuccess;
  int newShutterSpeedFpsIndex;
  int newShutterSpeedFps;
  static const char *functionName = "jumpShutterSpeedFps";
  
  //printf("jumpShutterSpeedFps: value = %d\n", value);
  
  // The record rate list is in order of incresting rate
  
  // Only attempt to to change the index if the list has 2 or more elements
  if (this->ShutterSpeedFpsListSize > 1) {
    if (value > 0) {
      // Jump to fastest shutter speed
      newShutterSpeedFpsIndex = this->ShutterSpeedFpsListSize - 1;
    } else {
      // Jump to slowest shutter speed
      newShutterSpeedFpsIndex = 0;
    }
    newShutterSpeedFps = this->ShutterSpeedFpsList[newShutterSpeedFpsIndex];
    this->setShutterSpeedFps(newShutterSpeedFps);
  }
  
  return (asynStatus)status;
}


asynStatus Photron::setRecordRate(epicsInt32 value) {
  unsigned long nRet;
  unsigned long nErrorCode;
  asynStatus status;
  int opMode;
  double acqTime;
  
  static const char *functionName = "setRecordRate";
  
  // Remember the desired rate
  this->desiredRate = value;
  
  getIntegerParam(PhotronOpMode, &opMode);
  
  // Only allow the record rate to be set in Default mode
  // Setting the record rate in Variable mode exits Variable mode, but the
  // OpMode PV can't be easily kept in sync.
  if (opMode == 1) {
    return asynSuccess;
  }
  
  if (this->nRate == value) {
    // New value is the same as the current value, do nothing so that the
    // current resolution settings are not lost
    return asynSuccess;
  }
  
  status = findNearestValue(&value, &(this->recRateIndex),
                            this->RateListSize, this->RateList);
  
  if (status != asynSuccess) {
    return status;
  }
  
  nRet = PDC_SetRecordRate(this->nDeviceNo, this->nChildNo, value, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_SetRecordRate Error %d\n", nErrorCode);
    return asynError;
  } else {
    //printf("PDC_SetRecordRate succeeded. Rate = %d\n", value);
  }
  
  // Keep the exposure time in sync with the record rate
  acqTime = 1.0 / value;
  setDoubleParam(ADAcquireTime, acqTime);
  
  // Changing the record rate changes the current and available resolutions
  
  return asynSuccess;
}


asynStatus Photron::changeRecordRate(epicsInt32 value) {
  int status = asynSuccess;
  int newRecRateIndex;
  int newRecRate;
  int opMode;
  static const char *functionName = "changeRecordRate";
  
  // The record rate list is in order of incresting rate
  // Assumption: this->recRateIndex is up-to-date
  
  // If in variable mode, don't do anything, since there is no good way to
  // provide the user feedback they're changing the desired record rate
  getIntegerParam(PhotronOpMode, &opMode);
  if (opMode == 1) {
    return asynSuccess;
  }
  
  newRecRateIndex = changeListIndex(value, this->recRateIndex,
                                    this->RateListSize);
  
  if (newRecRateIndex != (int)this->recRateIndex) {
    // A valid change has been requested
    newRecRate = this->RateList[newRecRateIndex];
    this->setRecordRate(newRecRate);
  }
  
  return (asynStatus)status;
}


asynStatus Photron::changeVariableChannel(epicsInt32 value) {
  int status = asynSuccess;
  int chan;
  static const char *functionName = "changeVariableChannel";
  
  status |= getIntegerParam(PhotronVarChan, &chan);
  
  // setVeriableChannel corrects invalid channels, so no checking is done here
  if (value > 0) {
    // Increase the channel index
    chan++;
  } else {
    // Decrease the channel index
    chan--;
  }
  
  status |= this->setVariableChannel(chan);
  
  return (asynStatus)status;
}


asynStatus Photron::setVariableChannel(epicsInt32 value) {
  unsigned long nRet;
  unsigned long nErrorCode;
  epicsInt32 tempVal;
  int status = asynSuccess;
  int chan;
  int opMode;
  static const char *functionName = "setVariableChannel";
  
  //printf("setVariableChannel: value = %d\n", value);
  
  // Channel = 0 in default mode, but zero isn't a valid arguement to the set call
  
  getIntegerParam(PhotronOpMode, &opMode);
  
  // Channel has a range of 1-20
  if (value < 1) {
    chan = 1;
  } else if (value > NUM_VAR_CHANS) {
    chan = NUM_VAR_CHANS;
  } else {
    chan = value;
  }
  
  // Read the variable channel settings here instead of in readParameters
  // because we only want the values to change when a chan change is attempted
  if (chan > 0) {
    nRet = PDC_GetVariableChannelInfo(this->nDeviceNo, chan, &(this->varRate),
                                      &(this->varWidth), &(this->varHeight),
                                      &(this->varXPos), &(this->varYPos),
                                      &nErrorCode);
  } else {
    // This should never happen. Move this to init instead?
    this->varRate = 0;
    this->varWidth = 0;
    this->varHeight = 0;
    this->varXPos = 0;
    this->varYPos = 0;
  }
  
  // Only apply the channel selection if the user is in variable mode
  // This allows the user to examine the settings while in default mode
  if (opMode == 1)
  {
    if (this->varRate > 59) {
      // Only set the variable channel if the channel is not empty
      nRet = PDC_SetVariableChannel(this->nDeviceNo, this->nChildNo, chan, 
                                    &nErrorCode);
      if (nRet == PDC_FAILED) {
        printf("PDC_SetVariableChannel Error %d\n", nErrorCode);
        return asynError;
      }
    }
  }
  
  // This is unecessary if the var change was changed directly, but the channel
  // can also be incremented/decremented
  setIntegerParam(PhotronVarChan, chan);
  
  // set the variable channel readbacks
  setIntegerParam(PhotronVarChanRate, this->varRate);
  setIntegerParam(PhotronVarChanXSize, this->varWidth);
  setIntegerParam(PhotronVarChanYSize, this->varHeight);
  setIntegerParam(PhotronVarChanXPos, this->varXPos);
  setIntegerParam(PhotronVarChanYPos, this->varYPos);
  // also update the var chan edit fields
  if (this->varRate > 59) {
    // Channel is defined, use the same values as the readbacks
    setIntegerParam(PhotronVarEditRate, this->varRate);
    setIntegerParam(PhotronVarEditXSize, this->varWidth);
    setIntegerParam(PhotronVarEditYSize, this->varHeight);
    setIntegerParam(PhotronVarEditXPos, this->varXPos);
    setIntegerParam(PhotronVarEditYPos, this->varYPos);
    // Tweaking the var edit rate can only work if the index is kept up-to-date
    this->varRecRateIndex = findListIndex(this->varRate, this->VariableRateListSize,
                                    this->VariableRateList);
  } else {
    // Channel is empty, populate the edit fields with valid settings
    setIntegerParam(PhotronVarEditRate, this->nRate);
    setIntegerParam(PhotronVarEditXSize, this->width);
    setIntegerParam(PhotronVarEditYSize, this->height);
    // Set image in the center
    getIntegerParam(ADMinX, &tempVal);
    setIntegerParam(PhotronVarEditXPos, tempVal);
    getIntegerParam(ADMinY, &tempVal);
    setIntegerParam(PhotronVarEditYPos, tempVal);
    // Tweaking the var edit rate can only work if the index is kept up-to-date
    this->varRecRateIndex = findListIndex(this->nRate, this->VariableRateListSize,
                                    this->VariableRateList);
  }
  
  return (asynStatus)status;
}


// Inefficiently find the index of a given value
int Photron::findListIndex(epicsInt32 value, unsigned long listSize, 
                       unsigned long* listName) {
  int index;
  int retVal = 0;
  
  for (index=0; index<(int)listSize; index++) {
    if (value == listName[index]) {
      retVal = index;
      break;
    }
  }
  
  return retVal;
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
  int index;
  int eVal, eStatus;
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
  eStatus = statusToEPICS(this->nStatus);
  setIntegerParam(PhotronStatusName, eStatus);
  
  nRet = PDC_GetCamMode(this->nDeviceNo, this->nChildNo, &(this->camMode), &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetCamMode failed %d\n", nErrorCode);
    return asynError;
  }
  status |= setIntegerParam(PhotronCamMode, this->camMode);
  
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
  
  nRet = PDC_GetShutterSpeedFps(this->nDeviceNo, this->nChildNo, 
                                &(this->shutterSpeedFps), &nErrorCode);
  if (nRet = PDC_FAILED) {
    printf("PDC_GetShutterSpeedFps failed %d\n", nErrorCode);
    return asynError;
  }
  status |= setIntegerParam(PhotronShutterFps, this->shutterSpeedFps);
  
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
  tmode = this->trigModeToEPICS(this->triggerMode);
  
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
  
  if (this->functionList[PDC_EXIST_IRIG] == PDC_EXIST_SUPPORTED) {
    nRet = PDC_GetIRIG(this->nDeviceNo, &(this->IRIG), &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_GetIRIG failed %d\n", nErrorCode);
      return asynError;
    }
  } else {
    this->IRIG = 0;
  }
  status |= setIntegerParam(PhotronIRIG, this->IRIG);
  
  //
  if (this->functionList[PDC_EXIST_SYNC_PRIORITY] == PDC_EXIST_SUPPORTED) {
    nRet = PDC_GetSyncPriority(this->nDeviceNo, &(this->syncPriority), &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_GetSyncPriority failed %d\n", nErrorCode);
      return asynError;
    }
  } else {
    this->syncPriority = 0;
  }
  status |= setIntegerParam(PhotronSyncPriority, this->syncPriority);
  
  for (index=0; index<PDC_EXTIO_MAX_PORT; index++) {
    if (index < (int)this->inPorts) {
      nRet = PDC_GetExternalInMode(this->nDeviceNo, index+1, 
                                   &(this->ExtInMode[index]), &nErrorCode);
      if (nRet == PDC_FAILED) {
        printf("PDC_GetExternalInMode failed %d; index=%d\n", nErrorCode, index);
        return asynError;
      }
      eVal = this->inputModeToEPICS(this->ExtInMode[index]);
    } else {
      // This is necessary to avoid weird values for uninitialized mbbi records
      eVal = 0;
    }
    setIntegerParam(*PhotronExtInSig[index], eVal);
  }

  for (index=0; index<PDC_EXTIO_MAX_PORT; index++) {
    if (index < (int)this->outPorts) {
      nRet = PDC_GetExternalOutMode(this->nDeviceNo, index+1, 
                                    &(this->ExtOutMode[index]), &nErrorCode);
      if (nRet == PDC_FAILED) {
        printf("PDC_GetExternalOutMode failed %d; index=%d\n", nErrorCode, index);
        return asynError;
      }
      eVal = this->outputModeToEPICS(this->ExtOutMode[index]);
    } else {
      // This is necessary to avoid weird values for uninitialized mbbi records
      eVal = 0;
    }
    setIntegerParam(*PhotronExtOutSig[index], eVal);
  }
  
  // Does this ever change?
  nRet = PDC_GetRecordRateList(this->nDeviceNo, this->nChildNo, 
                               &(this->RateListSize), 
                               this->RateList, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetRecordRateList failed %d\n", nErrorCode);
    return asynError;
  }
  
  // Does this ever change?
  nRet = PDC_GetVariableRecordRateList(this->nDeviceNo, this->nChildNo, 
                               &(this->VariableRateListSize), 
                               this->VariableRateList, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetVariableRecordRateList failed %d\n", nErrorCode);
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
  
  nRet = PDC_GetShutterSpeedFpsList(this->nDeviceNo, this->nChildNo,
                                    &(this->ShutterSpeedFpsListSize),
                                    this->ShutterSpeedFpsList, &nErrorCode);
  if (nRet = PDC_FAILED) {
    printf("PDC_GetShutterSpeedFpsList failed. error = %d\n", nErrorCode);
    return asynError;
  }
  
  nRet = PDC_GetShadingModeList(this->nDeviceNo, this->nChildNo,
                                &(this->ShadingModeListSize),
                                this->ShadingModeList, &nErrorCode);
  if (nRet = PDC_FAILED) {
    printf("PDC_GetShadingModeList failed. error = %d\n", nErrorCode);
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
  
  //printf("Done reading parameters\n");
  
  if (status)
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: error, status=%d\n", driverName, functionName, status);
  return((asynStatus)status);
}


asynStatus Photron::readVariableInfo() {
  unsigned long nRet;
  unsigned long nErrorCode;
  int status = asynSuccess;
  unsigned long wStep, hStep, xPosStep, yPosStep, wMin, hMin, freePos;
  int channel;
  unsigned long rate, width, height, xPos, yPos;
  unsigned long ch;
  static const char *functionName = "readVariableInfo";  
  
  nRet = PDC_GetVariableRestriction(this->nDeviceNo, &wStep, &hStep, &xPosStep,
                                    &yPosStep, &wMin, &hMin, &freePos,
                                    &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetVariableRestriction failed. Error %d\n", nErrorCode);
    return asynError;
  } 
  printf("\nVariable restrictions:\n");
  printf("\tWidth Step: %d\n", wStep);
  printf("\tHeight Step: %d\n", hStep);
  printf("\tX Pos Step: %d\n", xPosStep);
  printf("\tY Pos Step: %d\n", yPosStep);
  printf("\tMin Width: %d\n", wMin);
  printf("\tMin Height: %d\n", hMin);
  printf("\tFree Pos: %d\n", freePos);
  
  setIntegerParam(PhotronVarChanWStep, wStep);
  setIntegerParam(PhotronVarChanHStep, hStep);
  setIntegerParam(PhotronVarChanXPosStep, xPosStep);
  setIntegerParam(PhotronVarChanYPosStep, yPosStep);
  setIntegerParam(PhotronVarChanWMin, wMin);
  setIntegerParam(PhotronVarChanHMin, hMin);
  setIntegerParam(PhotronVarChanFreePos, freePos);
  
  printf("\nChannel\tRate\tWidth\tHeight\tXPos\tYPos\n");
  for (channel = 1; channel <= PDC_VARIABLE_NUM; channel++) {
    nRet = PDC_GetVariableChannelInfo(this->nDeviceNo, channel, &rate, &width,
                                      &height, &xPos, &yPos, &nErrorCode);
    if (nRet == PDC_FAILED) {
      printf("PDC_GetVariableChannelInfo failed. Error %d\n", nErrorCode);
      return asynError;
    }
    
    printf("%d\t%d\t%d\t%d\t%d\t%d\n", channel, rate, width, height, xPos, yPos);
  }
  
  nRet = PDC_GetVariableChannel(this->nDeviceNo, this->nChildNo, &ch, &nErrorCode);
  if (nRet == PDC_FAILED) {
    printf("PDC_GetVariableChannel failed. Error %d\n", nErrorCode);
  } else {
    // In Default mode, ch is 0
    printf("ch = %d\n", ch);
  }
  
  return asynSuccess;
}


asynStatus Photron::parseResolutionList() {
  int index;
  unsigned long width, height, value;
  
  printf("  Available resolutions for rate=%d:\n", this->nRate);
  for (index=0; index<(int)this->ResolutionListSize; index++) {
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
  for (index=0; index<(int)this->ValidHeightListSize; index++) {
    printf("\t%d\n", this->ValidHeightList[index]);
  }
  
  printf("\n  Valid widths for rate=%d and height=%d\n", this->nRate, this->height);
  for (index=0; index<(int)this->ValidWidthListSize; index++) {
    printf("\t%d\n", this->ValidWidthList[index]);
  }
}


void Photron::printTrigModes() {
  int index;
  int mode;
  
  printf("\n  Trigger Modes:\n");
  for (index=0; index<(int)this->TriggerModeListSize; index++) {
    mode = this->TriggerModeList[index] >> 24;
    if (mode == 8) {
      printf("\t%d:\t%d", index, mode);
      printf("\t%d\n", (this->TriggerModeList[index] & 0xF));
    } else {
      printf("\t%d:\t%d\n", index, mode);
    }
    
  }
}


void Photron::printShutterSpeeds() {
  int index;
  
  printf("\n  Shutter Speeds (FPS):\n");
  for (index=0; index<(int)this->ShutterSpeedFpsListSize; index++) {
    printf("\t%d:\t%d\n", index, ShutterSpeedFpsList[index]);
  }
}


void Photron::printShadingModes() {
  int index;
  
  printf("\n  Shading Modes:\n");
  for (index=0; index<(int)this->ShadingModeListSize; index++) {
    printf("\t%d:\t%d\n", index, ShadingModeList[index]);
  }
}


/** Report status of the driver.
  * Prints details about the driver if details>0.
  * It then calls the ADDriver::report() method.
  * \param[in] fp File pointed passed by caller where the output is written to.
  * \param[in] details If >0 then driver details are printed.
  */
void Photron::report(FILE *fp, int details) {
  int index, jndex;

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
    fprintf(fp, "  In ports:          %d\n",  (int)this->inPorts);
    fprintf(fp, "  Out ports:         %d\n",  (int)this->outPorts);
    fprintf(fp, "\n");
    fprintf(fp, "  Width:             %d\n",  (int)this->width);
    fprintf(fp, "  Height:            %d\n",  (int)this->height);
    fprintf(fp, "  Resolution Index:  %d\n",  this->resolutionIndex);
    fprintf(fp, "  Camera Status:     %d\n",  (int)this->nStatus);
    fprintf(fp, "  Max Frames:        %d\n",  (int)this->nMaxFrames);
    fprintf(fp, "  Record Rate:       %d\n",  (int)this->nRate);
    fprintf(fp, "  Bit Depth:         %d\n",  (int)this->bitDepth);
    fprintf(fp, "\n");
    fprintf(fp, "  Trigger mode:      %x\n",  (int)this->triggerMode);
    fprintf(fp, "    A Frames:        %d\n",  (int)this->trigAFrames);
    fprintf(fp, "    R Frames:        %d\n",  (int)this->trigRFrames);
    fprintf(fp, "    R Count:         %d\n",  (int)this->trigRCount);
    fprintf(fp, "  IRIG:              %d\n",  (int)this->IRIG);
  }
  
  if (details > 4) {
    fprintf(fp, "  Available functions:\n");
    for( index=2; index<98; index++) {
      fprintf(fp, "    %d:         %d\n", index, this->functionList[index]);
    }
  }
  
  if (details > 2) {
    fprintf(fp, "\n  Available recording rates:\n");
    for (index=0; index<(int)this->RateListSize; index++) {
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
    
    //
    printShutterSpeeds();
    
    //
    printShadingModes();
  }
  
  if (details > 6) {
    
    fprintf(fp, "\n  External Inputs\n");
    for (index=0; index<(int)this->inPorts; index++) {
      fprintf(fp, "    Port %d (%d modes)\n", index+1, this->ExtInModeListSize[index]);
      for (jndex=0; jndex<(int)this->ExtInModeListSize[index]; jndex++) {
        fprintf(fp, "\t%d:\t0x%02x\n", jndex, this->ExtInModeList[index][jndex]);
      }
    }
    
    fprintf(fp, "\n  External Outputs\n");
    for (index=0; index<(int)this->outPorts; index++) {
      fprintf(fp, "    Port %d (%d modes)\n", index+1, this->ExtOutModeListSize[index]);
      for (jndex=0; jndex<(int)this->ExtOutModeListSize[index]; jndex++) {
        fprintf(fp, "\t%d:\t0x%02x\n", jndex, this->ExtOutModeList[index][jndex]);
      }
    }
    
    if (this->functionList[PDC_EXIST_SYNC_PRIORITY] == PDC_EXIST_SUPPORTED) {
      fprintf(fp, "\n  Sync Priority List:\n");
      for (index=0; index<(int)this->SyncPriorityListSize; index++) {
        fprintf(fp, "\t%d\t%02x\n", index, this->SyncPriorityList[index]);\
      }
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
