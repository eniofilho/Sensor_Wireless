/*! \file edOperationMachine.h
 *  \brief interface publica para a maquina de operacao do modo end device.
 */

#include "led.h"
#include "button.h"
#include "radio.h"
#include "flashParam.h"

typedef enum
{
   OPERATION_MACHINE_STATE_DEEP_SLEEP = 0,
   OPERATION_MACHINE_STATE_CONFIG,
   OPERATION_MACHINE_STATE_TURN_ON_RADIO,
   OPERATION_MACHINE_STATE_TURN_OFF_RADIO,
   OPERATION_MACHINE_STATE_SEARCH_AP_QUERY,
   OPERATION_MACHINE_STATE_SEARCH_AP_WAIT,
   OPERATION_MACHINE_STATE_CHANGE_CHANNEL,
   OPERATION_MACHINE_STATE_SEND_STATUS,
   OPERATION_MACHINE_STATE_MEASURE_BATT,
   OPERATION_MACHINE_STATE_WAIT_ACK,
   OPERATION_MACHINE_STATE_SLEEP,
   OPERATION_MACHINE_STATE_INFORM_STATUS
} OPERATION_MACHINE_STATE;

typedef struct OPERATION_MACHINE_STRUCT
{
   void (* init)              (void * pOp);
   void (* run)               (void * pOp);
   void (* setState)          (void * pOp, OPERATION_MACHINE_STATE state);
   void (* setTimeout)        (void * pOp, unsigned short timeout);
   void (* incTimer)          (void * pOp);

   OPERATION_MACHINE_STATE    state;
   unsigned char              channel;
   unsigned short             timer;
   unsigned short             timeout;
   unsigned char              configState;
   unsigned char              timeoutStatus;
   
   RADIO *                    radio;
   unsigned char              tempBuff[256];
   unsigned char              tempLen;
   unsigned char              message[15];

   LED *                      led;
   BUTTON *                   btConfig;
   BUTTON *                   btSense;
   
   FLASH_PARAM *              flash;
} OPERATION_MACHINE;

extern OPERATION_MACHINE operationMachine;

void opInit       (void * pOp);