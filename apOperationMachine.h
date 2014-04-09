/*! \file edOperationMachine.h
 *  \brief interface publica para a maquina de operacao do modo end device.
 */

#include "serial.h"
#include "radio.h"
#include "flashParam.h"

typedef enum
{
   OPERATION_MACHINE_STATE_IDLE = 0,
   OPERATION_MACHINE_STATE_SCAN_WAIT,
   OPERATION_MACHINE_STATE_SCAN_ACK,
   OPERATION_MACHINE_STATE_RECEIVE_WAIT,
   OPERATION_MACHINE_STATE_RECEIVE_ACK,
   OPERATION_MACHINE_STATE_INVENTORY_WAIT,
   OPERATION_MACHINE_STATE_DEBUG
} OPERATION_MACHINE_STATE;

typedef enum
{
   SENSOR_WRITE_STATUS_ERROR = 0,
   SENSOR_WRITE_STATUS_OK,
   SENSOR_WRITE_STATUS_ALREADY_ON_LIST
} SENSOR_WRITE_STATUS;

typedef enum
{
   SENSOR_ERASE_STATUS_ERROR = 0,
   SENSOR_ERASE_STATUS_OK,
   SENSOR_ERASE_STATUS_NOT_FOUND
} SENSOR_ERASE_STATUS;

typedef enum
{
   SENSOR_COMM_STATUS_NOT_PRESENT = 0,
   SENSOR_COMM_STATUS_OK = '1',
   SENSOR_COMM_STATUS_NOK = '0',
} SENSOR_COMM_STATUS;

typedef struct OPERATION_MACHINE_STRUCT
{
   void (* init)              (void * pOp);
   void (* run)               (void * pOp);
   void (* setState)          (void * pOp, OPERATION_MACHINE_STATE state);
   void (* setTimeout)        (void * pOp, unsigned short timeout);
   void (* incTimer)          (void * pOp);
   SENSOR_WRITE_STATUS (* sensorWrite)       (void * pOp, unsigned char * sensorID, unsigned char sensorLen); 
   SENSOR_ERASE_STATUS (* sensorErase)       (void * pOp, unsigned char * sensorID, unsigned char sensorLen);
   signed char   (* sensorGetPos)            (void * pOp, unsigned char * sensorID);
   unsigned char (* sensorGetCount)          (void * pOp);

   OPERATION_MACHINE_STATE    state;
   unsigned char              channel;
   unsigned short             timer;
   unsigned short             timeout;
   unsigned char              configState;
   unsigned char              timeoutStatus;
   unsigned char              earlyMessage;
   
   RADIO *                    radio;
   unsigned char              tempBuff[256];
   unsigned char              tempLen;
   unsigned char              message[32];
   
   unsigned char              sensorsFound;
   
   SENSOR_COMM_STATUS         sensorStatus[SENSOR_LIST_SIZE+1];
   unsigned char              sensorValue[SENSOR_LIST_SIZE][4];
   unsigned char              sensorType[SENSOR_LIST_SIZE];
   
   unsigned char              commTimeout;

   SERIAL *                   serial;
   FLASH_PARAM *              flash;
   
   unsigned char              wdtControl;
} OPERATION_MACHINE;

extern OPERATION_MACHINE operationMachine;

extern void opInit       (void * pOp);