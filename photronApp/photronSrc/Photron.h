#include <epicsEvent.h>
#include "ADDriver.h"

#include "SDK/Include/PDCLIB.h"

/** Simulation detector driver; demonstrates most of the features that areaDetector drivers can support. */
class epicsShareClass Photron : public ADDriver {
public:
	/* Constructor and Destructor */
    Photron(const char *portName, const char *ipAddress, int autoDetect,
                int maxBuffers, size_t maxMemory,
                int priority, int stackSize);
	~Photron();
				
    /* These methods are overwritten from asynPortDriver */
    virtual asynStatus connect(asynUser* pasynUser);
    virtual asynStatus disconnect(asynUser* pasynUser);

    /* These are the methods that we override from ADDriver */
	virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual void report(FILE *fp, int details);
	void PhotronTask(); /**< Should be private, but gets called from C, so must be public */
 
	/* These are called from C and so must be public */
	static void shutdown(void *arg);
	
protected:
    int P8BitSel;
    #define FIRST_PHOTRON_PARAM P8BitSel
    
    #define LAST_PHOTRON_PARAM P8BitSel

private:
    /* These are the methods that are new to this class */
	asynStatus disconnectCamera();
	asynStatus connectCamera();
	asynStatus setGeometry();
 	asynStatus readParameters();
	asynStatus readImage();
	asynStatus readImageDONOTUSE();
	asynStatus setTransferOption();

    /* These items are specific to the Photron driver */
    char *cameraId;                /* This can be an IP name, or IP address */
	int autoDetect;
    unsigned long uniqueIP;
    unsigned long uniqueId;
    unsigned long nDeviceNo;
	unsigned long nChildNo;
	unsigned long nStatus; // camera status
	/* */
	unsigned long deviceCode;
	TCHAR deviceName[PDC_MAX_STRING_LENGTH];
	/* Indices of functions in functionList range from 2 to 97 */
	char functionList[98];
	/* The actual version number is 1/100 of the retrieved value */
	unsigned long version;
    unsigned long sensorWidth;
    unsigned long sensorHeight;
    char *sensorBits;
	unsigned long maxChildDevCount;
	unsigned long childDevCount;
	int framesRemaining;
	unsigned long triggerMode;
	unsigned long trigAFrames;
	unsigned long trigRFrames;
	unsigned long trigRCount;

    /* Our data */
    epicsEventId startEventId;
    epicsEventId stopEventId;
    NDArray *pRaw;

};

typedef struct {
    ELLNODE node;
    Photron *pCamera;
} cameraNode;


// Define param strings here
 /*                                       String              asyn interface  access   Description  */
#define Photron8BitSelectString          "PHOTRON_8_BIT_SEL" /* (asynInt32,    rw)     bit position during 8-bit transfer from a device of more than 8 bits. */

#define NUM_PHOTRON_PARAMS ((int)(&LAST_PHOTRON_PARAM - &FIRST_PHOTRON_PARAM + 1))

