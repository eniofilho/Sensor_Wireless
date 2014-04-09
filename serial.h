/* \file serial.h
 * \brief Interface publica da comunicacao serial com o host.
 */

#include <stdarg.h>
#include "uart.h"

#define SERIAL_MESSAGE_QUEUE_SIZE 4
#define SERIAL_VAR_LEN 16

typedef enum
{
   SERIAL_STATE_IDLE = 0,
   SERIAL_STATE_SENSOR,
   SERIAL_STATE_SENSOR_WRITE,
   SERIAL_STATE_SENSOR_ERASE,
   SERIAL_STATE_CHANNEL,
   SERIAL_STATE_CHANNEL_SET,
   SERIAL_STATE_MODE,
   SERIAL_STATE_TIMEOUT,
   SERIAL_STATE_TIMEOUT_WRITE
} SERIAL_STATE;

typedef enum
{
   SERIAL_MESSAGE_NONE = 0,
   SERIAL_MESSAGE_SENSOR_WRITE,
   SERIAL_MESSAGE_SENSOR_ERASE,
   SERIAL_MESSAGE_SENSOR_LIST,
   SERIAL_MESSAGE_CHANNEL_SET,
   SERIAL_MESSAGE_CHANNEL_READ,
   SERIAL_MESSAGE_MODE_SEARCH,
   SERIAL_MESSAGE_MODE_RECEIVE,
   SERIAL_MESSAGE_MODE_RECEIVE_INIT,
   SERIAL_MESSAGE_MODE_INVENTORY,
   SERIAL_MESSAGE_MODE_DEBUG,
   SERIAL_MESSAGE_TIMEOUT_READ,
   SERIAL_MESSAGE_TIMEOUT_SET,
   SERIAL_MESSAGE_ACK,
} SERIAL_MESSAGE;

typedef struct SERIAL_STRUCT
{
   void (* init)              (void * pserial);
   void (* transmit)          (void * pserial, unsigned char * data,...);
   void (* processBuffRx)     (void * pserial);
   void (* putMessage)        (void * pserial, SERIAL_MESSAGE message);
   char (* getMessage)        (void * pserial, SERIAL_MESSAGE * message);
   void (* clearMessages)     (void * pserial);
   void (* reset)             (void * pserial);
   
   UART *         uart;
   SERIAL_STATE   state;
   unsigned int   subState;
   SERIAL_MESSAGE tempMessage;
   SERIAL_MESSAGE messages[SERIAL_MESSAGE_QUEUE_SIZE];
   unsigned int   msgPtrIn;
   unsigned int   msgPtrOut;
   unsigned char  var1[SERIAL_VAR_LEN];
   unsigned int   var1Len;
   unsigned char  var2[SERIAL_VAR_LEN];
   unsigned int   var2Len;
   unsigned short timeoutSerial;
} SERIAL;

extern SERIAL serial1;

