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

/** Main driver class inherited from areaDetector's ADDriver class.
 * One instance of this class will control one camera.
 */
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
  void PhotronWaitTask(); 
  void PhotronRecTask(); 
  void PhotronPlayTask(); 
  
  /* These are called from C and so must be public */
  static void shutdown(void *arg);
  
protected:
    int PhotronStatus;          /** Camera status                             (int32 read) */
    int PhotronStatusName;      /** Camera status string                      (int32 read) */
    int PhotronCamMode;         /** Camera mode                               (int32 read/write) */
    int PhotronSyncPulse;       /** Preferred ext sync polarity               (int32 write) */
    int PhotronAcquireMode;     /** Acquire mode                              (int32 write) */
    int PhotronMaxFrames;       /** Max frames that can be saved in memory
                                    at the current resolution                 (int32 read) */
    int Photron8BitSel;         /** 8-bit offset. Only applies when 
                                    data type is UInt8                        (int32 read/write) */
    int PhotronRecRate;         /** Record rate. Units are frames per second  (int32 read/write) */
    int PhotronChangeRecRate;   /** Increases or decreases the record rate    (int32 write) */
    int PhotronResIndex;        /** Index of the current resolution           (int32 read/write) */
    int PhotronChangeResIdx;    /** Increases or decreases the res index      (int32 write) */
    int PhotronShutterFps;      /** The shutter speed in Hz                   (int32 read/write) */
    int PhotronChangeShutterFps;/** Increases or decreases the shutter speed  (int32 write) */
    int PhotronJumpShutterFps;  /** Jumps to the max or min shutter speed     (int32 write) */
    int PhotronVarChan;         /** Current variable channel (1-20)           (int32 read/write) */
    int PhotronChangeVarChan;   /** Increases or decreases the variable chan  (int32 write) */
    int PhotronVarChanRate;     /** Record rate for the current variable chan (int32 read) */
    int PhotronVarChanXSize;    /** ROI width for the current variable chan   (int32 read) */
    int PhotronVarChanYSize;    /** ROI height for the current variable chan  (int32 read) */
    int PhotronVarChanXPos;     /** ROI hoirzontal position for the current 
                                    variable channel                          (int32 read) */
    int PhotronVarChanYPos;     /** ROI vertical position for the current 
                                    variable channel                          (int32 read) */
    int PhotronVarChanWStep;    /** Distance between valid widths for
                                    variable-channel editing                  (int32 read) */
    int PhotronVarChanHStep;    /** Distance between valid heights for
                                    variable-channel editing                  (int32 read) */
    int PhotronVarChanXPosStep; /** Distance between valid horizontal
                                    positions for variable-channel editing    (int32 read) */
    int PhotronVarChanYPosStep; /** Distance between valid vertical
                                    positions for variable-channel editing    (int32 read) */
    int PhotronVarChanWMin;     /** Minimum width allowed for 
                                    variable-channel editing                  (int32 read) */
    int PhotronVarChanHMin;     /** Minimum height allowed for
                                    variable-channel editing                  (int32 read) */
    int PhotronVarChanFreePos;  /** Flag indicating dimensions allowing
                                    positions to be changed in var-chan edit  (int32 read) */
    int PhotronVarChanApply;    /** Save desired settings to the current
                                    variable channel                          (int32 write) */
    int PhotronVarChanErase;    /** Erase settings of current variable chan   (int32 write) */
    int PhotronVarEditRate;     /** Desired record rate for current var chan  (int32 read/write) */
    int PhotronVarEditXSize;    /** Desired ROI width for current var chan    (int32 read/write) */
    int PhotronVarEditYSize;    /** Desired ROI height for current var chan   (int32 read/write) */
    int PhotronVarEditXPos;     /** Desired ROI horizontal position for the 
                                    current variable channnel                 (int32 read/write) */
    int PhotronVarEditYPos;     /** Desired ROI vertical position for the
                                    current variable channel                  (int32 read/write) */
    int PhotronVarEditMaxRes;   /** Set desired height and width to the largest
                                    square allowed given the desired record rate
                                    for the current variable channel          (int32 read/write) */
    int PhotronChangeVarEditRate;   /** Increases or decreases the desired record
                                        rate for the current variable channel     (int32 write) */
    int PhotronChangeVarEditXSize;  /** Increases or decreases the desired width
                                        of the current variable channel by the
                                        value of PhotronVarChanWStep              (int32 write) */
    int PhotronChangeVarEditYSize;  /** Increases or decreases the desired height
                                        of the current variable channel by the
                                        value of PhotronVarChanHStep              (int32 write) */
    int PhotronChangeVarEditXPos;   /** Increases or decreases the desired horizontal
                                        position of the current variable channel 
                                        by the value of PhotronVarChanXPosStep    (int32 write) */
    int PhotronChangeVarEditYPos;   /** Increases or decreases the desired vertical
                                        position of the current variable channel
                                        by the value of PhotronVarChanYPosStep    (int32 write) */
    int PhotronAfterFrames;     /** Number of frames to record after a 
                                    trigger is received                       (int32 read/write) */
    int PhotronRandomFrames;    /** Total number of frames to record          (int32 read/write) */
    int PhotronRecCount;        /** Number of recordings before camera
                                    automatically returns to live mode        (int32 read/write) */
    int PhotronSoftTrig;        /** Initiates a recording when AcquireMode is 
                                    Record. Ignored when AcquireMode is Live  (int32 write) */
    int PhotronFrameStart;      /** Index of the first frame of a recording   (int32 read) */
    int PhotronFrameEnd;        /** Index of the last frame of a recording    (int32 read) */
    int PhotronLiveMode;        /** Initiates early playback for trigger modes 
                                    with multiple recordings                  (int32 write) */
    int PhotronPreviewMode;
    int PhotronPMStart;         /** Index of first frame of the preview range (int32 read/write) */
    int PhotronPMEnd;           /** Index of last frame of the preview range  (int32 read/write) */
    int PhotronPMIndex;         /** Index of the current frame                (int32 read/write) */
    int PhotronChangePMIndex;   /** Increases or decreases the index by 1     (int32 write) */
    int PhotronPMFirst;         /** Jump to the first frame of preview range  (int32 write) */
    int PhotronPMLast;          /** Jump to the last frame of preview range   (int32 write) */
    int PhotronPMPlay;          /** Start or stop playing the preview range   (int32 write) */
    int PhotronPMPlayRev;       /** Start or stop playing the preview range
                                    in reverse                                (int32 write) */
    int PhotronPMSave;          /** Save the images in the preview range      (int32 write) */
    int PhotronPMCancel;        /** Exit preview mode without saving images   (int32 write) */
    int PhotronPMPlayFPS;       /** Updates per second during playback        (int32 read/write) */
    int PhotronPMPlayMult;      /** Images per update during playback         (int32 read/write) */
    int PhotronPMRepeat;        /** Enable or disable repeat during playback  (int32 write) */
    int PhotronIRIG;            /** Enable or disable the use of IRIG 
                                    timecodes when AcquireMode is Record      (int32 read/write) */
    int PhotronMemIRIGDay;      /** Day of year from timecode for current frame (int32 read) */
    int PhotronMemIRIGHour;     /** Hour from timecode for current frame      (int32 read) */
    int PhotronMemIRIGMin;      /** Minute from timecode for current frame    (int32 read) */
    int PhotronMemIRIGSec;      /** Second from timecode for current frame    (int32 read) */
    int PhotronMemIRIGUsec;     /** Microsecond from timecode for current frame (int32 read) */
    int PhotronMemIRIGSigEx;    /** Signal-exist flag for current frame       (int32 read) */
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
    int PhotronBurstTrans;
    #define FIRST_PHOTRON_PARAM PhotronStatus
    #define LAST_PHOTRON_PARAM PhotronBurstTrans
    
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
  asynStatus testMethod();
  asynStatus setPMIndex(epicsInt32 value);
  asynStatus changePMIndex(epicsInt32 value);
  asynStatus setPreviewRange(epicsInt32 function, epicsInt32 value);
  asynStatus readMem();
  asynStatus setIRIG(epicsInt32 value);
  asynStatus setSyncPriority(epicsInt32 value);
  asynStatus setExternalInMode(epicsInt32 port, epicsInt32 value);
  asynStatus setExternalOutMode(epicsInt32 port, epicsInt32 value);
  asynStatus setShadingMode(epicsInt32 value);
  asynStatus setBurstTransfer(epicsInt32 value);
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
  epicsEventId startWaitEventId;
  epicsEventId stopWaitEventId;
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
  unsigned long burstTransfer;
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
  int previewDone;
  //
  int forceWait;
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
static void PhotronWaitTaskC(void *drvPvt);
static void PhotronRecTaskC(void *drvPvt);
static void PhotronPlayTaskC(void *drvPvt);

typedef struct {
  ELLNODE node;
  Photron *pCamera;
} cameraNode;

// Define param strings here
#define PhotronStatusString           "PHOTRON_STATUS"
#define PhotronStatusNameString       "PHOTRON_STATUS_NAME"
#define PhotronCamModeString          "PHOTRON_CAM_MODE"
#define PhotronSyncPulseString        "PHOTRON_SYNC_PULSE"
#define PhotronAcquireModeString      "PHOTRON_ACQUIRE_MODE"
#define PhotronMaxFramesString        "PHOTRON_MAX_FRAMES"
#define Photron8BitSelectString       "PHOTRON_8_BIT_SEL"
#define PhotronRecordRateString       "PHOTRON_REC_RATE"
#define PhotronChangeRecRateString    "PHOTRON_CHANGE_REC_RATE"
#define PhotronResIndexString         "PHOTRON_RES_INDEX"
#define PhotronChangeResIdxString     "PHOTRON_CHANGE_RES_IDX"
#define PhotronShutterFpsString       "PHOTRON_SHUTTER_FPS"
#define PhotronChangeShutterFpsString "PHOTRON_CHANGE_SHUTTER_FPS"
#define PhotronJumpShutterFpsString   "PHOTRON_JUMP_SHUTTER_FPS"
// Var chan selection 
#define PhotronVarChanString          "PHOTRON_VAR_CHAN"
#define PhotronChangeVarChanString    "PHOTRON_CHANGE_VAR_CHAN"
#define PhotronVarChanRateString      "PHOTRON_VAR_CHAN_RATE"
#define PhotronVarChanXSizeString     "PHOTRON_VAR_CHAN_X_SIZE"
#define PhotronVarChanYSizeString     "PHOTRON_VAR_CHAN_Y_SIZE"
#define PhotronVarChanXPosString      "PHOTRON_VAR_CHAN_X_POS"
#define PhotronVarChanYPosString      "PHOTRON_VAR_CHAN_Y_POS"
// Var chan limits (fixed for camera)
#define PhotronVarChanWStepString     "PHOTRON_VAR_CHAN_W_STEP"
#define PhotronVarChanHStepString     "PHOTRON_VAR_CHAN_H_STEP"
#define PhotronVarChanXPosStepString  "PHOTRON_VAR_CHAN_X_POS_STEP"
#define PhotronVarChanYPosStepString  "PHOTRON_VAR_CHAN_Y_POS_STEP"
#define PhotronVarChanWMinString      "PHOTRON_VAR_CHAN_W_MIN"
#define PhotronVarChanHMinString      "PHOTRON_VAR_CHAN_H_MIN"
#define PhotronVarChanFreePosString   "PHOTRON_VAR_CHAN_FREE_POS"
// Var chan editing
#define PhotronVarChanApplyString     "PHOTRON_VAR_CHAN_APPLY"
#define PhotronVarChanEraseString     "PHOTRON_VAR_CHAN_ERASE"
#define PhotronVarEditRateString      "PHOTRON_VAR_EDIT_RATE"
#define PhotronVarEditXSizeString     "PHOTRON_VAR_EDIT_X_SIZE"
#define PhotronVarEditYSizeString     "PHOTRON_VAR_EDIT_Y_SIZE"
#define PhotronVarEditXPosString      "PHOTRON_VAR_EDIT_X_POS"
#define PhotronVarEditYPosString      "PHOTRON_VAR_EDIT_Y_POS"
#define PhotronVarEditMaxResString    "PHOTRON_VAR_EDIT_MAX_RES"
#define PhotronChangeVarEditRateString  "PHOTRON_CHANGE_VAR_EDIT_RATE"
#define PhotronChangeVarEditXSizeString "PHOTRON_CHANGE_VAR_EDIT_X_SIZE"
#define PhotronChangeVarEditYSizeString "PHOTRON_CHANGE_VAR_EDIT_Y_SIZE"
#define PhotronChangeVarEditXPosString  "PHOTRON_CHANGE_VAR_EDIT_X_POS"
#define PhotronChangeVarEditYPosString  "PHOTRON_CHANGE_VAR_EDIT_Y_POS"
//
#define PhotronAfterFramesString      "PHOTRON_AFTER_FRAMES"
#define PhotronRandomFramesString     "PHOTRON_RANDOM_FRAMES"
#define PhotronRecCountString         "PHOTRON_REC_COUNT"
#define PhotronSoftTrigString         "PHOTRON_SOFT_TRIG"
#define PhotronFrameStartString       "PHOTRON_FRAME_START"
#define PhotronFrameEndString         "PHOTRON_FRAME_END"
#define PhotronLiveModeString         "PHOTRON_LIVE_MODE"
#define PhotronPreviewModeString "PHOTRON_PREVIEW_MODE" /* (asynInt32,    w) */
#define PhotronPMStartString          "PHOTRON_PM_START"
#define PhotronPMEndString            "PHOTRON_PM_END"
#define PhotronPMIndexString          "PHOTRON_PM_INDEX"
#define PhotronChangePMIndexString    "PHOTRON_CHANGE_PM_INDEX"
#define PhotronPMFirstString          "PHOTRON_PM_FIRST"
#define PhotronPMLastString           "PHOTRON_PM_LAST"
#define PhotronPMPlayString           "PHOTRON_PM_PLAY"
#define PhotronPMPlayRevString        "PHOTRON_PM_PLAY_REV"
#define PhotronPMSaveString           "PHOTRON_PM_SAVE"
#define PhotronPMCancelString         "PHOTRON_PM_CANCEL"
#define PhotronPMPlayFPSString        "PHOTRON_PM_PLAY_FPS"
#define PhotronPMPlayMultString       "PHOTRON_PM_PLAY_MULT"
#define PhotronPMRepeatString         "PHOTRON_PM_REPEAT"
#define PhotronIRIGString             "PHOTRON_IRIG"
#define PhotronMemIRIGDayString       "PHOTRON_MEM_IRIG_DAY"
#define PhotronMemIRIGHourString      "PHOTRON_MEM_IRIG_HOUR"
#define PhotronMemIRIGMinString       "PHOTRON_MEM_IRIG_MIN"
#define PhotronMemIRIGSecString       "PHOTRON_MEM_IRIG_SEC"
#define PhotronMemIRIGUsecString      "PHOTRON_MEM_IRIG_USEC"
#define PhotronMemIRIGSigExString     "PHOTRON_MEM_IRIG_SIGEX"
#define PhotronSyncPriorityString     "PHOTRON_SYNC_PRIORITY" /* (asynInt32, rw)  */
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
#define PhotronBurstTransString  "PHOTRON_BURST_TRANS"  /* (asynInt32, rw)  */

#define NUM_PHOTRON_PARAMS ((int)(&LAST_PHOTRON_PARAM-&FIRST_PHOTRON_PARAM+1))
