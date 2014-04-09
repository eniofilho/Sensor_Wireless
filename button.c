/*! \file button.c
 *  \brief implementacao do objeto botao.
 */

#include "button.h"
#include "cc430x513x.h"

// DEFINES
#define BUTTON_TIMEOUT_DEBOUNCE    10
#define BUTTON_TIMEOUT_PRESS_SHORT 100
#define BUTTON_TIMEOUT_PRESS_LONG  300


// prototipos dos metodos
void buttonInit       (void * pbt, unsigned char port, unsigned char bit);
void buttonClear      (void * pbt);
void buttonIntEnable  (void * pbt);
void buttonIntDisable (void * pbt);
char buttonGet        (void * pbt, BUTTON_PRESS_TYPE * type);
char buttonGetPin     (void * pbt);
void buttonRun        (void *pbt);

// Instancias dos botoes
BUTTON btConfig = {buttonInit};
BUTTON btSense = {buttonInit};

// Implementacao dos metodos
void buttonInit       (void * pbt, unsigned char port, unsigned char bit)
{
   BUTTON * bt = (BUTTON *)pbt;
   
   bt->clear = buttonClear;
   bt->intEnable = buttonIntEnable;
   bt->intDisable = buttonIntDisable;
   bt->get = buttonGet;
   bt->getPin = buttonGetPin;
   bt->run = buttonRun;
   
   switch(port)
   {
      case 0:
         bt->port = (GPIO_PORT_BT *)0x0200;
         break;
      case 1:
         bt->port = (GPIO_PORT_BT *)0x0220;
         break;
      case 2:
         bt->port = (GPIO_PORT_BT *)0x0240;
         break;
   }
   bt->bit = bit;
   
   bt->clear(bt);
   
   // configura o pino
   bt->port->PSEL &= ~(0x01 << bt->bit);
   bt->port->PDIR &= ~(0x01 << bt->bit);
   bt->port->POUT |=  (0x01 << bt->bit); // pull up
   bt->port->PREN  |=  (0x01 << bt->bit); // pull up
}

void buttonClear      (void * pbt)
{
   BUTTON * bt = (BUTTON *)pbt;
   
   bt->state = BUTTON_STATE_PRESS_LONG_WAIT_RELEASE;
   bt->timer = 0;
}

void buttonIntEnable  (void * pbt)
{
   BUTTON * bt = (BUTTON *)pbt;
   
   if (bt->port == (GPIO_PORT_BT *)0x0200)
   {
      if (bt->getPin(bt)) bt->port->PIES |=  (0x01 << bt->bit);
      else                bt->port->PIES &=  ~(0x01 << bt->bit);
      bt->port->PIE  |=  (0x01 << bt->bit);
      bt->port->PIFG &= ~(0x01 << bt->bit);
   }
}

void buttonIntDisable (void * pbt)
{
   BUTTON * bt = (BUTTON *)pbt;
   if (bt->port == (GPIO_PORT_BT *)0x0200)
   {
      bt->port->PIE  &=  ~(0x01 << bt->bit);
   }
}

char buttonGet        (void * pbt, BUTTON_PRESS_TYPE * type)
{
   BUTTON * bt = (BUTTON *)pbt;
   
   if (bt->type)
   {
      *type = bt->type;
      bt->type = BUTTON_PRESS_NONE;
      return 1;
   }
   return 0;
}

char buttonGetPin     (void * pbt)
{
   BUTTON * bt = (BUTTON *)pbt;
   
   if ((bt->port->PIN & (0x01 << bt->bit)) == 0) return 0;
   return 1;
}

void buttonRun        (void *pbt)
{
   BUTTON * bt = (BUTTON *)pbt;
   
   ++bt->timer;
   
   switch(bt->state)
   {
      case BUTTON_STATE_IDLE:
         if ((bt->port->PIN & (0x01 << bt->bit)) == 0)
         {
            bt->state = BUTTON_STATE_DEBOUNCE;
            bt->timer = 0;
            bt->timeout = BUTTON_TIMEOUT_DEBOUNCE;
         }
         break;
      case BUTTON_STATE_DEBOUNCE:
         if (bt->timer >= bt->timeout)
         {
            if ((bt->port->PIN & (0x01 << bt->bit)) == 0)
            {
               bt->state = BUTTON_STATE_PRESS_SHORT;
               bt->timer = 0;
               bt->timeout = BUTTON_TIMEOUT_PRESS_SHORT;
            }
         }
         break;
      case BUTTON_STATE_PRESS_SHORT:
         if ((bt->port->PIN & (0x01 << bt->bit)))
         {
            bt->state = BUTTON_STATE_IDLE;
            bt->type = BUTTON_PRESS_SHORT;
         }
         else  if (bt->timer >= bt->timeout)
         {
            bt->state = BUTTON_STATE_PRESS_LONG;
            bt->timer = 0;
            bt->timeout = BUTTON_TIMEOUT_PRESS_LONG;
         }
         break;
      case BUTTON_STATE_PRESS_LONG:
         if ((bt->port->PIN & (0x01 << bt->bit)))
         {
            bt->state = BUTTON_STATE_IDLE;
            bt->type = BUTTON_PRESS_SHORT;
         }
         else  if (bt->timer >= bt->timeout)
         {
            bt->state = BUTTON_STATE_PRESS_LONG_WAIT_RELEASE;
            bt->type = BUTTON_PRESS_LONG;
         }
         break;
      case BUTTON_STATE_PRESS_LONG_WAIT_RELEASE:
         if ((bt->port->PIN & (0x01 << bt->bit)))
         {
            bt->state = BUTTON_STATE_IDLE;
         }
         break;
   }
}

#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
   volatile unsigned short tempIV = P1IV;
   LPM4_EXIT;
}

#pragma vector=PORT2_VECTOR
__interrupt void PORT2_ISR(void)
{
   volatile unsigned short tempIV = P2IV;
   LPM4_EXIT;
}