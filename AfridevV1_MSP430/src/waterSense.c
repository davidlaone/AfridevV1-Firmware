/** 
 * @file waterSense.c
 * \n Source File
 * \n Outpour MSP430 Firmware
 * 
 * \brief Routines to support water sensing algorithm.
 */

#include "outpour.h"
#include "CTS_Layer.h"
#include "waterDetect.h"

/***************************
 * Module Data Definitions
 **************************/

/**
 * \typedef sensorStats_t
 * \brief define a type to hold stats from the water sensing 
 *        algorithm
 */
typedef struct sensorStats_s {
    uint16_t lastMeasFlowRateInMl;    /**< Last flow rate calculated */
    uint16_t unknowns;                /**< Overall count of unknown case (skipped pad) */
    uint8_t pump_active;              /**< Current status of pump */
} sensorStats_t;

/****************************
 * Module Data Declarations
 ***************************/

/**
* \var wsData 
* \brief Module data structure 
*/
// static
sensorStats_t padStats;                                    /**< Array to hold stats */

/*************************
 * Module Prototypes
 ************************/

/***************************
 * Module Public Functions
 **************************/

/**
* \brief Call once as system startup to initialize the 
*        waterSense module.
* \ingroup PUBLIC_API
*/
void waterSense_init(void)
{
    // initialize pad-data analysis
    waterDetect_init();

    // zero module data structure
    memset(&padStats, 0, sizeof(sensorStats_t));

    //  Clear the stats
    waterSense_clearStats();
}

/**
* \brief Send water sense debug information to the uart.  Dumps 
*        the complete sensorStats data structure.
* \ingroup PUBLIC_API
* 
*/
void waterSense_sendDebugDataToUart(void)
{
    // Output debug information
    dbgMsgMgr_sendDebugMsg(MSG_TYPE_DEBUG_PAD_STATS, (uint8_t *)&padStats, sizeof(sensorStats_t));
}


/**
* \brief Return the last flow rate measured
* \ingroup PUBLIC_API
* 
* @return uint16_t  Flow rate in mL per second
*/
uint16_t waterSense_getLastMeasFlowRateInML(void)
{
    return (padStats.lastMeasFlowRateInMl);
}

/**
* \brief Return current max statistic
* \ingroup PUBLIC_API
*
* @return uint16_t Current max measured value for pad
*/
uint16_t waterSense_getPadStatsMax(padId_t padId)
{
    return (waterDetect_getTargetAir((uint8_t)padId));
}

/**
* \brief Return current min statistic
* \ingroup PUBLIC_API
*
* @return uint16_t Current min measured value for pad
*/
uint16_t waterSense_padStatsMin(padId_t padId)
{
    return (waterDetect_getTargetWater((uint8_t)padId));
}

/**
* \brief Return subMerged count statistic for pad
* \ingroup PUBLIC_API
*
* @return uint16_t Current submerged measured value for pad
*/
uint16_t waterSense_getPadStatsSubmerged(padId_t padId)
{
    return (waterDetect_getPadSubmergedCount((uint8_t)padId));
}

/**
* \brief Return Current unknown count statistic for pad
* \ingroup PUBLIC_API
*
* @return uint16_t Current unknowns count for pad
*/
uint16_t waterSense_getPadStatsUnknowns(void)
{
    return padStats.unknowns;
}

/**
* \brief Clear saved statistics
*
* \ingroup PUBLIC_API
*/
void waterSense_clearStats(void)
{
    padStats.unknowns = 0;
    waterDetect_clear_stats();
}

/*************************
 * Module Private Functions
 ************************/

/**
* 
* \brief Returns estimate of mL for this second. Will
*    update pad max values and next pad max values when
*    appropriate.
*
* \li INPUTS:  
* \li padCounts[TOTAL_PADS]: Taken from TI_CAPT_Raw. Generated
*     internally
*
* \li OUTPUTS:
*/
void waterSense_takeReading(void)
{
    uint8_t pad_number;
    uint16_t padCounts[TOTAL_PADS];

    // Perform the capacitive measurements
    TI_CAPT_Raw(&pad_sensors, &padCounts[0]);

    // Loop for each pad.

    for (pad_number = 0; pad_number < NUM_PADS; pad_number++)
    {
        waterDetect_add_sample(pad_number, padCounts[pad_number]);
    }
}

/**
 * 
 * @brief Perform pad coverage and flow rate calculations. 
 *        Returns number of submerged pads.
 * 
 * @return uint8_t numOfSubmergedPads: Raw count of submerged
 *         sensors
 */
uint8_t waterSense_analyzeData(void)
{
    uint8_t unknowns = 0;
    uint8_t percentile;
    uint8_t submergedPadsBitMask;   /* Which pads are submerged for this meas */
    uint8_t numOfSubmergedPads;     /* Number of pads submerged from last meas */

    waterDetect_mark_outliers();
    waterDetect_update_stats();

    numOfSubmergedPads = waterDetect_read_water_level(&submergedPadsBitMask, &unknowns);

    if (!padStats.pump_active && (numOfSubmergedPads == NUM_PADS)) {
        waterDetect_set_water_target();
        padStats.pump_active = 1;
    }
    else if (padStats.pump_active && !numOfSubmergedPads) {
        waterDetect_set_air_target();
        padStats.pump_active = 0;
    }

    padStats.unknowns += unknowns;

    if (numOfSubmergedPads)
    {
        padStats.lastMeasFlowRateInMl = waterDetect_get_flow_rate(numOfSubmergedPads, &percentile);
    } else {
        padStats.lastMeasFlowRateInMl = 0;
    }

    return (numOfSubmergedPads);
}

