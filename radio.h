/*! \file radio.h
 *  \brief interface publica para o objeto radio.
 */

#define RADIO_RX_BUFFER_SIZE 258

typedef enum
{
   RADIO_STATE_OFF = 0,
   RADIO_STATE_RX_MODE,
   RADIO_STATE_IDLE_MODE
} RADIO_STATE;

typedef struct RADIO_STRUCT
{
   void (* init)              (void * pradio);
   void (* powerOff)          (void * pradio);
   void (* reset)             (void * pradio);
   void (* config)            (void * pradio);
   void (* receiveOn)         (void * pradio);
   void (* receiveOff)        (void * pradio);
   char (* transmit)          (void * pradio, unsigned char * data, unsigned char len);
   char (* getData)           (void * pradio, unsigned char * buff, unsigned char * len);
   
   void (* isr)               (void);

   RADIO_STATE    state;
   unsigned char  rxBuffer[RADIO_RX_BUFFER_SIZE];
   unsigned char  rxLen;
   
   unsigned char  timer;
} RADIO;

extern RADIO radio1;

void radio_reset(void);
void config_radio(void);
void ReceiveOn(void);
void ReceiveOff(void);
unsigned char transmitPacket(unsigned char *buffer, unsigned char length);
void RadioIsr(void);
unsigned char radioStrobe(unsigned char addr);