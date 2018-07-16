/** 
 * @file outpour.h
 * \n Source File
 * \n Outpour MSP430 Firmware
 * 
 * \brief MSP430 sleep control and support routines
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "msp430.h"
#include "msp430g2553.h"
#include "RTC_Calendar.h"
#include "modemMsg.h"

/******************************************************************************/
/** Water Detection MACROS **/

/**
 * \def NEW_PROCESSOR_IN_USE
 * For the AfridevV1 configuration always define this.
 */
#define NEW_PROCESSOR_IN_USE

/**
 * \def WATERDETECT_READ_WATER_LEVEL_NORMAL
 * Used to identify that the sensor is mounted in a well in its 
 * normal orientation.  For testing and development, this may 
 * not be the case. 
 */
#define WATERDETECT_READ_WATER_LEVEL_NORMAL

/**
 * \def TICKS_PER_TREND
 * Controls how many capacitance measurements to take before 
 * running the water detection algorithm.  One capacitance 
 * measurement is taken on each iteration of the main exec loop. 
 * The main exec loop runs at a rate of (TICKS_PER_TREND / 
 * SECONDS_PER_TREND) hertz (i.e. 2 Hz). 
 */
#define TICKS_PER_TREND 4

/**
 * \def SECONDS_PER_TREND
 * Controls how often (in seconds) to execute the water 
 * detection algorithm. The storage and modem communication 
 * tasks also run at this periodic rate within the main exec 
 * loop. 
 */
#define SECONDS_PER_TREND 2

/**
 * \def SECONDS_PER_TREND_SHIFT
 * Represents the shift to perform a divide of 
 * SECONDS_PER_TREND. 
 */
#define SECONDS_PER_TREND_SHIFT 1

/**
 * \def TIMER_INTERRUPTS_PER_SECOND
 * Identifies how often the system timer interrupts per second. 
 * The main exec loop wakes up from low power mode after each 
 * timer interrupt. 
 */
#define TIMER_INTERRUPTS_PER_SECOND 2

/******************************************************************************/

/**
 * \def OUTPOUR_PRODUCT_ID
 * \brief Specify the outpour product ID number that is sent in 
 *        messages.
 */
#define OUTPOUR_PRODUCT_ID ((uint8_t)0)

/**
 * \def FW_VERSION_MAJOR
 * \brief Specify the outpour firmware major version number.
 */
#define FW_VERSION_MAJOR ((uint8_t)0x03)

/**
 * \def FW_VERSION_MINOR
 * \brief Specify the outpour firmware minor version number.
 */
#define FW_VERSION_MINOR ((uint8_t)0x05)

/**
 * \def ACTIVATE_REBOOT_KEY
 * \brief A non-zero value indicating that we need to start a 
 *        system reboot sequence.
 */
#define ACTIVATE_REBOOT_KEY 0xC3

/**
 * \def ACTIVATE_FWUPGRADE_KEY 
 * \brief A non-zero value indicating that we need to start a 
 *        firmware upgrade sequence.  This means re-booting into
 *        the boot loader.
 */
#define ACTIVATE_FWUPGRADE_KEY 0xE7

/*******************************************************************************
* System Tick Access
*******************************************************************************/
// Define the type that a system tick value is represented in
typedef uint32_t sys_tick_t;
// Just return the system tick variable value.
#define GET_SYSTEM_TICK() ((sys_tick_t)getSecondsSinceBoot());
// Return the number of elapsed seconds
#define GET_ELAPSED_TIME_IN_SEC(x) (((sys_tick_t)getSecondsSinceBoot())-(sys_tick_t)(x))

// Used for testing if running the sys tick at faster then
// the standard interval. For normal operation, must be set to 1.
#define TIME_SCALER ((uint8_t)1)

/*******************************************************************************
* MISC Macros
*******************************************************************************/

/**
 * \def TIME_5_SECONDS 
 * \brief Macro for 5 seconds
 */
#define TIME_5_SECONDS ((uint8_t)5)
/**
 * \def TIME_10_SECONDS 
 * \brief Macro for 10 seconds
 */
#define TIME_10_SECONDS ((uint8_t)10)
/**
 * \def TIME_20_SECONDS 
 * \brief Macro for 20 seconds
 */
#define TIME_20_SECONDS ((uint8_t)20)
/**
 * \def TIME_30_SECONDS 
 * \brief Macro for 30 seconds
 */
#define TIME_30_SECONDS ((uint8_t)30)
/**
 * \def TIME_60_SECONDS 
 * \brief Macro for 60 seconds
 */
#define TIME_60_SECONDS ((uint8_t)60)
/**
 * \def SECONDS_PER_MINUTE
 * \brief Macro to specify seconds per minute
 */
#define SECONDS_PER_MINUTE ((uint8_t)60)
/**
 * \def SECONDS_PER_HOUR
 * \brief Macro to specify seconds per hour
 */
#define SECONDS_PER_HOUR (SECONDS_PER_MINUTE*((uint16_t)60))
/**
 * \def TIME_ONE_HOUR
 * \brief Macro to specify one hour in terms of seconds
 */
#define TIME_ONE_HOUR SECONDS_PER_HOUR
/**
 * \def SECONDS_PER_DAY
 * \brief Macro to specify seconds per day
 */
#define SECONDS_PER_DAY ((uint32_t)86400)
/**
 * \def SEC_PER_MINUTE
 * \brief Macro to specify the number of seconds in one minute
 */
#define SEC_PER_MINUTE ((uint8_t)60)
/**
 * \def TIME_45_MINUTES
 * \brief Macro to specify the number of seconds in 45 minutes
 */
#define TIME_45_MINUTES ((uint16_t)(SEC_PER_MINUTE*(uint16_t)45))
/**
 * \def TIME_60_MINUTES
 * \brief Macro to specify the number of seconds in 60 minutes
 */
#define TIME_60_MINUTES ((uint16_t)(SEC_PER_MINUTE*(uint16_t)60))
/**
 * \def TIME_5_MINUTES
 * \brief Macro to specify the number of seconds in 5 minutes
 */
#define TIME_5_MINUTES ((uint16_t)(SEC_PER_MINUTE*(uint16_t)5))
/**
 * \def TIME_10_MINUTES
 * \brief Macro to specify the number of seconds in 10 minutes
 */
#define TIME_10_MINUTES ((uint16_t)(SEC_PER_MINUTE*(uint16_t)10))
/**
 * \def TIME_20_MINUTES
 * \brief Macro to specify the number of seconds in 10 minutes
 */
#define TIME_20_MINUTES ((uint16_t)(SEC_PER_MINUTE*(uint16_t)20))

/**
 * \typedef padId_t
 * \brief Give each pad number a name
 */
typedef enum padId_e {
    PAD0,        /** 0 */
    PAD1,        /** 1 */
    PAD2,        /** 2 */
    PAD3,        /** 3 */
    PAD4,        /** 4 */
    PAD5,        /** 5 */
    TOTAL_PADS = 6,     /** there are 6 total pads */
    MAX_PAD = PAD5,     /** PAD5 is the max pad value */
} padId_t;

#define LS_VCC BIT0
#define RXD BIT1
#define TXD BIT2
#define GSM_STATUS BIT4
#define GSM_INT BIT5
#define GSM_EN BIT6
#define GSM_DCDC BIT7

/*******************************************************************************
*  Centralized method for enabling and disabling MSP430 interrupts
*******************************************************************************/
static inline void enableGlobalInterrupt(void) {
    _BIS_SR(GIE);
}
static inline void disableGlobalInterrupt(void) {
    _BIC_SR(GIE);
}
static inline void enableSysTimerInterrupt(void) {
    TA1CCTL0 |= CCIE;
}
static inline void disableSysTimerInterrupt(void) {
    TA1CCTL0 &= ~CCIE;
}
static inline void restoreSysTimerInterrupt(uint16_t val) {
    TA1CCTL0 &= ~CCIE; // clear the value
    TA1CCTL0 |= val;   // set to val
}
static inline uint16_t getAndDisableSysTimerInterrupt(void) {
    volatile uint16_t current = TA1CCTL0; // read reg
    current  &= CCIE;  // get current interrupt setting
    TA1CCTL0 &= ~CCIE; // disable interrupt
    return current;    // return interrupt setting
}

/*******************************************************************************
*  Polling Delays
*******************************************************************************/
void secDelay(uint8_t secCount);
void ms1Delay(uint8_t msCount);
void us10Delay(uint8_t us10);

/*******************************************************************************
* sysExec.c
*******************************************************************************/
/**
 * \def REBOOT_KEY1
 * \def REBOOT_KEY2
 * \def REBOOT_KEY3
 * \def REBOOT_KEY4
 * \brief These keys are used to validate the OTA reset command.
 */
#define REBOOT_KEY1 ((uint8_t)0xAA)
#define REBOOT_KEY2 ((uint8_t)0x55)
#define REBOOT_KEY3 ((uint8_t)0xCC)
#define REBOOT_KEY4 ((uint8_t)0x33)

void sysExec_exec(void);
bool sysExec_startRebootCountdown(uint8_t activateReboot);
void sysError(void);

/*******************************************************************************
* Utils.c
*******************************************************************************/
typedef struct timeCompare_s {
    uint8_t hoursA;
    uint8_t minutesA;
    uint8_t secondsA;
    uint8_t hoursB;
    uint8_t minutesB;
    uint8_t secondsB;
    uint32_t timeDiffInSeconds;
} timeCompare_t;

unsigned int gen_crc16(const unsigned char *data, unsigned int size);
unsigned int gen_crc16_2buf(const unsigned char *data1, unsigned int size1, const unsigned char *data2, unsigned int size2);
uint32_t timeInSeconds(uint8_t hours, uint8_t minutes, uint8_t seconds);
void calcTimeDiffInSeconds(timeCompare_t *timeCompareP);
void initApplicationRecord(void);
bool checkForApplicationRecord(void);

/*******************************************************************************
* modemCmd.h
*******************************************************************************/

/**
 * \typedef modemCmdWriteData_t 
 * \brief Container to pass parmaters to the modem command write 
 *        function.
 */
typedef struct modemCmdWriteData_s {
    outpour_modem_command_t cmd; /**< the modem command */
    MessageType_t payloadMsgId;  /**< the payload type (Cascade message type) */
    uint8_t *payloadP;           /**< the payload pointer (if any) */
    uint16_t payloadLength;      /**< size of the payload in bytes */
    uint16_t payloadOffset;      /**< for receiving partial data */
    bool statusOnly;             /**< only perform status retrieve from modem - no cmd */
} modemCmdWriteData_t;

/**
 * \typedef modemCmdReadData_t 
 * \brief Container to read the raw response returned from the 
 *        modem as a result of sending it a command. 
 */
typedef struct modemCmdReadData_s {
    modem_command_t modemCmdId;    /**< the cmd we are sending to the modem */
    bool valid;                    /**< indicates that the response is correct (crc passed, etc) */
    uint8_t *dataP;                /**< the pointer to the raw buffer */
    uint16_t lengthInBytes;        /**< the length of the data in the buffer */
} modemCmdReadData_t;

/**
 * \def OTA_PAYLOAD_MAX_READ_LENGTH
 * \brief Specify the max length of space allowed in the RX ISR
 *        Buffer for OTA message payload data. This length is in
 *        addition to the message header overhead which is read
 *        into the RX ISR buffer as well. Note that this max
 *        length only applies to the data portion received via
 *        the M_COMMAND_GET_INCOMING_PARTIAL command from the
 *        Modem. If the OTA data length is larger than the
 *        allowed max length, than an iterative approach is used
 *        to read the data from the modem in chunks. The only
 *        case where the iterative approach is needed is for a
 *        firmware upgrade.  All other OTA payload data length
 *        is currently less than 16 bytes.
 */
#define OTA_PAYLOAD_MAX_RX_READ_LENGTH ((uint16_t)16)

/**
 * \def OTA_RESPONSE_LENGTH
 * \brief Specify the total size of an OTA response.  This is 
 *        the length of an OTA response message being sent to
 *        the cloud by the unit. It is a constant value. It
 *        consists of the header and the data.  The header is
 *        always 16 bytes and the data is always 32 bytes -
 *        regardless of whether it is all used by the message
 *        sent.  Example OTA Response messages include
 *        OTA_OPCODE_GMT_CLOCKSET, OTA_OPCODE_LOCAL_OFFSET, etc.
 * \note An OTA Response should not be confused with a data 
 *       message (also sent to the cloud). Data messages are not
 *       a constant length.  Examples include
 *       MSG_TYPE_DAILY_LOG, MSG_TYPE_CHECKIN, etc.
 */
#define OTA_RESPONSE_LENGTH (OTA_RESPONSE_HEADER_LENGTH+OTA_RESPONSE_DATA_LENGTH)

/**
 * \def OTA_RESPONSE_HEADER_LENGTH
 * \brief Define the header length of an OTA response message 
 */
#define OTA_RESPONSE_HEADER_LENGTH ((uint8_t)16)

/**
 * \def OTA_RESPONSE_DATA_LENGTH
 * \brief Define the data length of an OTA response message. The
 *        data follows the header in the message.
 */
#define OTA_RESPONSE_DATA_LENGTH ((uint8_t)32)

/**
 * \typedef otaResponse_t
 * \brief Define a container to hold a partial OTA response.
 */
typedef struct otaResponse_s {
    uint8_t *buf;                         /**< A buffer to hold one OTA message */
    uint16_t lengthInBytes;               /**< how much valid data is in the buf */
    uint16_t remainingInBytes;            /**< how much remaining of the total OTA */
}otaResponse_t;

void modemCmd_exec(void);
void modemCmd_init(void);
bool modemCmd_write(modemCmdWriteData_t *writeCmdP);
void modemCmd_read(modemCmdReadData_t *readDataP);
bool modemCmd_isResponseReady(void);
bool modemCmd_isError(void);
bool modemCmd_isBusy(void);

/*******************************************************************************
* modemPower.c
*******************************************************************************/
void modemPower_exec(void);
void modemPower_init(void);
void modemPower_restart(void);
void modemPower_powerDownModem(void);
bool modemPower_isModemOn(void);
uint16_t modemPower_getModemOnTimeInSecs(void);
bool modemPower_isModemOnError(void);

/*******************************************************************************
* modemMgr.c
*******************************************************************************/

/**
 * \typedef msgHeader_t
 * \brief Define the structure of the header that sits on top of
 *        of all outbound messages.
 */
typedef struct __attribute__((__packed__))msgHeader_s {
    uint8_t payloadStartByte;       /**< 0 */
    uint8_t payloadMsgId;           /**< 1 */
    uint8_t productId;              /**< 2 */
    uint8_t GMTsecond;              /**< 3 */
    uint8_t GMTminute;              /**< 4 */
    uint8_t GMThour;                /**< 5 */
    uint8_t GMTday;                 /**< 6 */
    uint8_t GMTmonth;               /**< 7 */
    uint8_t GMTyear;                /**< 8 */
    uint8_t fwMajor;                /**< 9 */
    uint8_t fwMinor;                /**< 10 */
    uint8_t daysActivatedMsb;       /**< 11 */
    uint8_t daysActivatedLsb;       /**< 12 */
    uint8_t storageWeek;            /**< 14 */
    uint8_t storageDay;             /**< 13 */
    uint8_t reserve1;               /**< 15 */
} msgHeader_t;

void modemMgr_exec(void);
void modemMgr_init(void);
bool modemMgr_grab(void);
bool modemMgr_isModemUp(void);
bool modemMgr_isModemUpError(void);
void modemMgr_sendModemCmdBatch(modemCmdWriteData_t *cmdWriteP);
void modemMgr_stopModemCmdBatch(void);
bool modemMgr_isModemCmdComplete(void);
bool modemMgr_isModemCmdError(void);
void modemMgrRelease(void);
void modemMgr_restartModem(void);
bool modemMgr_isAllocated(void);
void modemMgr_release(void);
bool modemMgr_isReleaseComplete(void);
otaResponse_t* modemMgr_getLastOtaResponse(void);
bool modemMgr_isLinkUp(void);
bool modemMgr_isLinkUpError(void);
uint8_t modemMgr_getNumOtaMsgsPending(void);
uint16_t modemMgr_getSizeOfOtaMsgsPending(void);
uint8_t* modemMgr_getSharedBuffer(void);

/*******************************************************************************
* msgData.c
*******************************************************************************/
void dataMsgMgr_exec(void);
void dataMsgMgr_init(void);
bool dataMsgMgr_isSendMsgActive(void);
bool dataMsgMgr_sendDataMsg(MessageType_t msgId, uint8_t *dataP, uint16_t lengthInBytes);
bool dataMsgMgr_sendDailyLogs(void);

/*******************************************************************************
* msgOta.c
*******************************************************************************/
void otaMsgMgr_exec(void);
void otaMsgMgr_init(void);
void otaMsgMgr_getAndProcessOtaMsgs(void);
void otaMsgMgr_stopOtaProcessing(void);
bool otaMsgMgr_isOtaProcessingDone(void);

/*******************************************************************************
* msgDebug.c
*******************************************************************************/

void dbgMsgMgr_sendDebugMsg(MessageType_t msgId, uint8_t *dataP, uint16_t lengthInBytes);

/*******************************************************************************
* msgData.c
*******************************************************************************/
/**
 * \typedef dataMsgState_t
 * \brief Specify the states for sending a data msg to the 
 *        modem.
 */
typedef enum dataMsgState_e {
    DMSG_STATE_IDLE,
    DMSG_STATE_GRAB,
    DMSG_STATE_WAIT_FOR_MODEM_UP,
    DMSG_STATE_SEND_MSG,
    DMSG_STATE_SEND_MSG_WAIT,
    DMSG_STATE_WAIT_FOR_LINK,
    DMSG_STATE_PROCESS_OTA,
    DMSG_STATE_PROCESS_OTA_WAIT,
    DMSG_STATE_RELEASE,
    DMSG_STATE_RELEASE_WAIT,
} dataMsgState_t;

/**
 * \typedef dataMsgSm_t
 * \brief Define a contiainer to hold the information needed by 
 *        the data message module to perform sending a data
 *        command to the modem.
 *  
 * \note To save memory, this object can potentially be a common
 *       object that all clients use because only one client
 *       will be using the modem at a time?
 */
typedef struct dataMsgSm_s {
    dataMsgState_t dataMsgState;  /**< current data message state */
    modemCmdWriteData_t cmdWrite; /**< the command info object */
    uint8_t modemResetCount;      /**< for error recovery, count times modem is power cycled */
    bool sendCmdDone;             /**< flag to indicate sending the current command to modem is complete */
    bool allDone;                 /**< flag to indicate send session is complete and modem is off */
    bool connectTimeout;          /**< flag to indicate the modem was not able to connect to the network */
    bool commError;               /**< flag to indicate an modem UART comm error occured - not currently used - can remove */
} dataMsgSm_t;

void dataMsgSm_init(void);
void dataMsgSm_initForNewSession(dataMsgSm_t *dataMsgP);
void dataMsgSm_sendAnotherDataMsg(dataMsgSm_t *dataMsgP);
void dataMsgSm_stateMachine(dataMsgSm_t *dataMsgP);

/*******************************************************************************
* msgFinalAssembly.c
*******************************************************************************/
void fassMsgMgr_exec(void);
void fassMsgMgr_init(void);
void fassMsgMgr_sendFassMsg(void);

/*******************************************************************************
* time.c
*******************************************************************************/
/**
 * \typedef timePacket_t 
 * \brief Specify a structure to hold time data that will be 
 *        sent as part of the final assembly message.
 */
typedef struct  __attribute__((__packed__))timePacket_s {
    uint8_t second;
    uint8_t minute;
    uint8_t hour24;
    uint8_t day;
    uint8_t month;
    uint8_t year;
} timePacket_t;

void timerA1_init(void);
void getBinTime(timePacket_t *tpP);
uint8_t bcd_to_char(uint8_t bcdValue);
uint32_t getSecondsSinceBoot(void);

/*******************************************************************************
* storage.c
*******************************************************************************/
void storageMgr_init(void);
void storageMgr_exec(uint16_t currentFlowRateInSecML);
void storageMgr_overrideUnitActivation(bool flag);
uint16_t storageMgr_getDaysActivated(void);
void storageMgr_resetWeeklyLogs(void);
void storageMgr_setStorageAlignmentTime(uint8_t alignSecond, uint8_t alignMinute, uint8_t alignHour24);
void storageMgr_setTransmissionRate(uint8_t transmissionRateInDays);
uint16_t storageMgr_getNextDailyLogToTransmit(uint8_t **dataPP);
void storageMgr_sendDebugDataToUart(void);
uint8_t storageMgr_getStorageClockInfo(uint8_t *bufP);
uint8_t storageMgr_prepareMsgHeader(uint8_t *dataPtr, uint8_t payloadMsgId);

/*******************************************************************************
* waterSense.c
*******************************************************************************/
void waterSense_init(void);
void waterSense_exec(void);
bool waterSense_isInstalled(void);
uint16_t waterSense_getLastMeasFlowRateInML(void);
void waterSense_clearStats(void);
uint16_t waterSense_getPadStatsMax(padId_t padId);
uint16_t waterSense_padStatsMin(padId_t padId);
uint16_t waterSense_getPadStatsSubmerged(padId_t padId);
uint16_t waterSense_getPadStatsUnknowns(void);
uint16_t padStats_zeros(void);
int16_t waterSense_getTempCelcius(void);
void waterSense_writeConstants(uint8_t *dataP);
void waterSense_sendDebugDataToUart(void);

/*******************************************************************************
* hal.c
*******************************************************************************/
void hal_sysClockInit(void);
void hal_uartInit(void);
void hal_pinInit(void);

/*******************************************************************************
* flash.c
*******************************************************************************/
void msp430Flash_erase_segment(uint8_t *flashSectorAddrP);
void msp430Flash_write_bytes(uint8_t *flashP, uint8_t *srcP, uint16_t num_bytes);
void msp430Flash_write_int16(uint8_t *flashP, uint16_t val16);
void msp430Flash_write_int32(uint8_t *flashP, uint32_t val32);

/*******************************************************************************
* For Firmware Upgrade Support
*******************************************************************************/
/**
 * \def FLASH_UPGRADE_KEY1
 * \def FLASH_UPGRADE_KEY2
 * \def FLASH_UPGRADE_KEY3
 * \def FLASH_UPGRADE_KEY4
 * \brief These keys are used to validate the OTA firmware
 *        upgrade command.
 */
#define FLASH_UPGRADE_KEY1 ((uint8_t)0x31)
#define FLASH_UPGRADE_KEY2 ((uint8_t)0x41)
#define FLASH_UPGRADE_KEY3 ((uint8_t)0x59)
#define FLASH_UPGRADE_KEY4 ((uint8_t)0x26)

#define APR_LOCATION ((uint8_t *)0x1040)  // INFO C
#define APR_MAGIC1 ((uint16_t)0x1234)
#define APR_MAGIC2 ((uint16_t)0x5678)

/**
 * \typedef appRecord_t
 * \brief This structure is used to put an application record in 
 *        one of the INFO sections.  The structure is used to
 *        tell the bootloader that application has started.
 */
typedef struct appRecord_e {
    uint16_t magic1;
    uint16_t magic2;
    uint16_t crc16;
} appRecord_t;

