/** 
 * @file storage.c
 * \n Source File
 * \n Cascade MSP430 Firmware
 * 
 * \brief Routines to support processing, storing and 
 *        transmitting water data statistics.
 */

#include "outpour.h"

/***************************
 * Module Data Definitions
 **************************/

/**
 * \def TRANSMISSION_RATE_IN_DAYS
 * Specify how often to transmit data 
 */
#define TRANSMISSION_RATE_IN_DAYS 1

/**
 * \def TOTAL_WEEKLY_LOGS
 * \brief Specify the number of weekly logs in flash.
 */
#define TOTAL_WEEKLY_LOGS 1

/**
 * \def CURRENT_WEEKLY_LOG_NUM
 * Always 0. Other products support multiple weeks of data 
 * storage.  But Afridev1 does not. 
 */
#define CURRENT_TX_WEEK 0

/**
 * \def CURRENT_WEEKLY_LOG_NUM
 * Always 0. Other products support multiple weeks of data 
 * storage.  But Afridev1 does not. 
 */
#define CURRENT_WEEKLY_LOG_NUM 0

/**
 * \def WEEKLY_LOG_SIZE
 * \brief Specify the total size of a weekly block allocated in 
 *        flash.
 */
#define WEEKLY_LOG_SIZE ((uint16_t)0x400)

/**
 * \def TOTAL_DAYS_IN_A_WEEK
 * \brief For clarity in the code
 */
#define TOTAL_DAYS_IN_A_WEEK ((uint8_t)7)

/**
 * \def TOTAL_HOURS_IN_A_DAY
 * \brief For clarity in the code
 */
#define TOTAL_HOURS_IN_A_DAY ((uint8_t)24)

/**
 * \def TOTAL_MINUTES_IN_A_HOUR
 * \brief For clarity in the code
 */
#define TOTAL_MINUTES_IN_A_HOUR ((uint8_t)60)

/**
 * \def TOTAL_SECONDS_IN_A_MINUTE
 * \brief For clarity in the code
 */
#define TOTAL_SECONDS_IN_A_MINUTE ((uint8_t)60)

/**
 * \def FLASH_WRITE_ONE_BYTE
 * \brief For clarity in the code
 */
#define FLASH_WRITE_ONE_BYTE ((uint8_t)1)

/**
 * \def DAILY_MILLILITERS_ACTIVATION_THRESHOLD
 * \brief Used as the daily milliliters threshold to consider 
 *        the unit activated.
 */
#define DAILY_MILLILITERS_ACTIVATION_THRESHOLD ((uint16_t)50*1000)

/**
 * \def FLASH_BLOCK_SIZE
 * \brief Define how big a flash block is.  Represents the 
 *        minimum size that can be erased.
 */
#define FLASH_BLOCK_SIZE ((uint16_t)512)

/**
 * \typedef dailyLog_t
 * \brief Define the structure of the daily log data that is 
 *        sent inside the daily packet.
 */
typedef struct __attribute__((__packed__))dailyLog_s {
    uint16_t litersPerHour[24];      /**< 48, 00-47 */
    uint16_t padMax[6];              /**< 12, 48-59 */
    uint16_t padMin[6];              /**< 12, 60-71 */
    uint16_t padSubmerged[6];        /**< 12, 72-83 */
    uint16_t dailyLiters;            /**< 02, 84-85 */
    uint16_t unknowns;               /**< 02, 86-87 */
    uint8_t  redFlag;                /**< 01, 88 */
} dailyLog_t;

typedef union packetHeader_s {
    msgHeader_t msgHeader;
    uint8_t bytes[16]; // force to 16 bytes
} packetHeader_t;

typedef union packetData_s {
    dailyLog_t dailyLog;
    uint8_t bytes[112]; // force to 112 bytes
} packetData_t;

/**
 * \typedef dailyPacket_t
 * \brief Define the structure of the daily log packet that is 
 *        sent to the server.  Note that it consists of a header
 *        and the data.  Both the header and the data sections
 *        are set to a specific size by using unions.  The total
 *        size of the packet is set at 128 bytes. 
 */
typedef struct dailyPacket_s {
    packetHeader_t packetHeader;
    packetData_t packetData;
} dailyPacket_t;

/**
 * \typedef weeklyLog_t
 * \brief  Define the layout of the weekly log in flash.  It 
 *         currently consists of the 7 daily log packets and
 *         meta data.
 */
typedef struct weeklyLog_s {
    dailyPacket_t dailyPackets[7];       /**< The seven daily logs of the week */
    uint8_t clearOnTransmit[7];          /**< Byte cleared for day when log transmitted */
    uint8_t clearOnReady[7];             /**< Byte cleared for day when log ready to send */
} weeklyLog_t;

/**
 * \typedef storageData_t 
 * \brief Define a container to hold data for the storage 
 *        module.
 */
typedef struct storageData_s {
    uint16_t daysActivated;            /**< Total days unit has been activated */
    uint16_t minuteMilliliterSum;      /**< Running milliliter sum for current minute */
    uint32_t hourMilliliterSum;        /**< Running milliliter sum for current hour */
    uint32_t dayMilliliterSum;         /**< Running milliliter sum for current day */

    uint8_t storageTime_seconds;       /**< Current storage time - sec  */
    uint8_t storageTime_minutes;       /**< Current storage time - min */
    uint8_t storageTime_hours;         /**< Current storage time - hour */
    uint8_t storageTime_dayOfWeek;     /**< Current storage time - day */
    uint8_t storageTime_week;          /**< Current storage time - week */

    bool alignStorageFlag;             /**< True if time to align storage time */
    uint8_t alignSecond;               /**< Time to align at - sec */
    uint8_t alignMinute;               /**< Time to align at - min */
    uint8_t alignHour24;               /**< Time to align at - hour */
    int32_t alignSafetyCheckInSec;     /**< Max time to wait for an align event */

    uint8_t transmissionRateInDays;    /**< Specify how often to transmit data */
    uint8_t totalDailyLogsTransmitted; /**< Counter of total daily logs transmitted in current tx session */
    bool haveSentDailyLogs;            /**< Flag to indicate we have transmitted a daily log */

} storageData_t;

/****************************
 * Module Data Declarations
 ***************************/

/*
 *  These are the weekly log entries in flash.  Each weekly log entry
 *  contains 7 daily log entries plus some meta data.  The storage module
 *  rotates through each weekly log, storing the water stats for each day
 *  of the week.  The day's water stats are stored at the end of each
 *  day (midnight of the following day).
 */
#pragma DATA_SECTION(week1Log, ".week1Data")
static const weeklyLog_t week1Log;

#define WEEKLY_LOG_ADDRESS_IN_FLASH ((weeklyLog_t *)&week1Log)

/**
 * \var stData 
 * \brief Declare the module data container 
 */
// static
storageData_t stData;

/********************* 
 * Module Prototypes
 *********************/

static bool doesAlignTimeMatch(void);
static void recordLastMinute(void);
static void recordLastHour(void);
static void recordLastDay(void);
static void writeStatsToDailyLog(void);
static dailyLog_t* getDailyLogAddr(uint8_t dayOfTheWeek);
static msgHeader_t* getDailyHeaderAddr(uint8_t dayOfTheWeek);
static dailyPacket_t* getDailyPacketAddr(uint8_t dayOfTheWeek);
static void eraseWeeklyLog(void);
static void prepareNextWeeklyLog(void);
static void prepareDailyLog(void);
static void markDailyLogAsReady(uint8_t dayOfTheWeek);
static bool isDailyLogReady(uint8_t dayOfTheWeek);
static void markDailyLogAsTransmitted(uint8_t dayOfTheWeek);
static bool wasDailyLogTransmitted(uint8_t dayOfTheWeek);
static void checkAndTransmitMonthlyCheckin(void);
static void checkAndTransmitDailyLogs(bool overrideTransmissionRate);
static void sendMonthlyCheckin(void);
static void sendActivatedMessage(void);
static void clearAlignStats(void);

/***************************
 * Module Public Functions
 **************************/

/**
* \brief Call once as system startup to initialize the storage 
*        module.
* \ingroup PUBLIC_API
*/
void storageMgr_init(void) {
    memset(&stData, 0, sizeof(storageData_t));

    // Set the default value for transmission rate
    stData.transmissionRateInDays = TRANSMISSION_RATE_IN_DAYS;

    // Reset the weekly log number
    storageMgr_resetWeeklyLogs();
}

/**
* \brief This is the flash storage manager. It is called from 
*        the main processing loop.
* \ingroup EXEC_ROUTINE
*/
void storageMgr_exec(uint16_t currentFlowRateInMLPerSec) {

    // If we are waiting for an alignment event to occur, see if there
    // is a match (GMT time == alignment time).
    if (stData.alignStorageFlag == true) {
        // Decrement our safety down counter.
        // We use a safety down-counter to make sure that we don't wait more
        // than 24 hours for the alignment event to occur.
        stData.alignSafetyCheckInSec -= SECONDS_PER_TREND;
        if (doesAlignTimeMatch() || (stData.alignSafetyCheckInSec < 0)) {
            // If the current time is equal to the storage offset,
            // then zero storage time and reset the weekly log number
            stData.alignStorageFlag = false;
            clearAlignStats();
            storageMgr_resetWeeklyLogs();
        }
        // Don't start storing any data until we are officially aligned.
        return;
    }

    // Update the flow rate per minute running sum (in milliliters)
    stData.minuteMilliliterSum += currentFlowRateInMLPerSec;

    // Increment the unit of time, record the amount of water, reset the previous unit of time

    // Note: this function is called every two seconds by the main loop.
    stData.storageTime_seconds += SECONDS_PER_TREND;

    if (stData.storageTime_seconds == TOTAL_SECONDS_IN_A_MINUTE) {
        // Record data
        recordLastMinute();
        // Update time
        stData.storageTime_minutes++;
        stData.storageTime_seconds = 0;
    }
    if (stData.storageTime_minutes == TOTAL_MINUTES_IN_A_HOUR) {
        // Record data
        recordLastHour();
        // Update time
        stData.storageTime_hours++;
        stData.storageTime_minutes = 0;
    }
    if (stData.storageTime_hours == TOTAL_HOURS_IN_A_DAY) {
        // Record data
        recordLastDay();
        // Update Time
        stData.storageTime_dayOfWeek++;
        stData.storageTime_hours = 0;
    }
    if (stData.storageTime_dayOfWeek == TOTAL_DAYS_IN_A_WEEK) {
        // Update Time
        stData.storageTime_dayOfWeek = 0;
        stData.storageTime_week++;
        // Check if its time to send a monthly checkin message.
        // We only send the message if we are not activated and four
        // weeks have passed.
        checkAndTransmitMonthlyCheckin();
    }
}

/**
* \brief Save the GMT time values that the storage clock 
*        midnight should be aligned with. These will be compared
*        to current GMT time for a match.  All storage
*        operations are stopped until the GMT match time occurs.
* \ingroup PUBLIC_API
* 
* @param alignSecond  GMT storage clock alignment second value 
* @param alignMinute  GMT storage clock alignment minute value 
* @param alignHour24  GMT storage clock alignment hour value 
*/
void storageMgr_setStorageAlignmentTime(uint8_t alignSecond, uint8_t alignMinute, uint8_t alignHour24) {

    bool alignTimeIsValid = true;
    stData.alignSecond = alignSecond;
    stData.alignMinute = alignMinute;
    stData.alignHour24 = alignHour24;

    // Validate for legal values.
    if (alignSecond > 59) {
        alignTimeIsValid = false;
    } else if (alignMinute > 59) {
        alignTimeIsValid = false;
    } else if (alignHour24 > 23) {
        alignTimeIsValid = false;
    }

    if (alignTimeIsValid) {
        // Set the flag that we are waiting for an alignment event
        stData.alignStorageFlag = true;
        clearAlignStats();

        // We create a safety down-counter in seconds to make sure that we don't
        // miss the alignment event.  The alignment event is currently
        // dependent on matching the alignment time against the unit's GMT
        // clock.  The two must match exactly for the alignment to occur.  If
        // for some reason the unit is busy and the check is not performed at
        // that exact time, then the unit will never get out of the alignment
        // waiting phase. The safety down-counter will catch that situation.

        // Set safety alignment check time counter
        stData.alignSafetyCheckInSec = ((sys_tick_t)SECONDS_PER_DAY);
    }
}

/**
* \brief Override the unit activation.  Either enable or 
*        disable.
* \ingroup PUBLIC_API
*
* @param flag set true or false 
*/
void storageMgr_overrideUnitActivation(bool flag) {
    if ((flag == true) && (stData.daysActivated == 0)) {
        stData.daysActivated = 1;
    } else if (flag == false) {
        stData.daysActivated = 0;
    }
}

/**
* \brief Return the number of days the unit has been activated.
* \ingroup PUBLIC_API
* 
* @return uint16_t Number of days unit has been activated.
*/
uint16_t storageMgr_getDaysActivated(void) {
    return stData.daysActivated;
}

/**
* \brief Resets the current weekly log number.
* \ingroup PUBLIC_API
*/
void storageMgr_resetWeeklyLogs(void) {
    // Does nothing for Afridev1 because there is only one weekly log.
    return;
}

/**
 * \brief This function is used to identify the next daily that
 *        is ready to transmit.  If there is a log ready for
 *        transmit, the pointer and size are returned. If no
 *        daily log is ready for transmit, the function returns
 *        0.  If a daily log pointer is returned, then that
 *        daily log is marked as being transmitted in the weekly
 *        info meta data.
 * \brief To Determine if a daily log is ready or has been 
 *        transmitted, it looks at the weekly info meta data
 *        associated with each weekly log.  Status bits are
 *        maintained in the meta data area to mark if a daily
 *        log is ready for transmit and if it has already been
 *        transmitted.
 * \brief The function will sequentially search all the daily 
 *        logs of every weekly log from oldest to newest looking
 *        for any daily logs that are marked as ready but not
 *        yet transmitted.
 *  
 * \ingroup PUBLIC_API
 * 
 * \param dataPP Pointer to a pointer that is filled in with the
 *               address of the daily log.
 * 
 * \return uint16_t Size of the daily log to send, otherwise set
 *         to zero if no daily log is ready to transmit
 */
uint16_t storageMgr_getNextDailyLogToTransmit(uint8_t **dataPP) {
    uint8_t i;
    uint16_t length = 0;
    bool allDailyLogsTransmitted = false;

    do {
        // Loop for each day of the week until a daily log is found
        // that is ready and has not been transmitted.
        for (i = 0; i < TOTAL_DAYS_IN_A_WEEK; i++) {
            // Get the flags to check if daily log is ready
            // and has not yet been transmitted.
            bool logReady = isDailyLogReady(i);
            bool wasTransmitted = wasDailyLogTransmitted(i);
            if (logReady && !wasTransmitted) {
                // Get the address of the daily log
                dailyPacket_t *dpP = getDailyPacketAddr(i);
                *dataPP = (uint8_t *)dpP;
                length = sizeof(dailyPacket_t);
                // Mark this daily log as being transmitted
                markDailyLogAsTransmitted(i);
                break;
            }
        }

        // Check if we transmitted all days of current transmit week.
        // If so, move to following week.
        if (i == TOTAL_DAYS_IN_A_WEEK) {
            length = 0;
            allDailyLogsTransmitted = true;
        }
    } while ((length == 0) && !allDailyLogsTransmitted);

    // Perform a safety check to makes sure we are not stuck transmitting
    // a daily log over and over.  We should never transmit more than the
    // total number of daily logs stored.
    if (length != 0) {
        if (stData.totalDailyLogsTransmitted < (TOTAL_DAYS_IN_A_WEEK * TOTAL_WEEKLY_LOGS)) {
            stData.totalDailyLogsTransmitted++;
            // Mark flag to indicate that at least one daily log has been sent
            stData.haveSentDailyLogs = true;
        } else {
            length = 0;
        }
    }

    return length;
}

#if 0
/**
* \brief Return the total number of daily logs that are ready to 
*        transmit for the specified week.
* 
* @return uint8_t Total logs ready to transmit for the specified
*         week.
*/
uint8_t storageMgr_getNumDailyLogsToTransmit(uint8_t weeklyLogNum) {
    uint8_t i;
    uint8_t numOfDailyLogsReadyToTransmit = 0;
    // Loop for each day of the week.  Count the total number of daily logs
    // that are ready and have not been transmitted.
    for (i = 0; i < TOTAL_DAYS_IN_A_WEEK; i++) {
        // Get the flags to check if daily log is ready
        // and has not yet been transmitted.
        bool logReady = isDailyLogReady(i);
        bool wasTransmitted = wasDailyLogTransmitted(i);
        if (logReady && !wasTransmitted) {
            numOfDailyLogsReadyToTransmit++;
        }
    }
    return numOfDailyLogsReadyToTransmit;
}
#endif

/**
* \brief Send debug information to the uart.  
* \ingroup PUBLIC_API
*/
void storageMgr_sendDebugDataToUart(void) {
    // Get the shared buffer (we borrow the ota buffer)
    uint8_t *payloadP = modemMgr_getSharedBuffer();
    // Add structure size plus two for start byte and payload msg ID
    uint8_t payloadSize = sizeof(storageData_t) + 2;
    // Add payload start byte
    payloadP[0] = 0x1;
    // Add payload msg ID
    payloadP[1] = MSG_TYPE_DEBUG_STORAGE_INFO;
    // Add debug data
    memcpy(&payloadP[2], &stData, sizeof(storageData_t));
    // Output debug information
    dbgMsgMgr_sendDebugMsg(MSG_TYPE_DEBUG_STORAGE_INFO, payloadP, payloadSize);
}

/**
 *  \brief Set how often to transmit the daily logs (in days).
 *         The legal values are 1 and 7.
 */
void storageMgr_setTransmissionRate(uint8_t transmissionRateInDays) {
    if ((transmissionRateInDays != 1) && (transmissionRateInDays != 7)) {
        transmissionRateInDays = 1;
    }
    stData.transmissionRateInDays = transmissionRateInDays;
}

/**
* \brief Read the storage time parameters.
* 
* @param bufP  Pointer to buffer to store the data.
* 
* @return uint8_t Returns the number of data bytes stored into 
*         the buffer (9 bytes).
*/
uint8_t storageMgr_getStorageClockInfo(uint8_t *bufP) {
    *bufP++ = stData.storageTime_seconds;       /**< Current storage time - sec  */
    *bufP++ = stData.storageTime_minutes;       /**< Current storage time - min  */
    *bufP++ = stData.storageTime_hours;         /**< Current storage time - hour */
    *bufP++ = stData.storageTime_dayOfWeek;     /**< Current storage time - day  */
    *bufP++ = stData.storageTime_week;          /**< Current storage time - week */
    *bufP++ = stData.alignStorageFlag;          /**< True if time to align storage time */
    *bufP++ = stData.alignSecond;               /**< Time to align at - sec */
    *bufP++ = stData.alignMinute;               /**< Time to align at - min */
    *bufP++ = stData.alignHour24;               /**< Time to align at - hour */
    return 9;
}

/**
* \brief Initialize the header portion of an outgoing message.
* 
* @param dataPtr  The buffer to store the header data
* @param payloadMsgId The outgoing message type
* 
* @return uint8_t Returns the length of the header added to the 
*         buffer.
*/
uint8_t storageMgr_prepareMsgHeader(uint8_t *dataPtr, uint8_t payloadMsgId) {

    // The daily header structure is defined so that it will be exactly 16
    // bytes. The daily header is used to start all messages sent to the cloud.
    msgHeader_t *msgHeaderP = (msgHeader_t *)dataPtr;
    timePacket_t tp;

    // Get current time
    getBinTime(&tp);

    // Add Payload start byte
    msgHeaderP->payloadStartByte = 0x1;
    // Add Payload Message ID
    msgHeaderP->payloadMsgId = payloadMsgId;
    // Add Product ID
    msgHeaderP->productId = OUTPOUR_PRODUCT_ID;
    // Add Time
    msgHeaderP->GMTsecond = tp.second;
    msgHeaderP->GMTminute = tp.minute;
    msgHeaderP->GMThour = tp.hour24;
    msgHeaderP->GMTday = tp.day;
    msgHeaderP->GMTmonth = tp.month;
    msgHeaderP->GMTyear = tp.year;
    // Add FW Version
    msgHeaderP->fwMajor = FW_VERSION_MAJOR;
    msgHeaderP->fwMinor = FW_VERSION_MINOR;
    // Add Days Activated
    msgHeaderP->daysActivatedMsb = stData.daysActivated >> 8;
    msgHeaderP->daysActivatedLsb = stData.daysActivated & 0xFF;
    // Add storage week
    msgHeaderP->storageWeek = stData.storageTime_week;
    // Add storage Day of the week
    msgHeaderP->storageDay = stData.storageTime_dayOfWeek;
    // Just set to an easy byte to recognize byte for now.
    msgHeaderP->reserve1 = 0xA5;
    return (sizeof(packetHeader_t));
}

/*************************
 * Module Private Functions
 ************************/

/**
* \brief This function determines if its time to transmit the 
*        daily logs.  At a maximum, we will send up to
*        WEEKLY_LOG_NUM_MAX total weeks worth of data.
* 
* @param overrideTransmissionRate  If set to true, then daily 
*                                  log transmission will be
*                                  initiated regardless of the
*                                  transmissionRate.
*/
static void checkAndTransmitDailyLogs(bool overrideTransmissionRate) {
    // Only consider transmitting data if we are activated
    if (stData.daysActivated) {
        bool transmissionRateMet = false;

        if (stData.transmissionRateInDays == 7) {
            // Check if have reached the day to transmit the daily logs.
            // Currently, we transmit once a week, always at the end of the week.
            if (stData.storageTime_dayOfWeek == (TOTAL_DAYS_IN_A_WEEK - 1)) {
                // Set flag
                transmissionRateMet = true;
            }
        } else {
            // Daily transmit
            transmissionRateMet = true;
        }

        if (transmissionRateMet || overrideTransmissionRate) {
            // Its time to transmit the accumulated daily logs
            stData.totalDailyLogsTransmitted = 0;
            // Start the process of sending the daily logs.
            // This will send the oldest daily log that is ready.
            dataMsgMgr_sendDailyLogs();
        }
    }
}

/**
* \brief This function determines if its time to transmit a
*        monthly check in message.  If we are not activated yet
*        or have not transmitted a daily log in the last four
*        weeks, then send a monthly check-in message.
*/
static void checkAndTransmitMonthlyCheckin(void) {
    if ((stData.storageTime_week % 4) == 0) {
        if (!stData.daysActivated || !stData.haveSentDailyLogs) {
            sendMonthlyCheckin();
        }
        // Reset flag.  We want to identify whether at least one
        // daily log is sent in the upcoming month.
        stData.haveSentDailyLogs = false;
    }
}

/**
* @brief Prepare and initiate sending the the monthly check-in 
*        message.
*/
static void sendMonthlyCheckin(void) {
    // Get the shared buffer (we borrow the ota buffer)
    uint8_t *payloadP = modemMgr_getSharedBuffer();
    // Fill in the buffer with the standard message header
    uint8_t payloadSize = storageMgr_prepareMsgHeader(payloadP, MSG_TYPE_CHECKIN);
    // Initiate sending the monthly check-in message
    dataMsgMgr_sendDataMsg(MSG_TYPE_CHECKIN, payloadP, payloadSize);
}

/**
* @brief Prepare and initiate sending the the unit activated 
*        message.
*/
static void sendActivatedMessage(void) {
    uint16_t dayLiterSum = stData.dayMilliliterSum / 1000;
    // Get the shared buffer (we borrow the ota buffer)
    uint8_t *payloadP = modemMgr_getSharedBuffer();
    // Fill in the buffer with the standard message header
    uint8_t payloadSize = storageMgr_prepareMsgHeader(payloadP, MSG_TYPE_ACTIVATED);
    // Add total liters for the day
    payloadP[payloadSize++] = dayLiterSum >> 8;
    payloadP[payloadSize++] = dayLiterSum & 0xFF;
    // Initiate sending the activated message
    dataMsgMgr_sendDataMsg(MSG_TYPE_ACTIVATED, payloadP, payloadSize);
}

/**
 * \brief Maintain the running sum for the hour.  At the end of 
 *        each minute, add currentMinuteML into the hourly
 *        running sum.
 */
static void recordLastMinute(void) {
    stData.hourMilliliterSum += stData.minuteMilliliterSum;
    stData.minuteMilliliterSum = 0;
}

/**
 * \brief Write the total liters for the current hour into 
 *        flash. Update the running sum for the total daily
 *        liters.
 * \note The liters for the hour is stored in the current daily
 *       log contained in the current weekly log section.
 * \li The hourly water volume is stored in the daily log as 
 *     liters. A left shift of 5 bits is used during the
 *     conversion sequence from Milliliters to Liters (Liters =
 *     Milliliters/1024). This does produce a small error as
 *     part of the conversion.
 */
static void recordLastHour(void) {
    // Get pointer to today's dailyLog in flash.
    dailyLog_t *dailyLogsP = getDailyLogAddr(stData.storageTime_dayOfWeek);
    // Get address to liters parameter in the dailyLog
    uint8_t *hourlyLitersAddrP = (uint8_t *)&(dailyLogsP->litersPerHour[stData.storageTime_hours]);
    uint16_t litersForThisHour = 0;

    // The hourly water volume is stored in the log as total liters (really Milliliters/32)
    litersForThisHour = (stData.hourMilliliterSum >> 5) & 0xffff;

    if (stData.daysActivated) {
        // If this is the completion of the first hour of the day, then prepare
        // the daily log area in flash.
        if (stData.storageTime_hours == 0) {
            // If this is the first day of the week, then prepare the
            // weekly log area in flash.
            if (stData.storageTime_dayOfWeek == 0) {
                prepareNextWeeklyLog();
            }
            prepareDailyLog();
        }
        // Store the hourly milliliter value to the flash log
        msp430Flash_write_int16(hourlyLitersAddrP, litersForThisHour);
    }

    // Track the total daily milliliters
    stData.dayMilliliterSum += stData.hourMilliliterSum;

    // Zero hour sum
    stData.hourMilliliterSum = 0;
}

/**
* \brief Write the pad statistics to the daily log.
* \note If a new redFlag condition has occurred, then send the 
*       daily logs completed for this week.
*/
static void recordLastDay(void) {

    // Only write to daily log in flash if unit is activated
    if (stData.daysActivated) {

        // Get pointer to today's dailyLog in flash.
        dailyLog_t *dailyLogsP = getDailyLogAddr(stData.storageTime_dayOfWeek);

        // Write data stats to dailyLog
        writeStatsToDailyLog();

        // Mark the current daily log as ready in the weekly log meta data.
        markDailyLogAsReady(stData.storageTime_dayOfWeek);

        // Check if its time to transmit data
        // Data is only sent if we are activated and we have reached
        // the end of the week.
        checkAndTransmitDailyLogs(false);

        // Increment total days activated
        stData.daysActivated++;
    } else {
        // If we are not writing to the daily log, then we need to clear the flow
        // stats accumulated on a daily basis.  These are collected by
        // the writeStatsToDailyLog function if the unit is activated.
        // But because we are not activated, we need to clear them here instead.

        // Clear all the pad statistics
        waterSense_clearStats();
    }

    // If unit is not activated, check if it should be based on the total liters for the day.
    // If the number of measured daily liters exceeds the threshold, then consider
    // the unit activated.  Unit is considered not-activated if daysActivated is 0.
    if (!stData.daysActivated && (stData.dayMilliliterSum > DAILY_MILLILITERS_ACTIVATION_THRESHOLD)) {
        // Send the activated status message
        sendActivatedMessage();
        // unit is now activated
        stData.daysActivated++;
    }

    // Reset the daily based statistics
    stData.dayMilliliterSum = 0;
}

/**
* \brief Fill in the water stats into the daily log.
*/
static void writeStatsToDailyLog(void) {
    uint8_t *addr;
    uint16_t i = 0;
    uint16_t u16Val;
    uint8_t u8Val;


    // Get pointer to today's dailyLog in flash.
    dailyLog_t *dailyLogsP = getDailyLogAddr(stData.storageTime_dayOfWeek);

#if 0
    // daily log layout for reference (see daily log structure)
    uint16_t litersPerHour[24];      /**< 48, 00-47 */
    uint16_t padMax[6];              /**< 12, 48-59 */
    uint16_t padMin[6];              /**< 12, 60-71 */
    uint16_t padSubmerged[6];        /**< 12, 72-83 */
    uint16_t dailyLiters;            /**< 02, 84-85 */
    uint16_t unknowns;               /**< 02, 86-87 */
    uint8_t  redFlag;                /**< 01, 88 */
#endif

    // Write the redFlag condition to the daily log - NO LONGER USED
    u8Val = 0;
    msp430Flash_write_bytes((uint8_t *)&(dailyLogsP->redFlag), (uint8_t *)&u8Val, FLASH_WRITE_ONE_BYTE);

    // Write the total daily liters value for the dailyLiters field
    msp430Flash_write_int16((uint8_t *)&(dailyLogsP->dailyLiters), (stData.dayMilliliterSum / 1000));

    // NOTE - consider removing pad stats for Afridev1

    // Write per PAD stats to flash
    for (i = 0; i < 6; i++) {
        addr = 	(uint8_t *)&(dailyLogsP->padMax[i]);
        u16Val = waterSense_getPadStatsMax((padId_t)i);
        msp430Flash_write_int16(addr, u16Val);

        addr = 	(uint8_t *)&(dailyLogsP->padMin[i]);
        u16Val = waterSense_padStatsMin((padId_t)i);
        msp430Flash_write_int16(addr, u16Val);

        addr = 	(uint8_t *)&(dailyLogsP->padSubmerged[i]);
        u16Val = waterSense_getPadStatsSubmerged((padId_t)i);
        msp430Flash_write_int16(addr, u16Val);

    }
    // Write unknown stat to flash
    addr = 	(uint8_t *)&(dailyLogsP->unknowns);
    u16Val = waterSense_getPadStatsUnknowns();
    msp430Flash_write_int16(addr, u16Val);

    // Clear all the pad statistics
    waterSense_clearStats();
}

/**
* \brief Utility routine to check if the alignment time matches 
*        the current GMT time for seconds, minutes and hours.
* \note We currently don't match seconds, as it should not be 
*       needed and adds risk to missing the align time window.
* 
* @return bool Returns true if a match occurred.
*/
static bool doesAlignTimeMatch(void) {
    timePacket_t NowTime;
    getBinTime(&NowTime);
    if ((NowTime.minute == stData.alignMinute) && (NowTime.hour24 == stData.alignHour24)) {
        return true;
    } else {
        return false;
    }
}

/**
* \brief Utility function to get the daily log address contained
*        in the weekly log.
* 
* @param dayOfTheWeek  Which day of the week.
* 
* @return dailyLog_t* Returns a pointer to the daily log
*/
static dailyLog_t* getDailyLogAddr(uint8_t dayOfTheWeek) {
    weeklyLog_t *wlP = WEEKLY_LOG_ADDRESS_IN_FLASH;
    dailyLog_t *dailyLogP = &wlP->dailyPackets[dayOfTheWeek].packetData.dailyLog;
    return dailyLogP;
}

/**
* \brief   Utility function to get the address to the header 
*          portion of a daily packet.
* 
* @param dayOfTheWeek Which day of the week
* 
* @return msgHeader_t*  Pointer to the packet header.
*/
static msgHeader_t* getDailyHeaderAddr(uint8_t dayOfTheWeek) {
    weeklyLog_t *wlP = WEEKLY_LOG_ADDRESS_IN_FLASH;
    msgHeader_t *msgHeaderP = &wlP->dailyPackets[dayOfTheWeek].packetHeader.msgHeader;
    return msgHeaderP;
}

/**
* \brief Utility function to get the address to a daily packet.
* 
* @param dayOfTheWeek Which day of the week
* 
* @return msgHeader_t*  Pointer to the packet.
*/
static dailyPacket_t* getDailyPacketAddr(uint8_t dayOfTheWeek) {
    weeklyLog_t *wlP = WEEKLY_LOG_ADDRESS_IN_FLASH;
    dailyPacket_t *dailyPacketP = &wlP->dailyPackets[dayOfTheWeek];
    return dailyPacketP;
}

/**
* \brief Erase the weekly log container (in flash).
*/
static void eraseWeeklyLog(void) {
    uint8_t *addr = (uint8_t *)WEEKLY_LOG_ADDRESS_IN_FLASH;
    msp430Flash_erase_segment(addr);
    msp430Flash_erase_segment(addr + FLASH_BLOCK_SIZE);
}

/**
* \brief Advance to the next weekly log container (rollover if 
*        required).  Erase the identified weekly log container.
*/
static void prepareNextWeeklyLog(void) {
    eraseWeeklyLog();
}

/**
* \brief Update the packet header portion of the daily log based
*        on beginning-of-day info.
*/
static void prepareDailyLog(void) {
    msgHeader_t *msgHeaderP = getDailyHeaderAddr(stData.storageTime_dayOfWeek);
    timePacket_t tp;
    uint8_t temp8;

    // Get current time
    getBinTime(&tp);

    // Payload start byte
    temp8 = 0x1;
    msp430Flash_write_bytes((uint8_t *)&(msgHeaderP->payloadStartByte), &temp8, FLASH_WRITE_ONE_BYTE);

    // Payload Message Type
    temp8 = MSG_TYPE_DAILY_LOG;
    msp430Flash_write_bytes((uint8_t *)&(msgHeaderP->payloadMsgId), &temp8, FLASH_WRITE_ONE_BYTE);

    // Product ID
    temp8 = OUTPOUR_PRODUCT_ID;
    msp430Flash_write_bytes((uint8_t *)&(msgHeaderP->productId), &temp8, FLASH_WRITE_ONE_BYTE);

    // Time
    temp8 = 0;
    msp430Flash_write_bytes((uint8_t *)&(msgHeaderP->GMTsecond), &temp8,     FLASH_WRITE_ONE_BYTE);
    msp430Flash_write_bytes((uint8_t *)&(msgHeaderP->GMTminute), &temp8,     FLASH_WRITE_ONE_BYTE);
    msp430Flash_write_bytes((uint8_t *)&(msgHeaderP->GMThour),   &temp8,     FLASH_WRITE_ONE_BYTE);
    msp430Flash_write_bytes((uint8_t *)&(msgHeaderP->GMTday),    &tp.day,    FLASH_WRITE_ONE_BYTE);
    msp430Flash_write_bytes((uint8_t *)&(msgHeaderP->GMTmonth),  &tp.month,  FLASH_WRITE_ONE_BYTE);
    msp430Flash_write_bytes((uint8_t *)&(msgHeaderP->GMTyear),   &tp.year,   FLASH_WRITE_ONE_BYTE);

    // FW Version
    temp8 = FW_VERSION_MAJOR;
    msp430Flash_write_bytes((uint8_t *)&(msgHeaderP->fwMajor), &temp8, FLASH_WRITE_ONE_BYTE);
    temp8 = FW_VERSION_MINOR;
    msp430Flash_write_bytes((uint8_t *)&(msgHeaderP->fwMinor), &temp8, FLASH_WRITE_ONE_BYTE);

    // Days Activated
    msp430Flash_write_int16((uint8_t *)&(msgHeaderP->daysActivatedMsb), stData.daysActivated);

    // Storage Week Number
    temp8 = stData.storageTime_week;
    msp430Flash_write_bytes((uint8_t *)&(msgHeaderP->storageWeek), &temp8, FLASH_WRITE_ONE_BYTE);

    // Storage day of the week
    temp8 = stData.storageTime_dayOfWeek;
    msp430Flash_write_bytes((uint8_t *)&(msgHeaderP->storageDay), &temp8, FLASH_WRITE_ONE_BYTE);

    // Unused
    temp8 = 0xA5;
    msp430Flash_write_bytes((uint8_t *)&(msgHeaderP->reserve1), &temp8, FLASH_WRITE_ONE_BYTE);
}

/**
* \brief Updates the record for tracking that a daily log is
*        ready for transmit.
* 
* @param dayOfTheWeek The day of the week to record
*/
static void markDailyLogAsReady(uint8_t dayOfTheWeek) {
    uint8_t zeroVal = 0;
    weeklyLog_t *wlP = WEEKLY_LOG_ADDRESS_IN_FLASH;
    if (dayOfTheWeek >= TOTAL_DAYS_IN_A_WEEK) {
        return;
    }
    uint8_t *entryP = &(wlP->clearOnReady[dayOfTheWeek]);
    msp430Flash_write_bytes(entryP, &zeroVal, FLASH_WRITE_ONE_BYTE);
}

/**
* \brief Utility function to check if a daily log is ready for
*        transmit.
* 
* @param dayOfTheWeek The day of the week to record
*/
static bool isDailyLogReady(uint8_t dayOfTheWeek) {
    weeklyLog_t *wlP = WEEKLY_LOG_ADDRESS_IN_FLASH;
    bool isReady = !wlP->clearOnReady[dayOfTheWeek];
    return isReady;
}

/**
* \brief Updates the record for tracking that a daily log has 
*        been transmitted.
* 
* @param dayOfTheWeek The day of the week to record
*/
static void markDailyLogAsTransmitted(uint8_t dayOfTheWeek) {
    uint8_t zeroVal = 0;
    weeklyLog_t *wlP = WEEKLY_LOG_ADDRESS_IN_FLASH;
    uint8_t *entryP = (uint8_t *)&(wlP->clearOnTransmit[dayOfTheWeek]);
    if (dayOfTheWeek >= TOTAL_DAYS_IN_A_WEEK) {
        return;
    }
    msp430Flash_write_bytes(entryP, &zeroVal, FLASH_WRITE_ONE_BYTE);
}

/**
* \brief Utility function to check if a daily log has been 
*        transmitted.
* 
* @param dayOfTheWeek The day of the week to record
*/
static bool wasDailyLogTransmitted(uint8_t dayOfTheWeek) {
    weeklyLog_t *wlP = WEEKLY_LOG_ADDRESS_IN_FLASH;
    // If zero, it means we transmitted packet.
    return (wlP->clearOnTransmit[dayOfTheWeek] ? false : true);
}

/**
* \brief clear stats that are reset when the storage clock is 
*        restarted/aligned.
*/
static void clearAlignStats(void) {
    stData.storageTime_seconds = 0;
    stData.storageTime_minutes = 0;
    stData.storageTime_hours = 0;
    stData.storageTime_dayOfWeek = 0;
    stData.storageTime_week = 0;
    stData.minuteMilliliterSum = 0;
    stData.hourMilliliterSum = 0;
    stData.dayMilliliterSum = 0;
}

