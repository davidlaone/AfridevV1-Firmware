/** 
 * @file sysExec.c
 * \n Source File
 * \n Outpour MSP430 Firmware
 * 
 * \brief main system exec that runs the top level loop.
 */

#include "outpour.h"
#include "waterDetect.h"
#include "waterSense.h"

/***************************
 * Module Data Definitions
 **************************/

/**
 * \def START_UP_MSG_TX_DELAY_IN_SECONDS
 * \brief Two messages are transmitted after the system starts. 
 *        This definition specifies the delay in seconds after
 *        startup until the first startup message is sent to the
 *        modem for transmit. After the first startup message
 *        has completed transmitting, the same delay is used to
 *        determine when to transmit the second startup message.
 */
#define START_UP_MSG_TX_DELAY_IN_SECONDS ((uint8_t)30)

/**
 * \def REBOOT_DELAY_IN_SECONDS
 * \brief This define is used in conjunction with the OTA reset
 *        device message. It specifies how long to wait after
 *        the message was received to perform the MSP430 reset.
 */
#define REBOOT_DELAY_IN_SECONDS ((uint8_t)20*TIME_SCALER)

/**
 * \def NO_WATER_HF_TO_LF_TIME_IN_SECONDS
 * \brief  Specify how long to wait before switching to
 *         the low-frequency (LF) water measurement interval in
 *         order to save power. By default, the system is in the
 *         high-frequency (HF) water measurement interval where
 *         water measurements are take every main loop
 *         iterataion. If water is detected while in the LF
 *         mode, the system immediately switches back to HF
 *         mode.
 */
#define NO_WATER_HF_TO_LF_TIME_IN_SECONDS ((uint16_t)TIME_60_SECONDS*5)

/**
 * \def LOW_FREQUENCY_MEAS_TIME_IN_SECONDS
 * \brief Specify how often to take the batch water measurements
 *        when in the low-frequency water measurement mode. The
 *        system transitions to the low-frequency interval if no
 *        water has been detected over a
 *        NO_WATER_HF_TO_LF_TIME_IN_SECONDS window of time.
 */
#define LOW_FREQUENCY_MEAS_TIME_IN_SECONDS ((uint8_t)TIME_60_SECONDS)

/**
 * \def WATER_LF_MEAS_BATCH_COUNT
 * This controls the number of water measurements taken per 
 * LOW_FREQUENCY_MEAS_TIME_IN_SECONDS while in the low-frequency 
 * water measurement mode.
 */
#define WATER_LF_MEAS_BATCH_COUNT 4

/**
 * \typedef sysExecData_t
 * \brief Container to hold data for the sysExec module.
 */
typedef struct sysExecData_s {
    bool startUpMsg1WasSent : 1;      /**< Flag specifying if first startup msg was sent */
    bool startUpMsg2WasSent : 1;      /**< Flag specifying if second startup msg was sent */
    int8_t secondsTillStartUpMsgTx;   /**< Seconds until startup messages are transmitted */
    int8_t secondsTillReboot;         /**< How long to wait to perform a MSP430 reset */
    uint8_t rebootCountdownIsActive;  /**< Specify if a reboot countdown sequence is in-progress */
    uint8_t noWaterMeasCount;         /**< Maintain a count of sequential no water detected measurements */
    uint8_t waterMeasDelayCount;      /**< Delay next water measurement count-down */
} sysExecData_t;

/****************************
 * Module Data Declarations
 ***************************/

sysExecData_t sysExecData;

/*************************
 * Module Prototypes
 ************************/
static uint16_t analyzeWaterMeasurementData(void);
static void startUpMessageCheck(void);
static void sendStartUpMsg1(void);
static void sendStartUpMsg2(void);
static void sysExec_doReboot(void);

#ifdef SEND_DEBUG_INFO_TO_UART
static void sysExec_sendDebugDataToUart(void);
#endif

/***************************
 * Module Public Functions
 **************************/

/**
* \brief This function contains the main software loop
*        for the MSP430 firmware.  After calling the
*        initialization routines, it drops into an infinite
*        while loop.  In the while loop, it goes into a low
*        power mode (i.e. sleep) waiting for the timer interrupt
*        to wake it up. The timer interrupt occurs once every .5
*        seconds. Once awake, it performs a water capacitance
*        measurement. Every 4th time through the main loop, it
*        performs the water detection algorithm and system tasks
*        by calling the exec routines of the different
*        sub-modules.
*
* \ingroup EXEC_ROUTINE
*/
void sysExec_exec(void) {

    uint8_t exec_main_loop_counter = 0;

    memset(&sysExecData, 0, sizeof(sysExecData_t));

    // Set how long to wait until first startup message should be transmitted
    sysExecData.secondsTillStartUpMsgTx = START_UP_MSG_TX_DELAY_IN_SECONDS;

    // Initialize the date for Jan 1, 2017
    // h, m, s, am/pm (must be in BCD)
    setTime(0x00, 0x00, 0x00, 0x00);
    // y, m, d
    setDate(2017, 1, 1);

    // Call the module init routines
    modemPower_init();
    modemCmd_init();
    modemMgr_init();
    dataMsgSm_init();
    dataMsgMgr_init();
    otaMsgMgr_init();
    waterSense_init();
    storageMgr_init();

    // Start the timer interrupt
    timerA1_init();

    // Enable the global interrupt
    enableGlobalInterrupt();

    // Start the infinite exec main loop
    while (1) {

        // Go into low power mode.
        // Wake on the exit of the TimerA0 interrupt.
        // Wakes every .5 seconds to run the main loop
        __bis_SR_register(LPM3_bits);

        // Take a water measurement
        // Don't take a measurement if the modem is in use or the water
        // measurement delay count is above the batch count. The waterMeasDelay
        // count is always zero in HF water measurement mode. In LF water measurement
        // mode, the counter is used to control how often the water measurements are
        // performed. In LF water measurement mode, we want to take a batch of
        // measurements periodically.
        if ((sysExecData.waterMeasDelayCount < WATER_LF_MEAS_BATCH_COUNT) && !modemMgr_isAllocated()) {
            waterSense_takeReading();
        }

        // Increment main loop counter
        exec_main_loop_counter++;

        // Perform system tasks every SECONDS_PER_TREND (i.e. every 2 seconds)
        // which is every fourth time that the exec main loop runs.
        if (exec_main_loop_counter >= TICKS_PER_TREND) {

            uint16_t currentFlowRateInMLPerSec = 0;

            // zero the main loop counter
            exec_main_loop_counter = 0;

            // Don't perform water data analysis if modem is in use.
            if (!modemMgr_isAllocated()) {
                currentFlowRateInMLPerSec = analyzeWaterMeasurementData();
            }

            // Record the water stats and initiate periodic communication
            storageMgr_exec(currentFlowRateInMLPerSec);

            // Perform communication support - these run the state machines
            // that perform the software modem interaction.  They do not
            // initiate modem communication, but once communication is started
            // they handle all aspects of the modem interfacing.
            modemCmd_exec();     /* perform Low-level modem interface processing */
            dataMsgMgr_exec();   /* perform High-level send data message */
            otaMsgMgr_exec();    /* perform High-level OTA Message Processing */
            modemMgr_exec();     /* perform Low-level message processing */
            modemCmd_exec();     /* perform Low-level modem interface processing (again) */
            modemPower_exec();   /* Handle powering on and off the modem */

            // A system reboot sequence is started when a firmware upgrade message
            // or system restart message is received. If a reboot sequence has started,
            // then decrement counter and check for a timeout.  When timeout occurs,
            // perform a reboot.
            if (sysExecData.rebootCountdownIsActive == ACTIVATE_REBOOT_KEY) {
                if (sysExecData.secondsTillReboot >= 0) {
                    sysExecData.secondsTillReboot -= SECONDS_PER_TREND;
                }
                if (sysExecData.secondsTillReboot <= 0) {
                    sysExec_doReboot();
                }
            }

            // Two messages are transmitted shortly after the system starts:
            // The final assembly message and a monthly check-in message.
            if (!sysExecData.startUpMsg1WasSent || !sysExecData.startUpMsg2WasSent) {
                startUpMessageCheck();
            }

#ifdef SEND_DEBUG_INFO_TO_UART
            // Only send debug data if the modem is not in use.
            if (!modemMgr_isAllocated()) {
                sysExec_sendDebugDataToUart();
            }
#endif
        }
    }
}

/***************************
 * Module Private Functions
 **************************/

/**
 * \brief Call the water data analysis routine. Check if the 
 *        system should move to the low-frequency (LF)
 *        measurement mode in order to save power. This
 *        happens if no water has been detected for a certain
 *        amount of time. By default, the system is in the
 *        high-frequency (HF) measurement mode.  When in LF
 *        measurement mode, the water analysis function is
 *        not performed on every call. When in HF measurement
 *        mode, it is.
 * 
 * @return uint16_t Returns the current flow rate measured in 
 *         milliliters per second.
 */
static uint16_t analyzeWaterMeasurementData(void) {
    uint16_t currentFlowRateInMLPerSec = 0;
    bool waterDetectedFlag = false;
    bool didWaterAnalysisFlag = false;

    // When in LF measurement mode, a batch of water data analysis calls will be
    // performed  before deciding if the LF measurement mode should continue. If
    // in HF measurement mode, then waterMeasDelayCount will be zero and the water
    // analysis is performed on every call. If water is detected on any measurement,
    // then the system will always return to HF measurement mode.

    // In LF water measurement mode, take a batch of measurements periodically.
    // In HF water measurement mode, the waterMeasDelayCount will always be zero.
    if (sysExecData.waterMeasDelayCount < WATER_LF_MEAS_BATCH_COUNT) {

        // Perform algorithm to analyze water data samples.
        waterSense_analyzeData();

        // Read the flow rate in milliliters per second
        currentFlowRateInMLPerSec = waterSense_getLastMeasFlowRateInML();

        if (currentFlowRateInMLPerSec) {
            waterDetectedFlag = true;
        }
        didWaterAnalysisFlag = true;
    }

    if (didWaterAnalysisFlag && waterDetectedFlag) {
        // If water flow is detected, then clear all LF water measurement counters.
        sysExecData.noWaterMeasCount = 0;
        sysExecData.waterMeasDelayCount = 0;
    }

    // Perform checks to determine if system should go into LF measurement mode.
    else {

        // Once the noWaterMeasCount reaches the timeout count, it will no
        // longer change until water has been detected, keeping the water
        // measurement interval in the LF mode.

        // For the compare, the NO_WATER_HF_TO_LF_TIME_IN_SECONDS is divided by
        // two (SECONDS_PER_TREND) because the noWaterMeasCount count is incremented
        // once every two seconds.

        if (sysExecData.noWaterMeasCount < ((NO_WATER_HF_TO_LF_TIME_IN_SECONDS >> SECONDS_PER_TREND_SHIFT)-1)) {
            // The noWaterMeasCount is incremented once every two seconds. 
            sysExecData.noWaterMeasCount++;
        } else {
            // No water has been detected for at least NO_WATER_HF_TO_LF_TIME_IN_SECONDS.
            // Stay in the LF water measurement mode in order to save power.
            // If the waterMeasDelayCount has expired, re-start the LF delay countdown.
            if (!sysExecData.waterMeasDelayCount) {
                // Set the delay counter based on how often the waterMeasDelayCount is
                // decremented within the exec main loop. It is decremented once every
                // two seconds (SECONDS_PER_TREND)
                sysExecData.waterMeasDelayCount = (LOW_FREQUENCY_MEAS_TIME_IN_SECONDS >> SECONDS_PER_TREND_SHIFT);
            }
        }
    }

    // If the LF delay count is non-zero, then decrement it.
    if (sysExecData.waterMeasDelayCount) {
        sysExecData.waterMeasDelayCount--;
    }

    return currentFlowRateInMLPerSec;
}

/**
 * \brief There are two startup messages that must be sent. 
 *        Check the flags and delays to determine if the first
 *        one should be sent. Once the first startup message is
 *        sent, start the countdown to send the next one - but only
 *        after the first one has completed transmitting.
 *
 * \note The application record is updated after the first
 *       startup message completes. The record is written by the
 *       Application and used by the bootloader to help identify
 *       that the Application started successfully. The
 *       application record lives in one of the flash info
 *       sections.
 * 
 */
static void startUpMessageCheck(void) {
    if (!sysExecData.startUpMsg1WasSent) {
        if (sysExecData.secondsTillStartUpMsgTx > 0) {
            sysExecData.secondsTillStartUpMsgTx -= SECONDS_PER_TREND;
        }
        if (sysExecData.secondsTillStartUpMsgTx <= 0) {
            sendStartUpMsg1();
            sysExecData.startUpMsg1WasSent = true;
            sysExecData.secondsTillStartUpMsgTx = START_UP_MSG_TX_DELAY_IN_SECONDS;
        }
    } else if (!sysExecData.startUpMsg2WasSent && !dataMsgMgr_isSendMsgActive()) {
        if (sysExecData.secondsTillStartUpMsgTx > 0) {
            sysExecData.secondsTillStartUpMsgTx -= SECONDS_PER_TREND;
        }
        if (sysExecData.secondsTillStartUpMsgTx <= 0) {
            // Update the Application Record after sending the first FA packet.
            // The record is written by the Application and used by the
            // bootloader to help identify that the Application started successfully.
            // We wait until after the first FA packet is sent to write the
            // Application Record in an attempt to help verify that the
            // application code does not have a catastrophic issue.
            // If the bootloader does not see an Application
            // Record when it boots, then it will go into recovery mode.
            //
            // Don't write a new Application Record if one already exists.  It is
            // erased by the bootloader after a new firmware upgrade has been
            // performed before jumping to the new Application code.
            // The Application Record is located in the flash INFO C section.
            if (!checkForApplicationRecord()) {
                // If the record is not found, write one.
                initApplicationRecord();
            }
            sendStartUpMsg2();
            sysExecData.startUpMsg2WasSent = true;
        }
    }
}

/**
* \brief Initiate sending the first startup message which is a 
*        Final Assembly Message Type
*/
static void sendStartUpMsg1(void) {
    // Prepare the final assembly message
    // Get the shared buffer (we borrow the ota buffer)
    uint8_t *payloadP = modemMgr_getSharedBuffer();
    // Fill in the buffer with the standard message header
    uint8_t payloadSize = storageMgr_prepareMsgHeader(payloadP, MSG_TYPE_FINAL_ASSEMBLY);
    // Initiate sending the final assembly message
    dataMsgMgr_sendDataMsg(MSG_TYPE_FINAL_ASSEMBLY, payloadP, payloadSize);
}

/**
* \brief Initiate sending the second startup message which is a 
*        Monthly Check-In Message Type
*/
static void sendStartUpMsg2(void) {
    // Prepare the monthly check-in message
    // Get the shared buffer (we borrow the ota buffer)
    uint8_t *payloadP = modemMgr_getSharedBuffer();
    // Fill in the buffer with the standard message header
    uint8_t payloadSize = storageMgr_prepareMsgHeader(payloadP, MSG_TYPE_CHECKIN);
    // Initiate sending the monthly check-in message
    dataMsgMgr_sendDataMsg(MSG_TYPE_CHECKIN, payloadP, payloadSize);
}

/**
* \brief Support utility for the OTA message that resets the 
*        unit.  Checks to make sure the message keys are
*        correct, and if so, then starts a countdown counter for
*        rebooting the unit.
* 
* @return bool  Returns true if keys were correct.
*/
bool sysExec_startRebootCountdown(uint8_t activateReboot) {
    bool status = false;
    if (activateReboot == ACTIVATE_REBOOT_KEY) {
        sysExecData.secondsTillReboot = REBOOT_DELAY_IN_SECONDS;
        sysExecData.rebootCountdownIsActive = activateReboot;
        status = true;
    }
    return (status);
}

/**
* \brief Support routine that is called to perform a system 
*        reboot.  Called as a result of receiving the OTA reset
*        unit message.
*/
static void sysExec_doReboot(void) {
    if (sysExecData.rebootCountdownIsActive == ACTIVATE_REBOOT_KEY) {
        // Disable the global interrupt
        disableGlobalInterrupt();
        // Modem should already be off, but just for safety, turn it off
        modemPower_powerDownModem();
        while (1) {
            // Force watchdog reset
            WDTCTL = 0xDEAD;
            while (1);
        }
    } else {
        sysExecData.rebootCountdownIsActive = 0;
    }
}

#ifdef SEND_DEBUG_INFO_TO_UART
/**
* \brief Send debug information to the uart.  
*/
static void sysExec_sendDebugDataToUart(void) {
    // Get the shared buffer (we borrow the ota buffer)
    uint8_t *payloadP = modemMgr_getSharedBuffer();
    uint8_t payloadSize = storageMgr_prepareMsgHeader(payloadP, MSG_TYPE_DEBUG_TIME_INFO);

    dbgMsgMgr_sendDebugMsg(MSG_TYPE_DEBUG_TIME_INFO, payloadP, payloadSize);
    _delay_cycles(10000);
    storageMgr_sendDebugDataToUart();
    _delay_cycles(10000);
    // waterSense_sendDebugDataToUart();
    // _delay_cycles(10000);
}
#endif

