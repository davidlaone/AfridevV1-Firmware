/** 
 * @file modemMgr.c
 * \n Source File
 * \n Outpour MSP430 Bootloader Firmware
 * 
 * \brief High level interface that the message objects used to
 *        transmit and receive messages to/from the modem.
 * 
 * \note Batch Job:  When a command is sent to the modem from
 *       the uppler layers, a sequence of support commands are
 *       also sent. The complete sequence includes:
 *
 * \li Send A Modem Ping Message  (M_COMMAND_PING)
 * \li Send The Primary Command to Send (M_COMMAND_SEND_SOS, 
 *     M_COMMAND_GET_INCOMING_PARTIAL,
 *     M_COMMAND_DELETE_INCOMING)
 * \li Send A Modem Status Message (M_COMMAND_MODEM_STATUS)
 * \li Send A Modem Message Status Message
 *     (M_COMMAND_MESSAGE_STATUS)
 *
 * This total set of commands is referred to as a modem write
 * batch job.  This composes the fundamental framework used to
 * communicate with the modem for the upper layers. A batch job
 * runs a state machine that cycles though several states to
 * send the individual commands to the modem and wait for
 * responses.
 * 
 * The upper layers use the single API modemMgr_sendModemCmdBatch
 * to send the modem command they are interested in.  The batch
 * job first sends a ping msg, then the modem command of
 * interest, then a modem status and finally a message status
 * command. Information returned from the modem status and
 * message status are parsed for key information such as Network
 * Connection Status and Number of pending OTA messages.
 */

/***************************
 * Module Data Definitions
 **************************/

#include "outpour.h"

/**
 * \typedef mwBatchState_t
 * \brief Define the states that make up the modem write batch 
 *        job states.
 */
typedef enum mwBatchState_e {
    MWBATCH_STATE_IDLE,
    MWBATCH_STATE_PING,
    MWBATCH_STATE_PING_WAIT,
    MWBATCH_STATE_WRITE_CMD,
    MWBATCH_STATE_WRITE_CMD_WAIT,
    MWBATCH_STATE_MODEM_STATUS,
    MWBATCH_STATE_MODEM_STATUS_WAIT,
    MWBATCH_STATE_MSG_STATUS,
    MWBATCH_STATE_MSG_STATUS_WAIT,
    MWBATCH_STATE_DONE,
}mwBatchState_t;

/**
 * \typedef mwBatchData_t
 * \brief Define a container to hold data specific to the modem 
 *        write batch job.
 */
typedef struct mwBatchData_s {
    bool allocated;                  /**< flag to indicate modem is owned by client */
    bool active;                     /**< currently sending out a write batch job */
    bool commError;                  /**< A modem UART comm error occurred during the job */
    uint8_t modemLinkUpStatus;       /**< network connection status received from modem */
    mwBatchState_t mwBatchState;     /**< current batch job state */
    modemCmdWriteData_t *cmdWriteP;  /**< A pointer to the command info object */
    otaResponse_t otaResponse;       /**< payload of the last ota message received */
    uint8_t numOfOtaMsgsAvailable;   /**< parsed from modem message status command */
    uint16_t sizeOfOtaMsgsAvailable; /**< parsed from modem message status command */
} mwBatchData_t;

/****************************
 * Module Data Declarations
 ***************************/

/**
 * \var otaBuf
 * \brief When an OTA message is received via the modemCmd 
 *        module, it is copied into the otaBuf for storage and
 *        access by upper modules.  This buffer location is
 *        specified in the linker command file to live right
 *        below the stack space.
 */
#pragma SET_DATA_SECTION(".commbufs")
static uint8_t otaBuf[OTA_PAYLOAD_BUF_LENGTH];  /**< A buffer to hold one OTA message */
#pragma SET_DATA_SECTION()

/**
 * \var mwBatchData
 * \brief Declare the container to hold data specific to the 
 *        modem write batch job.
 */
static mwBatchData_t mwBatchData;

/*************************
 * Module Prototypes
 ************************/
static void modemMgr_stateMachine(void);
static void parseModemStatusCmdResponse(modemCmdReadData_t *readDataP);
static void parseModemMsgStatusCmdResponse(modemCmdReadData_t *readDataP);
static void parseModemOtaCmdResponse(modemCmdReadData_t *readDataP);

/***************************
 * Module Public Functions
 **************************/

// TODO - need overall timeout on batch job!

/**
* \brief Drive the state machines for sending a write batch job
*        or to handle OTA messages
* \ingroup EXEC_ROUTINE
*/
void modemMgr_exec(void) {
    if (mwBatchData.active) {
        modemMgr_stateMachine();
    }
}

/**
* \brief Initialize the modem manager module.  Should be called 
*        once on system start up.
* \ingroup PUBLIC_API
*/
void modemMgr_init(void) {
    memset(&mwBatchData, 0, sizeof(mwBatchData_t));
    mwBatchData.mwBatchState = MWBATCH_STATE_PING;
    mwBatchData.otaResponse.buf = otaBuf;
}

/**
* \brief Grab the modem resource.  This is the first step that 
*        any modem client must use to allocate the modem.  If
*        the modem is not currently up, then the grab function
*        will start the modem power up sequence.
* \ingroup PUBLIC_API
* 
* @return bool Returns true if the modem resource was 
*         successfully allocated.
*/
bool modemMgr_grab(void) {
    bool success = false;
    if (!mwBatchData.allocated) {
        mwBatchData.allocated = true;
        success = true;
        if (!modemMgr_isModemUp()) {
            modemLink_restart();
        }
    }
    return success;
}

/**
* \brief Returns true if the modem is powered on.  
* \ingroup PUBLIC_API
* 
* @return bool True if modem is powered on.
*/
bool modemMgr_isModemUp(void) {
    return modemLink_isModemUp();
}

/**
* \brief Send a command to the modem.  This kicks off a write
*        batch job.  A write batch contains a number of
*        cmds that are sent to the modem in addition to the
*        command, including a modem status cmd and a modem
*        message status cmd.
* \ingroup PUBLIC_API
*
* @param cmdWriteP  pointer to a modemCmdWriteData_t object. 
*                   IMPORTANT - object must not be on the stack!
*                   The pointer is saved for future operations.
*/
void modemMgr_sendModemCmdBatch(modemCmdWriteData_t *cmdWriteP) {
    mwBatchData.cmdWriteP = cmdWriteP;
    mwBatchData.mwBatchState = MWBATCH_STATE_PING;
    mwBatchData.active = true;
    mwBatchData.commError = false;
    mwBatchData.modemLinkUpStatus = MODEM_STATE_IDLE;
    modemMgr_stateMachine();
}

/**
* \brief Stop any modem cmd batch job currently in progress. 
* \ingroup PUBLIC_API
*/
void modemMgr_stopModemCmdBatch(void) {
    mwBatchData.mwBatchState = MWBATCH_STATE_IDLE;
    mwBatchData.active = false;
}

/**
* \brief Poll function to determine if the batch job has
*        completed.  It takes multiple steps within the state
*        machine to complete a modem cmd batch job. Clients use
*        this function to determine when the job is complete.
* \ingroup PUBLIC_API
* 
* @return bool True if modem cmd batch job is complete.
*/
bool modemMgr_isModemCmdComplete(void) {
    return !mwBatchData.active;
}

/**
* \brief Identify if an error occurred when performing the serial
*        transmit of the command to the modem.
* \ingroup PUBLIC_API
* 
* @return bool True if an error occurred.
*/
bool modemMgr_isModemCmdError(void) {
    bool error = false;
    // We only want to return the error once
    // the complete batch job is complete.
    // It just makes things simpler.
    if (!mwBatchData.active) {
        error = mwBatchData.commError;
    }
    return error;
}

/**
* \brief Power cycle the modem.  Restarts the modemLink state 
*        machine for powering on the modem.
* \ingroup PUBLIC_API
*/
void modemMgr_restartModem(void) {
    modemLink_restart();
}

/**
* \brief Release the modem.  Must be called by all upper layer 
*        message objects when they are done with the modem.
* \ingroup PUBLIC_API
*/
void modemMgr_release(void) {
    mwBatchData.allocated = false;
    mwBatchData.active = false;
    mwBatchData.mwBatchState = MWBATCH_STATE_IDLE;
    modemLink_shutdownModem();
}

/**
* \brief Returns the number of OTA messages pending.  This 
*        information is parsed from the modem message status
*        command that occurs as part of the modem write batch
*        job.
* \ingroup PUBLIC_API
* 
* @return uint8_t  Number of OTA messages as parsed from the 
*         modem message status command.
*/
uint8_t modemMgr_getNumOtaMsgsPending(void) {
    return mwBatchData.numOfOtaMsgsAvailable;
}

/**
* \brief Returns the size of OTA message pending. This 
*        information is parsed from the modem message status
*        command that occurs as part of the modem write batch
*        job.
* 
* @return uint16_t Total size of the OTA messages pending as 
*         parsed from the modem message message status command.
*/
uint16_t modemMgr_getSizeOfOtaMsgsPending(void) {
    return mwBatchData.sizeOfOtaMsgsAvailable;
}

/**
* \brief Returns true if the modem network status is connected. 
*        This information is parsed from the modem status
*        command that occurs as part of the modem write batch
*        job.
* \ingroup PUBLIC_API
* 
* @return bool True if modem network status is connected 
*         (0x4,MODEM_STATE_CONNECTED)
*/
bool modemMgr_isLinkUp(void) {
    return ((mwBatchData.modemLinkUpStatus == MODEM_STATE_CONNECTED) ? true : false);
}

/**
* \brief Returns true if the modem has returned an error network
*        connection status. This information is parsed from
*        the modem status command that occurs as part of the
*        modem write batch job.
* \ingroup PUBLIC_API
* 
* @return bool True if modem network connection status returned 
*         an error state.
*/
bool modemMgr_isLinkUpError(void) {
    // MSB (bit 7) set if the modem status is an error status.
    return ((mwBatchData.modemLinkUpStatus & 0x80) ? true : false);
}

/**
* \brief Return the payload of the last OTA (partial) message 
*        received.
* \ingroup PUBLIC_API
* 
* @return otaResponse_t* Returns a pointer to a otaResponse 
*         object.
*/
otaResponse_t* modemMgr_getLastOtaResponse(void) {
    return &mwBatchData.otaResponse;
}

/**
* \brief Retrieve a pointer to the shared buffer that can be
*        used for other purposes.  Due to limited RAM
*        availability on the MSP430, The message buffer used for
*        holding a received OTA message must be time shared. The
*        only time the buffer is needed for OTA processing is to
*        receive OTA messages.  Otherwise it can be used for
*        other things.
* \ingroup PUBLIC_API
* 
* @return uint8_t* Returns a pointer to the OTA buffer.
*/
uint8_t* modemMgr_getSharedBuffer(void) {
    return mwBatchData.otaResponse.buf;
}

/*************************
 * Module Private Functions
 ************************/

/**
* \brief State machine for sending the modem write batch set of 
*        commands.
*/
static void modemMgr_stateMachine(void) {
    bool continue_processing = false;
    do {
        continue_processing = false;
        switch (mwBatchData.mwBatchState) {
        case MWBATCH_STATE_IDLE:
            break;
        case MWBATCH_STATE_PING:
            {
                modemCmdWriteData_t mcWriteData;
                memset(&mcWriteData, 0, sizeof(modemCmdWriteData_t));
                mcWriteData.cmd = M_COMMAND_PING;
                modemCmd_write(&mcWriteData);
                // Next state
                mwBatchData.mwBatchState = MWBATCH_STATE_PING_WAIT;
            }
            break;
        case MWBATCH_STATE_PING_WAIT:
            if (!modemCmd_isBusy()) {
                // If the status only flag is set in the cmd, then the client is
                // only requesting modem status information and not to send
                // a new data command.  So set the state accordingly.
                if (mwBatchData.cmdWriteP->statusOnly) {
                    // Next state - jump to get modem status
                    mwBatchData.mwBatchState = MWBATCH_STATE_MODEM_STATUS;
                } else {
                    // Next state - send command
                    mwBatchData.mwBatchState = MWBATCH_STATE_WRITE_CMD;
                }
                continue_processing = true;
            }
            break;

        case MWBATCH_STATE_WRITE_CMD:
            {
                modemCmd_write(mwBatchData.cmdWriteP);
                // Next state
                mwBatchData.mwBatchState = MWBATCH_STATE_WRITE_CMD_WAIT;
            }
            break;
        case MWBATCH_STATE_WRITE_CMD_WAIT:
            if (!modemCmd_isBusy()) {
                // If cmd was a get OTA data request, parse and save the data
                if (mwBatchData.cmdWriteP->cmd == M_COMMAND_GET_INCOMING_PARTIAL) {
                    modemCmdReadData_t readData;
                    modemCmd_read(&readData);
                    parseModemOtaCmdResponse(&readData);
                }
                // Next state
                mwBatchData.mwBatchState = MWBATCH_STATE_MODEM_STATUS;
                continue_processing = true;
            }
            break;

        case MWBATCH_STATE_MODEM_STATUS:
            {
                modemCmdWriteData_t mcWriteData;
                memset(&mcWriteData, 0, sizeof(modemCmdWriteData_t));
                mcWriteData.cmd = M_COMMAND_MODEM_STATUS;
                modemCmd_write(&mcWriteData);

                // Next state
                mwBatchData.mwBatchState = MWBATCH_STATE_MODEM_STATUS_WAIT;
            }
            break;

        case MWBATCH_STATE_MODEM_STATUS_WAIT:
            if (!modemCmd_isBusy()) {
                // Read data and parse
                modemCmdReadData_t readData;
                modemCmd_read(&readData);
                parseModemStatusCmdResponse(&readData);

                // Next state
                mwBatchData.mwBatchState = MWBATCH_STATE_MSG_STATUS;
                continue_processing = true;
            }
            break;

        case MWBATCH_STATE_MSG_STATUS:
            {
                modemCmdWriteData_t mcWriteData;
                memset(&mcWriteData, 0, sizeof(modemCmdWriteData_t));
                mcWriteData.cmd = M_COMMAND_MESSAGE_STATUS;
                modemCmd_write(&mcWriteData);

                // Next state
                mwBatchData.mwBatchState = MWBATCH_STATE_MSG_STATUS_WAIT;
            }
            break;

        case MWBATCH_STATE_MSG_STATUS_WAIT:
            if (!modemCmd_isBusy()) {
                // Read data and parse
                modemCmdReadData_t readData;
                modemCmd_read(&readData);
                parseModemMsgStatusCmdResponse(&readData);

                // Next state
                mwBatchData.mwBatchState = MWBATCH_STATE_DONE;
                continue_processing = true;
            }
            break;

        case MWBATCH_STATE_DONE:
            mwBatchData.active = false;
            break;
        }

        // If a uart comm error occurred, record it.
        if (modemCmd_isError()) {
            mwBatchData.commError |= true;
        }

    } while (continue_processing);
}

/**
* \brief Parse a modem status command response to retrieve the 
*        link status.
* 
* @param readDataP Pointer to a modemCmdReadData_t object 
*/
static void parseModemStatusCmdResponse(modemCmdReadData_t *readDataP) {

    if (readDataP->valid && (readDataP->modemCmdId == M_COMMAND_MODEM_STATUS)) {
        modem_state_t modemState = (modem_state_t)readDataP->dataP[2];
        mwBatchData.modemLinkUpStatus = (uint8_t)modemState;
    }
}

/**
* \brief Parse a modem message status command response to 
*        retrieve the number of OTA messages available.
* 
* @param readDataP Pointer to a modemCmdReadData_t object 
*/
static void parseModemMsgStatusCmdResponse(modemCmdReadData_t *readDataP) {
    if (readDataP->valid && (readDataP->modemCmdId == M_COMMAND_MESSAGE_STATUS)) {
        // Only take LS Byte of message count
        // byte 0 = start byte of message
        // byte 1 = modemCmdId
        // byte 2 = number of ota messages MSB
        // byte 3 = number of ota messages LSB
        mwBatchData.numOfOtaMsgsAvailable = readDataP->dataP[3];
        // size of ota messages available is located in bytes 4,5,6 & 7 (a 32 bit value).
        // only read the lower 16 bits for size
        mwBatchData.sizeOfOtaMsgsAvailable = readDataP->dataP[6]<<8 | readDataP->dataP[7];
    }
}

/**
* \brief Parse a modem message status command response to 
*        retrieve the number of OTA messages available.
* 
* @param readDataP Pointer to a modemCmdReadData_t object 
*/
static void parseModemOtaCmdResponse(modemCmdReadData_t *readDataP) {
    if (readDataP->valid && (readDataP->modemCmdId == M_COMMAND_GET_INCOMING_PARTIAL)) {
        // The data return from the modem for a partial is as follows:
        // byte 0: start byte
        // byte 1: cmdId
        // bytes 2,3,4,5 = uint32_t dataLength (note MSB is first)
        // we are only interested in 8 bits since we will never retrieve more that 128 bytes at a time
        uint8_t lengthInBytes = readDataP->dataP[5];
        // bytes 6,7,8,9 = uint32_t dataRemaining (note MSB is first)
        uint16_t remainingInBytes = (readDataP->dataP[8] << 8) | readDataP->dataP[9];
        if (lengthInBytes > OTA_PAYLOAD_BUF_LENGTH) {
            lengthInBytes = 0;
        }
        // copy the payload start at byte offset 10 of the received modem response
        memcpy(&mwBatchData.otaResponse.buf[0], &readDataP->dataP[10], lengthInBytes);
        mwBatchData.otaResponse.lengthInBytes = lengthInBytes;
        mwBatchData.otaResponse.remainingInBytes = remainingInBytes;
    } else {
        mwBatchData.otaResponse.lengthInBytes = 0;
    }
}

