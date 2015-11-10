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
 
  /* These are called from C and so must be public */
  static void shutdown(void *arg);
  
protected:
    int PhotronStatus;
    int Photron8BitSel;
    int PhotronRecRate;
    #define FIRST_PHOTRON_PARAM PhotronStatus
    #define LAST_PHOTRON_PARAM PhotronRecRate

private:
  /* These are the methods that are new to this class */
  asynStatus disconnectCamera();
  asynStatus connectCamera();
  asynStatus getCameraInfo();
  asynStatus setGeometry();
  asynStatus getGeometry();
  asynStatus readParameters();
  asynStatus readImage();
  asynStatus setTransferOption();
  asynStatus setRecordRate(epicsInt32 value);
  asynStatus setStatus(epicsInt32 value);

  /* These items are specific to the Photron driver */
  // constructor
  char *cameraId;                /* This can be an IP name, or IP address */
  int autoDetect;
  epicsEventId startEventId;
  epicsEventId stopEventId;
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
  char *sensorBits;
  char *bitDepth;
  unsigned long RateListSize;
  unsigned long RateList[PDC_MAX_LIST_NUMBER];
  // readParameters
  unsigned long nStatus; // replace with PV?
  unsigned long nRate; // units = frames per second
  unsigned long triggerMode;
  unsigned long trigAFrames;
  unsigned long trigRFrames;
  unsigned long trigRCount;
  unsigned long highSpeedMode;
  /* Our data */
  NDArray *pRaw;
};

/* Declare this function here so that its implementation can appear below
   the contructor in the source file */ 
static void PhotronTaskC(void *drvPvt);

typedef struct {
  ELLNODE node;
  Photron *pCamera;
} cameraNode;

// Define param strings here
 /*                             String                  interface  access   */
#define PhotronStatusString     "PHOTRON_STATUS"    /* (asynInt32,    rw)   */
#define Photron8BitSelectString "PHOTRON_8_BIT_SEL" /* (asynInt32,    rw)   */
#define PhotronRecordRateString "PHOTRON_REC_RATE"  /* (asynInt32,    rw)   */

#define NUM_PHOTRON_PARAMS ((int)(&LAST_PHOTRON_PARAM-&FIRST_PHOTRON_PARAM+1))
