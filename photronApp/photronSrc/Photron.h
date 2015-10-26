#include <epicsEvent.h>
#include "ADDriver.h"

/** Simulation detector driver; demonstrates most of the features that areaDetector drivers can support. */
class epicsShareClass Photron : public ADDriver {
public:
    Photron(const char *portName, const char *ipAddress, int autoDetect,
                int maxBuffers, size_t maxMemory,
                int priority, int stackSize);

    /* These methods are overwritten from asynPortDriver */
    virtual asynStatus connect(asynUser* pasynUser);
    virtual asynStatus disconnect(asynUser* pasynUser);

    /* These are the methods that we override from ADDriver */
    virtual void report(FILE *fp, int details);

protected:
    //int SimGainX;
    //#define FIRST_SIM_DETECTOR_PARAM SimGainX
    //int SimPeakHeightVariation;

    //#define LAST_SIM_DETECTOR_PARAM SimPeakHeightVariation

private:
    /* These are the methods that are new to this class */
	asynStatus disconnectCamera();
	asynStatus connectCamera();

    /* These items are specific to the Photron driver */
    char *cameraId;                /* This can be an IP name, or IP address */
    unsigned long uniqueIP;
    unsigned long uniqueId;
	int autoDetect;
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
//#define SimGainXString          "SIM_GAIN_X"
//#define SimPeakHeightVariationString  "SIM_PEAK_HEIGHT_VARIATION"

//#define NUM_SIM_DETECTOR_PARAMS ((int)(&LAST_SIM_DETECTOR_PARAM - &FIRST_SIM_DETECTOR_PARAM + 1))
#define NUM_PHOTRON_PARAMS 0

