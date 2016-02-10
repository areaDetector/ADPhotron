#include <epicsEvent.h>
#include "ADDriver.h"

#include "SDK/Include/PDCLIB.h"

#define NUM_TRIGGER_MODES 14
#define NUM_INPUT_MODES 17
#define NUM_OUTPUT_MODES 35
#define NUM_SHADING_MODES 7
#define MAX_ENUM_STRING_SIZE 26
#define NUM_VAR_CHANS 20

typedef struct {
  int value;
  char string[MAX_ENUM_STRING_SIZE];
} enumStruct_t;

static const char *triggerModeStrings[NUM_TRIGGER_MODES] = {
  "Start",
  "Center",
  "End",
  "Random",
  "Manual",
  "Random reset",
  "Random center",
  "Random manual",
  "Two-stage 1/2",
  "Two-stage 1/4",
  "Two-stage 1/8",
  "Reset",
  "Recon cmd",
  "Random loop"
};

static const char *inputModeStrings[NUM_INPUT_MODES] = {
  "None",
  "On Cam Pos",
  "On Cam Neg",
  "On Others Pos",
  "On Others Neg",
  "Event Pos",
  "Event Neg",
  "Trig Pos",
  "Trig Neg",
  "Ready Pos",
  "Ready Neg",
  "Sync Pos",
  "Sync Neg",
  "Camsync",
  "Othersync",
  "Encoder Pos",
  "Encoder Neg"
};

static const char *outputModeStrings[NUM_OUTPUT_MODES] = {
  "Sync Pos",
  "Sync Neg",
  "Rec Pos",
  "Rec Neg",
  "Trig Pos",
  "Trig Neg",
  "Ready Pos",
  "Ready Neg",
  "IRIG Reset Pos",
  "IRIG Reset Neg",
  "TTL In Thru Pos",
  "TTL In Thru Neg",
  "Expose Pos",
  "Expose Neg",
  "Expose H1 Pos",
  "Expose H1 Neg",
  "Expose H2 Pos",
  "Expose H2 Neg",
  "Expose H3 Pos",
  "Expose H3 Neg",
  "Expose H4 Pos",
  "Expose H4 Neg",
  "Trigger",
  "Rec Pos & Sync Pos",
  "Rec Pos & Exp Pos",
  "Odd Rec Pos & Sync Pos",
  "Even Rec Pos & Sync Pos",
  "Odd Rec Pos",
  "Even Rec Pos",
  "Rec Start",
  "Rec Pos and Exp Neg",
  "Straddling",
  "Encoder Off",
  "Encoder Thru",
  "Encoder Re Timing"
};

static const char *shadingModeStrings[NUM_SHADING_MODES] = {
  "Off",
  "On",
  "Save",
  "Load",
  "Update",
  "Save File",
  "Load File"
};

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
  virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
  virtual asynStatus readEnum(asynUser *pasynUser, char *strings[], 
                              int values[], int severities[], 
                              size_t nElements, size_t *nIn);
  virtual void report(FILE *fp, int details);
  /* PhotronTask should be private, but gets called from C, so must be public */
  void PhotronTask(); 
  void PhotronRecTask(); 
  void PhotronPlayTask(); 
  
  /* These are called from C and so must be public */
  static void shutdown(void *arg);
  
protected:
    int PhotronStatus;
    int PhotronStatusName;
    int PhotronCamMode;
    int PhotronAcquireMode;
    int PhotronOpMode;
    int PhotronMaxFrames;
    int Photron8BitSel;
    int PhotronRecRate;
    int PhotronChangeRecRate;
    int PhotronResIndex;
    int PhotronChangeResIdx;
    int PhotronShutterFps;
    int PhotronChangeShutterFps;
    int PhotronJumpShutterFps;
    int PhotronVarChan;
    int PhotronChangeVarChan;
    int PhotronVarChanRate;
    int PhotronVarChanXSize;
    int PhotronVarChanYSize;
    int PhotronVarChanXPos;
    int PhotronVarChanYPos;
    int PhotronVarChanWStep;
    int PhotronVarChanHStep;
    int PhotronVarChanXPosStep;
    int PhotronVarChanYPosStep;
    int PhotronVarChanWMin;
    int PhotronVarChanHMin;
    int PhotronVarChanFreePos;
    int PhotronVarChanApply;
    int PhotronVarChanErase;
    int PhotronVarEditRate;
    int PhotronVarEditXSize;
    int PhotronVarEditYSize;
    int PhotronVarEditXPos;
    int PhotronVarEditYPos;
    int PhotronVarEditMaxRes;
    int PhotronChangeVarEditRate;
    int PhotronChangeVarEditXSize;
    int PhotronChangeVarEditYSize;
    int PhotronChangeVarEditXPos;
    int PhotronChangeVarEditYPos;
    int PhotronAfterFrames;
    int PhotronRandomFrames;
    int PhotronRecCount;
    int PhotronSoftTrig;
    int PhotronFrameStart;
    int PhotronFrameEnd;
    int PhotronLiveMode;
    int PhotronPreviewMode;
    int PhotronPMStart;
    int PhotronPMEnd;
    int PhotronPMIndex;
    int PhotronChangePMIndex;
    int PhotronPMFirst;
    int PhotronPMLast;
    int PhotronPMPlay;
    int PhotronPMPlayRev;
    int PhotronPMPlayFPS;
    int PhotronPMPlayMult;
    int PhotronPMRepeat;
    int PhotronPMSave;
    int PhotronPMCancel;
    int PhotronIRIG;
    int PhotronMemIRIGDay;
    int PhotronMemIRIGHour;
    int PhotronMemIRIGMin;
    int PhotronMemIRIGSec;
    int PhotronMemIRIGUsec;
    int PhotronMemIRIGSigEx;
    int PhotronSyncPriority;
    int PhotronTest;
    int PhotronExtIn1Sig;
    int PhotronExtIn2Sig;
    int PhotronExtIn3Sig;
    int PhotronExtIn4Sig;
    int PhotronExtOut1Sig;
    int PhotronExtOut2Sig;
    int PhotronExtOut3Sig;
    int PhotronExtOut4Sig;
    int PhotronShadingMode;
    #define FIRST_PHOTRON_PARAM PhotronStatus
    #define LAST_PHOTRON_PARAM PhotronShadingMode
    
    int* PhotronExtInSig[PDC_EXTIO_MAX_PORT];
    int* PhotronExtOutSig[PDC_EXTIO_MAX_PORT];
    
private:
  /* These are the methods that are new to this class */
  asynStatus disconnectCamera();
  asynStatus connectCamera();
  asynStatus getCameraInfo();
  asynStatus updateResolution();
  asynStatus setValidWidth(epicsInt32 value);
  asynStatus setValidHeight(epicsInt32 value);
  asynStatus setResolution(epicsInt32 value);
  asynStatus changeResIndex(epicsInt32 value);
  asynStatus setGeometry();
  asynStatus getGeometry();
  asynStatus readParameters();
  asynStatus readVariableInfo();
  asynStatus readImage();
  asynStatus readMemImage(epicsInt32 value);
  asynStatus readImageRange();
  asynStatus setTransferOption();
  asynStatus setRecordRate(epicsInt32 value, epicsInt32 flag);
  asynStatus changeRecordRate(epicsInt32 value);
  asynStatus setVariableRecordRate(epicsInt32 value);
  asynStatus changeVariableRecordRate(epicsInt32 value);
  asynStatus setVariableXSize(epicsInt32 value);
  asynStatus setVariableYSize(epicsInt32 value);
  asynStatus setVariableXPos(epicsInt32 value);
  asynStatus setVariableYPos(epicsInt32 value);
  asynStatus setVariableMaxRes();
  asynStatus changeVariableXSize(epicsInt32 value);
  asynStatus changeVariableYSize(epicsInt32 value);
  asynStatus changeVariableXPos(epicsInt32 value);
  asynStatus changeVariableYPos(epicsInt32 value);
  // IAMHERE
  asynStatus setShutterSpeedFps(epicsInt32 value);
  asynStatus changeShutterSpeedFps(epicsInt32 value);
  asynStatus jumpShutterSpeedFps(epicsInt32 value);
  asynStatus readVariableChannelInfo();
  asynStatus setVariableChannel(epicsInt32 value);
  asynStatus changeVariableChannel(epicsInt32 value);
  asynStatus applyVariableChannel();
  asynStatus eraseVariableChannel();
  asynStatus setStatus(epicsInt32 value);
  asynStatus parseResolutionList();
  void printResOptions();
  void printTrigModes();
  void printShutterSpeeds();
  void printShadingModes();
  asynStatus setPixelFormat();
  asynStatus setTriggerMode();
  asynStatus softwareTrigger();
  asynStatus setRecReady();
  asynStatus setEndless();
  asynStatus setLive();
  asynStatus setPlayback();
  asynStatus setPMIndex(epicsInt32 value);
  asynStatus changePMIndex(epicsInt32 value);
  asynStatus setPreviewRange(epicsInt32 function, epicsInt32 value);
  asynStatus readMem();
  asynStatus setIRIG(epicsInt32 value);
  asynStatus setSyncPriority(epicsInt32 value);
  asynStatus setExternalInMode(epicsInt32 port, epicsInt32 value);
  asynStatus setExternalOutMode(epicsInt32 port, epicsInt32 value);
  asynStatus setShadingMode(epicsInt32 value);
  int statusToEPICS(int apiStatus);
  int trigModeToEPICS(int apiMode);
  int trigModeToAPI(int mode);
  int shadingModeToEPICS(int apiMode);
  int shadingModeToAPI(int mode);
  int inputModeToEPICS(int apiMode);
  int inputModeToAPI(int mode);
  int outputModeToEPICS(int apiMode);
  int outputModeToAPI(int mode);
  asynStatus createStaticEnums();
  asynStatus createDynamicEnums();
  void timeDiff(PPDC_IRIG_INFO time1, PPDC_IRIG_INFO time2, PPDC_IRIG_INFO timeDiff);
  void timeDataToSec(PPDC_IRIG_INFO tData, double *seconds);
  asynStatus findNearestValue(epicsInt32* pValue, int* pListIndex, unsigned long listSize, unsigned long* listName);
  int changeListIndex(epicsInt32 value, unsigned long listIndex, unsigned long listSize);
  int findListIndex(epicsInt32 value, unsigned long listSize, unsigned long* listName);
  
  /* These items are specific to the Photron driver */
  // constructor
  char *cameraId;                /* This can be an IP name, or IP address */
  int autoDetect;
  epicsEventId startEventId;
  epicsEventId stopEventId;
  epicsEventId startRecEventId;
  epicsEventId stopRecEventId;
  epicsEventId resumeRecEventId;
  epicsEventId startPlayEventId;
  epicsEventId stopPlayEventId;
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
  unsigned long inPorts;
  unsigned long outPorts;
  unsigned long ExtInMode[PDC_EXTIO_MAX_PORT];
  unsigned long ExtInModeListSize[PDC_EXTIO_MAX_PORT];
  unsigned long ExtInModeList[PDC_EXTIO_MAX_PORT][PDC_MAX_LIST_NUMBER];
  unsigned long ExtOutMode[PDC_EXTIO_MAX_PORT];
  unsigned long ExtOutModeListSize[PDC_EXTIO_MAX_PORT];
  unsigned long ExtOutModeList[PDC_EXTIO_MAX_PORT][PDC_MAX_LIST_NUMBER];
  unsigned long SyncPriorityListSize;
  unsigned long SyncPriorityList[PDC_MAX_LIST_NUMBER];
  // updateResolution
  unsigned long width;
  unsigned long height;
  unsigned long xPos;
  unsigned long yPos;
  unsigned long ValidWidthListSize;
  unsigned long ValidWidthList[PDC_MAX_LIST_NUMBER];
  unsigned long ValidHeightListSize;
  unsigned long ValidHeightList[PDC_MAX_LIST_NUMBER];
  int resolutionIndex;
  // readParameters
  unsigned long nStatus;
  unsigned long camMode;
  unsigned long nMaxFrames;
  unsigned long nBlocks; // total number of current partition blocks
  unsigned long nRate; // units = frames per second
  unsigned long shutterSpeedFps;
  unsigned long triggerMode;
  unsigned long trigAFrames;
  unsigned long trigRFrames;
  unsigned long trigRCount;
  unsigned long shadingMode;
  unsigned long IRIG;
  unsigned long syncPriority;
  unsigned long RateListSize;
  unsigned long RateList[PDC_MAX_LIST_NUMBER];
  int recRateIndex;
  unsigned long VariableRateListSize;
  unsigned long VariableRateList[PDC_MAX_LIST_NUMBER];
  int varRecRateIndex;
  unsigned long ResolutionListSize;
  unsigned long ResolutionList[PDC_MAX_LIST_NUMBER];
  unsigned long TriggerModeListSize;
  unsigned long TriggerModeList[PDC_MAX_LIST_NUMBER];
  int shutterSpeedFpsIndex;
  unsigned long ShutterSpeedFpsListSize;
  unsigned long ShutterSpeedFpsList[PDC_MAX_LIST_NUMBER];
  unsigned long ShadingModeListSize;
  unsigned long ShadingModeList[PDC_MAX_LIST_NUMBER];
  unsigned long pixelBits;
  unsigned long highSpeedMode;
  unsigned long varRate;
  unsigned long varWidth;
  unsigned long varHeight;
  unsigned long varXPos;
  unsigned long varYPos;
  // where should this reside in the list?
  unsigned long bitDepth;
  // Keep track of the desired record rate (for switching back to Default mode)
  int desiredRate;
  // readMem
  epicsInt32 NDArrayCounterBackup;
  unsigned long memWidth;
  unsigned long memHeight;
  unsigned long memRate;
  unsigned long tMode;
  PDC_IRIG_INFO tDataStart;
  PDC_IRIG_INFO tDataEnd;
  PDC_FRAME_INFO FrameInfo;
  //
  epicsTimeStamp preIRIGStartTime;
  epicsTimeStamp postIRIGStartTime;
  int abortFlag;
  //
  int stopFlag;
  int dirFlag;
  //
  int stopRecFlag;
  /* Our data */
  NDArray *pRaw;
  int numValidTriggerModes_;
  int numValidShadingModes_;
  int numValidInputModes_[PDC_EXTIO_MAX_PORT];
  int numValidOutputModes_[PDC_EXTIO_MAX_PORT];
  enumStruct_t triggerModeEnums_[NUM_TRIGGER_MODES];
  enumStruct_t shadingModeEnums_[NUM_SHADING_MODES];
  enumStruct_t inputModeEnums_[PDC_EXTIO_MAX_PORT][NUM_INPUT_MODES];
  enumStruct_t outputModeEnums_[PDC_EXTIO_MAX_PORT][NUM_OUTPUT_MODES];
};

/* Declare this function here so that its implementation can appear below
   the contructor in the source file */ 
static void PhotronTaskC(void *drvPvt);
static void PhotronRecTaskC(void *drvPvt);
static void PhotronPlayTaskC(void *drvPvt);

typedef struct {
  ELLNODE node;
  Photron *pCamera;
} cameraNode;

// Define param strings here
 /*                             String                  interface  access   */
#define PhotronStatusString     "PHOTRON_STATUS"     /* (asynInt32,    rw)   */
#define PhotronStatusNameString  "PHOTRON_STATUS_NAME" /* (asynInt32,   rw)   */
#define PhotronCamModeString    "PHOTRON_CAM_MODE"   /* (asynInt32,    r)   */
#define PhotronAcquireModeString "PHOTRON_ACQUIRE_MODE" /* (asynInt32,    w) */
#define PhotronOpModeString     "PHOTRON_OP_MODE" /* (asynInt32,    w) */
#define PhotronMaxFramesString  "PHOTRON_MAX_FRAMES" /* (asynInt32,    r)   */
#define Photron8BitSelectString "PHOTRON_8_BIT_SEL"  /* (asynInt32,    rw)   */
#define PhotronRecordRateString "PHOTRON_REC_RATE"   /* (asynInt32,    rw)   */
#define PhotronChangeRecRateString "PHOTRON_CHANGE_REC_RATE" /* (asynInt32, w) */
#define PhotronResIndexString   "PHOTRON_RES_INDEX"  /* (asynInt32, rw) */
#define PhotronChangeResIdxString "PHOTRON_CHANGE_RES_IDX" /* (asynInt32, w) */
#define PhotronShutterFpsString   "PHOTRON_SHUTTER_FPS"  /* (asynInt32, rw) */
#define PhotronChangeShutterFpsString "PHOTRON_CHANGE_SHUTTER_FPS" /* (asynInt32, w) */
#define PhotronJumpShutterFpsString "PHOTRON_JUMP_SHUTTER_FPS" /* (asynInt32, w) */
// Var chan selection 
#define PhotronVarChanString "PHOTRON_VAR_CHAN"   /* (asynInt32,    rw)   */
#define PhotronChangeVarChanString "PHOTRON_CHANGE_VAR_CHAN" /* (asynInt32, w) */
#define PhotronVarChanRateString "PHOTRON_VAR_CHAN_RATE" /* (asynInt32,  r) */
#define PhotronVarChanXSizeString "PHOTRON_VAR_CHAN_X_SIZE" /* (asynInt32,  r) */
#define PhotronVarChanYSizeString "PHOTRON_VAR_CHAN_Y_SIZE" /* (asynInt32,  r) */
#define PhotronVarChanXPosString "PHOTRON_VAR_CHAN_X_POS" /* (asynInt32,  r) */
#define PhotronVarChanYPosString "PHOTRON_VAR_CHAN_Y_POS" /* (asynInt32,  r) */
// Var chan limits (fixed for camera)
#define PhotronVarChanWStepString "PHOTRON_VAR_CHAN_W_STEP" /* (asynInt32,  r) */
#define PhotronVarChanHStepString "PHOTRON_VAR_CHAN_H_STEP" /* (asynInt32,  r) */
#define PhotronVarChanXPosStepString "PHOTRON_VAR_CHAN_X_POS_STEP" /* (asynInt32,  r) */
#define PhotronVarChanYPosStepString "PHOTRON_VAR_CHAN_Y_POS_STEP" /* (asynInt32,  r) */
#define PhotronVarChanWMinString  "PHOTRON_VAR_CHAN_W_MIN" /* (asynInt32,  r) */
#define PhotronVarChanHMinString  "PHOTRON_VAR_CHAN_H_MIN" /* (asynInt32,  r) */
#define PhotronVarChanFreePosString "PHOTRON_VAR_CHAN_FREE_POS" /* (asynInt32, r) */
// Var chan editing
#define PhotronVarChanApplyString "PHOTRON_VAR_CHAN_APPLY" /* (asynInt32, w) */
#define PhotronVarChanEraseString "PHOTRON_VAR_CHAN_ERASE" /* (asynInt32, w) */
#define PhotronVarEditRateString "PHOTRON_VAR_EDIT_RATE" /* (asynInt32,  r) */
#define PhotronVarEditXSizeString "PHOTRON_VAR_EDIT_X_SIZE" /* (asynInt32,  r) */
#define PhotronVarEditYSizeString "PHOTRON_VAR_EDIT_Y_SIZE" /* (asynInt32,  r) */
#define PhotronVarEditXPosString "PHOTRON_VAR_EDIT_X_POS" /* (asynInt32,  r) */
#define PhotronVarEditYPosString "PHOTRON_VAR_EDIT_Y_POS" /* (asynInt32,  r) */
#define PhotronVarEditMaxResString "PHOTRON_VAR_EDIT_MAX_RES" /* (asynInt32,  r) */
#define PhotronChangeVarEditRateString "PHOTRON_CHANGE_VAR_EDIT_RATE" /* (asynInt32, w) */
#define PhotronChangeVarEditXSizeString "PHOTRON_CHANGE_VAR_EDIT_X_SIZE" /* (asynInt32, w) */
#define PhotronChangeVarEditYSizeString "PHOTRON_CHANGE_VAR_EDIT_Y_SIZE" /* (asynInt32, w) */
#define PhotronChangeVarEditXPosString "PHOTRON_CHANGE_VAR_EDIT_X_POS" /* (asynInt32, w) */
#define PhotronChangeVarEditYPosString "PHOTRON_CHANGE_VAR_EDIT_Y_POS" /* (asynInt32, w) */
//
#define PhotronAfterFramesString "PHOTRON_AFTER_FRAMES" /* (asynInt32,    rw) */
#define PhotronRandomFramesString "PHOTRON_RANDOM_FRAMES" /* (asynInt32,  rw) */
#define PhotronRecCountString   "PHOTRON_REC_COUNT"  /* (asynInt32,    rw)   */
#define PhotronSoftTrigString   "PHOTRON_SOFT_TRIG"   /* (asynInt32,    w) */
#define PhotronFrameStartString "PHOTRON_FRAME_START" /* (asynInt32,    r) */
#define PhotronFrameEndString   "PHOTRON_FRAME_END"   /* (asynInt32,    r) */
#define PhotronLiveModeString   "PHOTRON_LIVE_MODE" /* (asynInt32,    w) */
#define PhotronPreviewModeString "PHOTRON_PREVIEW_MODE" /* (asynInt32,    w) */
#define PhotronPMStartString    "PHOTRON_PM_START"  /* (asynInt32,   rw) */
#define PhotronPMEndString      "PHOTRON_PM_END"  /* (asynInt32,   rw) */
#define PhotronPMIndexString    "PHOTRON_PM_INDEX"  /* (asynInt32,   rw) */
#define PhotronChangePMIndexString "PHOTRON_CHANGE_PM_INDEX" /* (asynInt32, rw) */
#define PhotronPMFirstString     "PHOTRON_PM_FIRST"  /* (asynInt32,   rw) */
#define PhotronPMLastString     "PHOTRON_PM_LAST"  /* (asynInt32,   rw) */
#define PhotronPMPlayString     "PHOTRON_PM_PLAY"  /* (asynInt32,   rw) */
#define PhotronPMPlayRevString  "PHOTRON_PM_PLAY_REV"  /* (asynInt32,   rw) */
#define PhotronPMPlayFPSString  "PHOTRON_PM_PLAY_FPS"  /* (asynInt32,   rw) */
#define PhotronPMPlayMultString "PHOTRON_PM_PLAY_MULT"  /* (asynInt32,   rw) */
#define PhotronPMRepeatString   "PHOTRON_PM_REPEAT"  /* (asynInt32,   rw) */
#define PhotronPMSaveString     "PHOTRON_PM_SAVE" /* (asynInt32,    w) */
#define PhotronPMCancelString   "PHOTRON_PM_CANCEL"   /* (asynInt32,    w) */
#define PhotronIRIGString       "PHOTRON_IRIG"   /* (asynInt32,    w) */
#define PhotronMemIRIGDayString "PHOTRON_MEM_IRIG_DAY" /* (asynInt32,    r) */
#define PhotronMemIRIGHourString "PHOTRON_MEM_IRIG_HOUR" /* (asynInt32,    r) */
#define PhotronMemIRIGMinString "PHOTRON_MEM_IRIG_MIN" /* (asynInt32,    r) */
#define PhotronMemIRIGSecString "PHOTRON_MEM_IRIG_SEC" /* (asynInt32,    r) */
#define PhotronMemIRIGUsecString "PHOTRON_MEM_IRIG_USEC" /* (asynInt32,    r) */
#define PhotronMemIRIGSigExString "PHOTRON_MEM_IRIG_SIGEX" /* (asynInt32,  r) */
#define PhotronSyncPriorityString "PHOTRON_SYNC_PRIORITY" /* (asynInt32, rw)  */
#define PhotronTestString       "PHOTRON_TEST"            /* (asynInt32, rw) */
#define PhotronExtIn1SigString  "PHOTRON_EXT_IN_1_SIG" /* (asynInt32, rw)  */
#define PhotronExtIn2SigString  "PHOTRON_EXT_IN_2_SIG" /* (asynInt32, rw)  */
#define PhotronExtIn3SigString  "PHOTRON_EXT_IN_3_SIG" /* (asynInt32, rw)  */
#define PhotronExtIn4SigString  "PHOTRON_EXT_IN_4_SIG" /* (asynInt32, rw)  */
#define PhotronExtOut1SigString  "PHOTRON_EXT_OUT_1_SIG" /* (asynInt32, rw)  */
#define PhotronExtOut2SigString  "PHOTRON_EXT_OUT_2_SIG" /* (asynInt32, rw)  */
#define PhotronExtOut3SigString  "PHOTRON_EXT_OUT_3_SIG" /* (asynInt32, rw)  */
#define PhotronExtOut4SigString  "PHOTRON_EXT_OUT_4_SIG" /* (asynInt32, rw)  */
#define PhotronShadingModeString "PHOTRON_SHADING_MODE" /* (asynInt32, rw)  */

#define NUM_PHOTRON_PARAMS ((int)(&LAST_PHOTRON_PARAM-&FIRST_PHOTRON_PARAM+1))
