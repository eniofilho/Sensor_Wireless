/*! \file led.c
 *  \brief implementacao do objeto led.
 */

#include "led.h"
#include "cc430x513x.h"

// prototipos dos metodos
void ledInit          (void * pled, unsigned char port, unsigned char bit);
void ledOn            (void * pled);
void ledOff           (void * pled);
void ledBlink         (void * pled, unsigned char blinks, unsigned short onTime, unsigned short offTime, unsigned short pauseTime, unsigned char repetitions);
LED_STATE ledGetState (void * pled);
void ledRun           (void *pled);

// instancias dos leds
LED led1 = {ledInit};

// implementacao dos metodos
void ledInit          (void * pled, unsigned char port, unsigned char bit)
{
   LED * led = (LED *)pled;
   
   led->on = ledOn;
   led->off = ledOff;
   led->blink = ledBlink;
   led->getState = ledGetState;
   led->run = ledRun;
   
   switch(port)
   {
      case 0:
         led->port = (GPIO_PORT *)0x0200;
         break;
      case 1:
         led->port = (GPIO_PORT *)0x0220;
         break;
      case 2:
         led->port = (GPIO_PORT *)0x0240;
         break;
   }
   led->bit = bit;
   
   led->state = LED_STATE_OFF;
   led->tOn = 0;
   led->tOff = 0;
   led->tPause = 0;
   led->blinks = 0;
   led->repetitions = 0;
   led->timer = 0;
   led->timeout = 0;
   
   // configura o pino
   led->port->PSEL &= ~(0x01 << led->bit);
   led->port->PDIR |= (0x01 << led->bit);
   led->port->POUT &= ~(0x01 << led->bit);
}

void ledOn            (void * pled)
{
   LED * led = (LED *)pled;
   led->state = LED_STATE_ON;
   
   // configura o pino
   led->port->POUT |= (0x01 << led->bit);
}

void ledOff           (void * pled)
{
   LED * led = (LED *)pled;
   led->state = LED_STATE_OFF;
   
   // configura o pino
   led->port->POUT &= ~(0x01 << led->bit);
}

void ledBlink         (void * pled, unsigned char blinks, unsigned short onTime, unsigned short offTime, unsigned short pauseTime, unsigned char repetitions)
{
   LED * led = (LED *)pled;
   
   led->tOff = offTime;
   led->tOn = onTime;
   led->tPause = pauseTime;
   led->blinks = blinks;
   led->repetitions = repetitions;
   led->tempBlinks = blinks;
   led->timer = 0;
   led->off(led);
   led->state = LED_STATE_BL_PAUSE;
}

LED_STATE ledGetState (void * pled)
{
   LED * led = (LED *)pled;
   
   return led->state;
}

void ledRun      (void *pled)
{
   LED * led = (LED *)pled;
   
   ++led->timer;
   
   switch (led->state)
   {
      case LED_STATE_OFF:
      case LED_STATE_ON:
         break;
      case LED_STATE_BL_OFF:
         if (led->timer >= led->tOff)
         {
            led->timer = 0;
            if (led->tempBlinks)
            {
               --led->tempBlinks;
               led->state = LED_STATE_BL_ON;
               led->port->POUT |= (0x01 << led->bit);
            }
            else
            {
               led->state = LED_STATE_BL_PAUSE;
            }
         }
         break;
      case LED_STATE_BL_ON:
         if (led->timer >= led->tOn)
         {
            led->timer = 0;
            led->state = LED_STATE_BL_OFF;
            led->port->POUT &= ~(0x01 << led->bit);
         }
         break;
      case LED_STATE_BL_PAUSE:
         if (led->timer >= led->tPause)
         {
            led->timer = 0;
            if (led->repetitions)
            {
               --led->repetitions;
               led->state = LED_STATE_BL_OFF;
               led->tempBlinks = led->blinks;
               led->port->POUT &= ~(0x01 << led->bit);
            }
            else
            {
               led->state = LED_STATE_OFF;
               led->port->POUT &= ~(0x01 << led->bit);
            }
         }
         break;
   }
}
