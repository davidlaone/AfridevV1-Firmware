/** 
 * @file msgData.c
 * \n Source File
 * \n Outpour MSP430 Firmware
 * 
 * \brief Manage the high level details of sending a data 
 *        message to the modem.
 */

#include "outpour.h"

/***************************
 * Module Data Definitions
 **************************/

/**
 * \def DATA_MSG_MAX_RETRIES
 * \brief Specify how many retries to attempt to send the data 
 *        msg if the network was not able to connect.
 */
#define DATA_MSG_MAX_RETRIES ((uint8_t)1)

/**
 * \def DATA_MSG_DELAY_IN_SECONDS_TILL_RETX
 * \brief Specify how long to wait to retransmit as a result of 
 *        the modem failing to connect to the network.
 * \note Max hours in a uint16_t representing seconds is 18.2 
 *       hours.  Currently set at 12 hours.
 */
#define DATA_MSG_DELAY_IN_SECONDS_TILL_RETX ((uint16_t)12*60*60)

/**
 * \typedef msgData_t
 * \brief Define a container to store data and state information
 *        to support sending a data message to the modem.
 */
typedef struct msgData_s {
    bool sendDataMsgActive;    /**< flag to mark a data message is in progress */
    bool sendDataMsgScheduled; /**< flag to mark a data message is scheduled */
    bool sendDailyLogs;        /**< flag to indicate we are sending daily logs */
    uint8_t retryCount;        /**< number of retries attempted */
    uint16_t secsTillTransmit; /**< time in seconds until transmit: max is 18.2 hours as 16 bit value */
    dataMsgSm_t dataMsgSm;     /**< Data message state machine object */
} msgData_t;

/****************************
 * Module Data Declarations
 ***************************/

/**
* \var msgData
* \brief Declare the data msg object.
*/
// static
msgData_t msgData;

/*************************
 * Module Prototypes
 ************************/

/***************************
 * Module Public Functions
 **************************/

/**
* \brief Call once as system startup to initialize the data 
*        message module.
* \ingroup PUBLIC_API
*/
void dataMsgMgr_init(void) {
    memset(&msgData, 0, sizeof(msgData_t));
}

/**
* \brief Return status on whether the data message manager is 
*        busy sending a message or not.
* 
* @return bool Returns true is actively sending.
*/
bool dataMsgMgr_isSendMsgActive(void) {
    return msgData.sendDataMsgActive;
}

/**
* \brief Exec routine should be called once every time from the 
*        main processing loop.  Drives the data message state
*        machine if there is a data message transmit currently
*        in progress.
* \ingroup EXEC_ROUTINE
*/
void dataMsgMgr_exec(void) {

    if (msgData.sendDataMsgActive) {

        dataMsgSm_t *dataMsgSmP = &msgData.dataMsgSm;

        // Handle sending multiple daily logs.
        // If we are sending daily logs, then check if the data message state
        // machine is done sending the current daily log.  If done, then we
        // will get the next daily log, and start sending it.
        if (msgData.sendDailyLogs && dataMsgSmP->sendCmdDone) {
            uint8_t *dataP;
            modemCmdWriteData_t *cmdWriteP = &dataMsgSmP->cmdWrite;
            // Check if there is another daily log to send
            // If so, it returns a non-zero value representing the length
            uint16_t length = storageMgr_getNextDailyLogToTransmit(&dataP);
            if (length) {
                // Update the data message state machine so that it will be
                // in the correct state to send the next daily log.
                dataMsgSm_sendAnotherDataMsg(dataMsgSmP);
                // Update the data command object with info about the current
                // daily log to send
                cmdWriteP->cmd             = OUTPOUR_M_COMMAND_SEND_DATA;
                cmdWriteP->payloadMsgId    = MSG_TYPE_DAILY_LOG;  /* the payload type */
                cmdWriteP->payloadP        = dataP;   /* the payload pointer */
                cmdWriteP->payloadLength   = length;  /* size of the payload in bytes */
            } else {
                msgData.sendDailyLogs = false;
            }
        }

        // Call the data message state machine to perform work
        dataMsgSm_stateMachine(dataMsgSmP);

        // Check if the data message state machine is done with the
        // complete session
        if (dataMsgSmP->allDone) {
            msgData.sendDataMsgActive = false;
            // Check if a modem network connect timeout occurred
            if (dataMsgSmP->connectTimeout) {
                // Error case
                // Check if this already was a retry
                // If not, then schedule a retry.  If it was, then abort.
                if (msgData.retryCount < DATA_MSG_MAX_RETRIES) {
                    msgData.retryCount++;
                    msgData.sendDataMsgScheduled = true;
                    msgData.secsTillTransmit = DATA_MSG_DELAY_IN_SECONDS_TILL_RETX;
                }
            }
        }
    } else if (msgData.sendDataMsgScheduled) {
        if (msgData.secsTillTransmit > 0) {
            msgData.secsTillTransmit--;
        }
        if (msgData.secsTillTransmit == 0) {
            // The sendWaterMsg function will clear the retryCount.
            // We need to save the current value so we can restore it.
            uint8_t retryCount = msgData.retryCount;
            // Use the standard data msg API to initiate the retry.
            // For retries, we assume the data is already stored in the modem
            // from the original try. We only have to "kick" the modem with any
            // type of M_COMMAND_SEND_DATA msg to get it to send out what it has
            // stored in its FIFOs (once its connected to the network).
            dataMsgMgr_sendDataMsg(MSG_TYPE_RETRYBYTE, NULL, 0);
            // Restore retryCount value.
            msgData.retryCount = retryCount;
        }
    }
}

/*******************************************************************************/
/*******************************************************************************/

/**
* \brief Used by upper layers to send a outpour data msg to the
*        modem. This kicks off a multi-module/multi state
*        machine sequence for sending the data command to the
*        modem.
*
* \ingroup PUBLIC_API
*  
* \note If the modem does not connect to the network within a
*       specified time frame (WAIT_FOR_LINK_UP_TIME_IN_SECONDS),
*       then one retry will be scheduled for the future.
* 
* @param msgId The outpour message identifier
* @param dataP Pointer to the data to send
* @param lengthInBytes The length of the data to send.
*/
bool dataMsgMgr_sendDataMsg(MessageType_t msgId, uint8_t *dataP, uint16_t lengthInBytes) {
    dataMsgSm_t *dataMsgSmP = &msgData.dataMsgSm;

    // If already busy, just return.
    // Should never happen, but just in case.
    if (msgData.sendDataMsgActive) {
        return false;
    }

    // Note - a new data transmission will cancel any scheduled re-transmission

    msgData.sendDataMsgActive = true;
    msgData.sendDataMsgScheduled = false;
    msgData.retryCount = 0;
    msgData.secsTillTransmit = 0;

    // Initialize the data msg object.
    dataMsgSm_initForNewSession(dataMsgSmP);

    // Initialize the data command object in the data object
    dataMsgSmP->cmdWrite.cmd             = OUTPOUR_M_COMMAND_SEND_DATA;
    dataMsgSmP->cmdWrite.payloadMsgId    = msgId;          /* the payload type */
    dataMsgSmP->cmdWrite.payloadP        = dataP;          /* the payload pointer */
    dataMsgSmP->cmdWrite.payloadLength   = lengthInBytes;  /* size of the payload in bytes */

    // Call the state machine
    dataMsgSm_stateMachine(dataMsgSmP);

    return true;
}

/*******************************************************************************/
/*******************************************************************************/

/**
 * 
 * \brief Inform the data message manager to start sending out 
 *        the daily logs of the current week.  This function
 *        will start the first daily log that needs to be sent.
 *        The function queries the storage module to get the
 *        next daily log to send.  Then initializes the command
 *        write structure and calls the data message state
 *        machine to start the transmit sequence.  The calls to
 *        get the remaining data logs for the week to send is
 *        done by the data message manager exec function.
* \ingroup PUBLIC_API
 * 
 * \return bool Returns false if the data message manager is 
 *         already busy transmitting and currently not
 *         available.
 */
bool dataMsgMgr_sendDailyLogs(void) {
    uint8_t *dataP;
    uint16_t length = 0;

    // If already busy, just return.
    // Should never happen, but just in case.
    if (msgData.sendDataMsgActive) {
        return false;
    }

    // Get the first daily log to send.
    length = storageMgr_getNextDailyLogToTransmit(&dataP);
    if (length) {
        dataMsgSm_t *dataMsgSmP = &msgData.dataMsgSm;
        modemCmdWriteData_t *cmdWriteP = &dataMsgSmP->cmdWrite;

        // Note - a new data transmission will cancel any scheduled re-transmission

        msgData.sendDataMsgActive = true;
        msgData.sendDailyLogs = true;
        msgData.sendDataMsgScheduled = false;
        msgData.retryCount = 0;
        msgData.secsTillTransmit = 0;

        // Initialize the data message object so its ready to
        // start processing a new message.
        dataMsgSm_initForNewSession(dataMsgSmP);

        // Initialize the data command object used to communicate payload
        // pointer and length to the data message manager.
        cmdWriteP->cmd           = OUTPOUR_M_COMMAND_SEND_DATA;
        cmdWriteP->payloadMsgId  = MSG_TYPE_DAILY_LOG;  /* the payload type */
        cmdWriteP->payloadP      = dataP;   /* the payload pointer */
        cmdWriteP->payloadLength = length;  /* size of the payload in bytes */

        // Call the data message state machine to perform work
        dataMsgSm_stateMachine(dataMsgSmP);
    }
    return true;
}

