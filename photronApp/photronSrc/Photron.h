#include <epicsEvent.h>
#include "ADDriver.h"

#include "SDK/Include/PDCLIB.h"

/** Photron driver */
class epicsShareClass Photron : public ADDriver {
public:
  /* Constructor and Destructor */
  Photron(const char *portName, const char *ipAddress, int autoDetect,
          int maxBuffers, size_t maxMemory, int priority, int stackSize);
  ~Photron();

  /* These methods are overwritten from asynPortDriver */
  virtual asynStatus connect(asynUser* pasynUser);
  virtual asynStatus disconnect(asynUser* pasynUser);

  /* These are the methods that we override from ADDriver */
  virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
  virtual void report(FILE *fp, int details);
  /* PhotronTask should be private, but gets called from C, so must be public */
  void PhotronTask(); 
  void PhotronRecTask(); 
  
  /* These are called from C and so must be public */
  static void shutdown(void *arg);
  
protected:
    int PhotronStatus;
    int PhotronAcquireMode;
    int PhotronMaxFrames;
    int Photron8BitSel;
    int PhotronRecRate;
    int PhotronAfterFrames;
    int PhotronRandomFrames;
    int PhotronRecCount;
    int PhotronSoftTrig;
    int PhotronRecReady;
    int PhotronEndless;
    int PhotronLive;
    int PhotronPlayback;
    int PhotronReadMem;
    int PhotronIRIG;
    int PhotronMemIRIGDay;
    int PhotronMemIRIGHour;
    int PhotronMemIRIGMin;
    int PhotronMemIRIGSec;
    int PhotronMemIRIGUsec;
    int PhotronMemIRIGSigEx;
    #define FIRST_PHOTRON_PARAM PhotronStatus
    #define LAST_PHOTRON_PARAM PhotronMemIRIGSigEx

private:
  /* These are the methods that are new to this class */
  asynStatus disconnectCamera();
  asynStatus connectCamera();
  asynStatus getCameraInfo();
  asynStatus updateResolution();
  asynStatus setValidWidth(epicsInt32 value);
  asynStatus setValidHeight(epicsInt32 value);
  asynStatus setGeometry();
  asynStatus getGeometry();
  asynStatus readParameters();
  asynStatus readImage();
  asynStatus setTransferOption();
  asynStatus setRecordRate(epicsInt32 value);
  asynStatus setStatus(epicsInt32 value);
  asynStatus parseResolutionList();
  void printResOptions();
  void printTrigModes();
  asynStatus setPixelFormat();
  asynStatus setTriggerMode();
  asynStatus softwareTrigger();
  asynStatus setRecReady();
  asynStatus setEndless();
  asynStatus setLive();
  asynStatus setPlayback();
  asynStatus readMem();
  asynStatus setIRIG(epicsInt32 value);
  
  /* These items are specific to the Photron driver */
  // constructor
  char *cameraId;                /* This can be an IP name, or IP address */
  int autoDetect;
  epicsEventId startEventId;
  epicsEventId stopEventId;
  epicsEventId startRecEventId;
  epicsEventId stopRecEventId;
  // connectCamera
  unsigned long nDeviceNo;
  unsigned long nChildNo;   // hard-coded to 1 in connectCamera
  // getCameraInfo
  char functionList[98];   /* Indices (functions) range from 2 to 97 */
  unsigned long deviceCode;
  TCHAR deviceName[PDC_MAX_STRING_LENGTH];
  unsigned long deviceID;
  unsigned long productID;
  unsigned long lotID;
  unsigned long individualID;
  unsigned long version;   /* version number is 1/100 of the retrieved value */
  unsigned long maxChildDevCount;
  unsigned long childDevCount;
  unsigned long sensorWidth;
  unsigned long sensorHeight;
  unsigned long sensorBits;
  /* bitDepth should move to readParameters in the future */
  unsigned long bitDepth;
  // updateResolution
  unsigned long width;
  unsigned long height;
  unsigned long ValidWidthListSize;
  unsigned long ValidWidthList[PDC_MAX_LIST_NUMBER];
  unsigned long ValidHeightListSize;
  unsigned long ValidHeightList[PDC_MAX_LIST_NUMBER];
  // readParameters
  unsigned long nStatus; // replace with PV?
  unsigned long nMaxFrames;
  unsigned long nBlocks; // total number of current partition blocks
  unsigned long nRate; // units = frames per second
  unsigned long triggerMode;
  unsigned long trigAFrames;
  unsigned long trigRFrames;
  unsigned long trigRCount;
  unsigned long IRIG;
  unsigned long RateListSize;
  unsigned long RateList[PDC_MAX_LIST_NUMBER];
  unsigned long ResolutionListSize;
  unsigned long ResolutionList[PDC_MAX_LIST_NUMBER];
  unsigned long TriggerModeListSize;
  unsigned long TriggerModeList[PDC_MAX_LIST_NUMBER];
  unsigned long pixelBits;
  unsigned long highSpeedMode;
  //
  epicsTimeStamp preIRIGStartTime;
  epicsTimeStamp postIRIGStartTime;
  /* Our data */
  NDArray *pRaw;
};

/* Declare this function here so that its implementation can appear below
   the contructor in the source file */ 
static void PhotronTaskC(void *drvPvt);
static void PhotronRecTaskC(void *drvPvt);

typedef struct {
  ELLNODE node;
  Photron *pCamera;
} cameraNode;

// Define param strings here
 /*                             String                  interface  access   */
#define PhotronStatusString     "PHOTRON_STATUS"     /* (asynInt32,    rw)   */
#define PhotronAcquireModeString "PHOTRON_ACQUIRE_MODE" /* (asynInt32,    w) */
#define PhotronMaxFramesString  "PHOTRON_MAX_FRAMES" /* (asynInt32,    r)   */
#define Photron8BitSelectString "PHOTRON_8_BIT_SEL"  /* (asynInt32,    rw)   */
#define PhotronRecordRateString "PHOTRON_REC_RATE"   /* (asynInt32,    rw)   */
#define PhotronAfterFramesString "PHOTRON_AFTER_FRAMES" /* (asynInt32,    rw) */
#define PhotronRandomFramesString "PHOTRON_RANDOM_FRAMES" /* (asynInt32,  rw) */
#define PhotronRecCountString   "PHOTRON_REC_COUNT"  /* (asynInt32,    rw)   */
#define PhotronSoftTrigString   "PHOTRON_SOFT_TRIG"   /* (asynInt32,    w) */
#define PhotronRecReadyString   "PHOTRON_REC_READY"  /* (asynInt32,    w) */
#define PhotronEndlessString    "PHOTRON_ENDLESS"  /* (asynInt32,    w) */
#define PhotronLiveString       "PHOTRON_LIVE"  /* (asynInt32,    w) */
#define PhotronPlaybackString   "PHOTRON_PLAYBACK"  /* (asynInt32,    w) */
#define PhotronReadMemString    "PHOTRON_READ_MEM"   /* (asynInt32,    w) */
#define PhotronIRIGString       "PHOTRON_IRIG"   /* (asynInt32,    w) */
#define PhotronMemIRIGDayString "PHOTRON_MEM_IRIG_DAY" /* (asynInt32,    r) */
#define PhotronMemIRIGHourString "PHOTRON_MEM_IRIG_HOUR" /* (asynInt32,    r) */
#define PhotronMemIRIGMinString "PHOTRON_MEM_IRIG_MIN" /* (asynInt32,    r) */
#define PhotronMemIRIGSecString "PHOTRON_MEM_IRIG_SEC" /* (asynInt32,    r) */
#define PhotronMemIRIGUsecString "PHOTRON_MEM_IRIG_USEC" /* (asynInt32,    r) */
#define PhotronMemIRIGSigExString "PHOTRON_MEM_IRIG_SIGEX" /* (asynInt32,  r) */

#define NUM_PHOTRON_PARAMS ((int)(&LAST_PHOTRON_PARAM-&FIRST_PHOTRON_PARAM+1))
