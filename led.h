/*! \file led.h
 *  \brief interface publica para o objeto led.
 */

typedef enum
{
   LED_STATE_OFF = 0,
   LED_STATE_ON,
   LED_STATE_BL_OFF,
   LED_STATE_BL_ON,
   LED_STATE_BL_PAUSE
} LED_STATE;

typedef struct
{
   unsigned short PIN;
   unsigned short POUT;
   unsigned short PDIR;
   unsigned short PREN;
   unsigned short PDS;
   unsigned short PSEL;
} GPIO_PORT;

typedef struct LED_STRUCT
{
   void (* init)              (void * pled, unsigned char port, unsigned char bit);
   void (* on)                (void * pled);
   void (* off)               (void * pled);
   void (* blink)             (void * pled, unsigned char blinks, unsigned short onTime, unsigned short offTime, unsigned short pauseTime, unsigned char repetitions);
   LED_STATE (* getState)     (void * pled);
   void (* run)               (void *pled);

   LED_STATE      state;
   
   GPIO_PORT * port;
   unsigned char bit;
   
   unsigned short tOn;
   unsigned short tOff;
   unsigned short tPause;
   unsigned char  blinks;
   unsigned char tempBlinks;
   unsigned char  repetitions;
   unsigned short timer;
   unsigned short timeout;
} LED;

extern LED led1;
