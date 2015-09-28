#include <epicsEvent.h>
#include "ADDriver.h"

/** Simulation detector driver; demonstrates most of the features that areaDetector drivers can support. */
class epicsShareClass Photron : public ADDriver {
public:
    Photron(const char *portName, int maxSizeX, int maxSizeY, NDDataType_t dataType,
                int maxBuffers, size_t maxMemory,
                int priority, int stackSize);

    /* These are the methods that we override from ADDriver */
    virtual void report(FILE *fp, int details);

protected:
    //int SimGainX;
    //#define FIRST_SIM_DETECTOR_PARAM SimGainX
    //int SimPeakHeightVariation;

    //#define LAST_SIM_DETECTOR_PARAM SimPeakHeightVariation

private:
    /* These are the methods that are new to this class */

    /* Our data */
    epicsEventId startEventId;
    epicsEventId stopEventId;
    NDArray *pRaw;
};

// Define param strings here
//#define SimGainXString          "SIM_GAIN_X"
//#define SimPeakHeightVariationString  "SIM_PEAK_HEIGHT_VARIATION"

//#define NUM_SIM_DETECTOR_PARAMS ((int)(&LAST_SIM_DETECTOR_PARAM - &FIRST_SIM_DETECTOR_PARAM + 1))
#define NUM_PHOTRON_PARAMS 0

