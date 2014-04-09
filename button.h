/*! \file button.h
 *  \brief interface publica para o objeto botao.
 */

typedef enum
{
   BUTTON_STATE_IDLE = 0,
   BUTTON_STATE_DEBOUNCE,
   BUTTON_STATE_PRESS_SHORT,
   BUTTON_STATE_PRESS_LONG,
   BUTTON_STATE_PRESS_LONG_WAIT_RELEASE
} BUTTON_STATE;

typedef enum
{
   BUTTON_PRESS_NONE = 0,
   BUTTON_PRESS_SHORT,
   BUTTON_PRESS_LONG,
} BUTTON_PRESS_TYPE;

typedef struct
{
   unsigned short PIN;
   unsigned short POUT;
   unsigned short PDIR;
   unsigned short PREN;
   unsigned short PDS;
   unsigned short PSEL;
   unsigned short DNU1;
   unsigned char  P1IV;
   unsigned char  DNU2;
   unsigned short DNU3;
   unsigned short DNU4;
   unsigned short DNU5;
   unsigned short DNU6;
   unsigned short PIES;
   unsigned short PIE;
   unsigned short PIFG;
   unsigned char P2IV;
   unsigned char DNU7;
} GPIO_PORT_BT;

typedef struct BUTTON_STRUCT
{
   void (* init)              (void * pbt, unsigned char port, unsigned char bit);
   void (* clear)             (void * pbt);
   void (* intEnable)         (void * pbt);
   void (* intDisable)        (void * pbt);
   char (* get)               (void * pbt, BUTTON_PRESS_TYPE * type);
   char (* getPin)            (void * pbt);
   void (* run)               (void *pbt);

   BUTTON_STATE      state;
   
   GPIO_PORT_BT * port;
   unsigned char bit;
   
   BUTTON_PRESS_TYPE type;
   unsigned short timer;
   unsigned short timeout;
} BUTTON;

extern BUTTON btConfig;
extern BUTTON btSense;
