/* \file serial.c
 * \brief Implementacao da comunicacao serial com o host.
 */

#include "watchdog.h"
#include "serial.h"
#include "radio.h"

// prototipos dos metodos
void serialInit (void * pserial);
void serialTransmit (void * pserial, unsigned char * data,...);
void serialProcessBuffRx (void * pserial);
void serialPutMessage (void * pserial, SERIAL_MESSAGE message);
char serialGetMessage (void * pserial, SERIAL_MESSAGE * message);
void serialClearMessages (void * pserial);
void serialReset (void * pserial);

void IDtoASCII(unsigned char * input, unsigned char * output);

// instancias das maquinas seriais
SERIAL serial1 = {serialInit};

/* \brief Inicializa os metodos e as variaveis de cada instancia.*/
void serialInit (void * pserial)
{
   SERIAL * serial = (SERIAL *)pserial;
   
   serial->transmit = serialTransmit;
   serial->processBuffRx = serialProcessBuffRx;
   serial->putMessage = serialPutMessage;
   serial->getMessage = serialGetMessage;
   serial->clearMessages = serialClearMessages;
   serial->reset = serialReset;
   
   serial->uart = &uart1;

   serial->uart->init(serial->uart);
   serial->uart->config(serial->uart, UART_SPEED_115200, UART_BITS_8, UART_PARITY_NONE, UART_STOP_BITS_1);
   serial->uart->start(serial->uart);
   serial->reset(serial);
}

/* \brief Transmite uma string pela serial.*/
void serialTransmit (void * pserial, unsigned char * data,...)
{
   SERIAL * serial = (SERIAL *)pserial;
   
   va_list arguments; // lista de parametros variavel
   unsigned char charCount = 0;
   char charBuff[16];
   unsigned char i;
   va_start(arguments, ptr);

   for (; *data != 0; ++data)
   {
      if (*data != '%')
      {
         serial->uart->putBuffTx(serial->uart, *data);
      }
      else
      {
         ++data;
         switch(*data)
         {
            case 'I':
               IDtoASCII(va_arg ( arguments, char * ), (unsigned char *)charBuff);
               for (i = 0; i < 8; i++)
               {
                  serial->uart->putBuffTx(serial->uart,charBuff[i]);
               }
               break;
            case '%':
               serial->uart->putBuffTx(serial->uart,*data);
               break;
            case 'd':
            case 'i':
               charCount = 0;//intToStr( va_arg ( arguments, int ), (unsigned char *)charBuff);
               for (i = 0; i < charCount; i++)
               {
                  serial->uart->putBuffTx(serial->uart,charBuff[i]);
               }
               break;
            case 'c':
               serial->uart->putBuffTx(serial->uart, va_arg ( arguments, unsigned char ));
               break;
            case 's':
               {
                  char * stringPtr;
                  stringPtr = va_arg ( arguments, char * );
                  for (; *stringPtr != 0; ++stringPtr)
                  {
                     serial->uart->putBuffTx(serial->uart,*stringPtr);
                  }
               }
         }
      }
   }
}

/* \brief Processa os bytes do buffer de recepcao serial.*/
void serialProcessBuffRx (void * pserial)
{
   SERIAL * serial = (SERIAL *)pserial;
   unsigned char tempByte;
   
   if (serial->uart->getBuffRx(serial->uart, &tempByte))
   {
      if(tempByte == '@')
      {
          //Problema na mensagem, executa o reset do processador
          radio1.receiveOff(&radio1);
          radio1.receiveOn(&radio1);
          //radioStrobe(0x3A); 
      }
      else if (tempByte == '!')
      {
          serial->putMessage(serial, SERIAL_MESSAGE_ACK);
      }
     
      switch(serial->state)
      {
         case SERIAL_STATE_IDLE:
            switch(tempByte)
            {
               case 'S':
                  serial->state = SERIAL_STATE_SENSOR;
                  break;
               case 'C':
                  serial->state = SERIAL_STATE_CHANNEL;
                  break;
               case 'M':
                  serial->state = SERIAL_STATE_MODE;
                  break;
               case 'T':
                  serial->state = SERIAL_STATE_TIMEOUT;
                  break;
            }
            break;
         case SERIAL_STATE_SENSOR:
            switch(tempByte)
            {
               case 'W':
                  serial->state = SERIAL_STATE_SENSOR_WRITE;
                  serial->var1Len = 0;
                  break;
               case 'E':
                  serial->state = SERIAL_STATE_SENSOR_ERASE;
                  serial->var1Len = 0;
                  break;
               case 'L':
                  serial->state = SERIAL_STATE_IDLE;
                  serial->putMessage(serial, SERIAL_MESSAGE_SENSOR_LIST);
                  break;
               default:
                  serial->state = SERIAL_STATE_IDLE;
            }
            break;
         case SERIAL_STATE_SENSOR_WRITE:
            serial->var1[serial->var1Len++] = tempByte;
            if (serial->var1Len >= 4)
            {
               serial->state = SERIAL_STATE_IDLE;
               serial->putMessage(serial, SERIAL_MESSAGE_SENSOR_WRITE);
            }
            break;
         case SERIAL_STATE_SENSOR_ERASE:
            serial->var1[serial->var1Len++] = tempByte;
            if (serial->var1Len >= 4)
            {
               serial->state = SERIAL_STATE_IDLE;
               serial->putMessage(serial, SERIAL_MESSAGE_SENSOR_ERASE);
            }
            break;
         case SERIAL_STATE_CHANNEL:
            switch(tempByte)
            {
               case 'S':
                  serial->state = SERIAL_STATE_CHANNEL_SET;
                  break;
               case 'R':
                  serial->state = SERIAL_STATE_IDLE;
                  serial->putMessage(serial, SERIAL_MESSAGE_CHANNEL_READ);
                  break;
               default:
                  serial->state = SERIAL_STATE_IDLE;
            }
            break;
         case SERIAL_STATE_CHANNEL_SET:
            serial->var1[serial->var1Len++] = tempByte;
            if (serial->var1Len >= 1)
            {
               serial->state = SERIAL_STATE_IDLE;
               serial->putMessage(serial, SERIAL_MESSAGE_CHANNEL_SET);
            }
            break;
         case SERIAL_STATE_MODE:
            switch(tempByte)
            {
               case 'S':
                  serial->state = SERIAL_STATE_IDLE;
                  serial->putMessage(serial, SERIAL_MESSAGE_MODE_SEARCH);
                  break;
               case 'R':
                  serial->state = SERIAL_STATE_IDLE;
                  serial->putMessage(serial, SERIAL_MESSAGE_MODE_RECEIVE);
                  break;
                case 'I':
                  serial->state = SERIAL_STATE_IDLE;
                  serial->putMessage(serial, SERIAL_MESSAGE_MODE_INVENTORY);
                  break;
               case 'D':
                  serial->state = SERIAL_STATE_IDLE;
                  serial->putMessage(serial, SERIAL_MESSAGE_MODE_DEBUG);
                  break;
               default:
                  serial->state = SERIAL_STATE_IDLE;
            }
            break;
         case SERIAL_STATE_TIMEOUT:
            switch(tempByte)
            {
               case 'S':
                  serial->state = SERIAL_STATE_TIMEOUT_WRITE;
                  serial->var1Len = 0;
                  break;
               case 'R':
                  serial->state = SERIAL_STATE_IDLE;
                  serial->putMessage(serial, SERIAL_MESSAGE_TIMEOUT_READ);
                  break;
               default:
                  serial->state = SERIAL_STATE_IDLE;
            }
            break;
         case SERIAL_STATE_TIMEOUT_WRITE:
            serial->var1[serial->var1Len++] = tempByte;
            if (serial->var1Len >= 1)
            {
               serial->state = SERIAL_STATE_IDLE;
               serial->putMessage(serial, SERIAL_MESSAGE_TIMEOUT_SET);
            }
            break;
      }
   }
}

/* \brief Coloca na fila uma mensagem recebida pela serial.*/
void serialPutMessage (void * pserial, SERIAL_MESSAGE message)
{
   SERIAL * serial = (SERIAL *)pserial;
   
   serial->messages[serial->msgPtrIn] = message;
   serial->msgPtrIn++;
   serial->msgPtrIn &= (SERIAL_MESSAGE_QUEUE_SIZE-1);
}

/* \brief Le uma mensagem recebida pela serial.*/
char serialGetMessage (void * pserial, SERIAL_MESSAGE * message)
{
   SERIAL * serial = (SERIAL *)pserial;
   
   if(serial->msgPtrIn != serial->msgPtrOut)
   {
      *message = serial->messages[serial->msgPtrOut++];
      serial->msgPtrOut &= (SERIAL_MESSAGE_QUEUE_SIZE-1);
          return(1);
   }
   return(0);
}

/* \brief Limpa a fila de mensagens seriais recebidas.*/
void serialClearMessages (void * pserial)
{
   SERIAL * serial = (SERIAL *)pserial;
   serial->msgPtrIn = 0;
   serial->msgPtrOut = 0;
}

/* \brief Reinicializa a maquina serial.*/
void serialReset (void * pserial)
{
   SERIAL * serial = (SERIAL *)pserial;
   
   serial->state = SERIAL_STATE_IDLE;
   serial->subState = 0;
   serial->tempMessage = SERIAL_MESSAGE_NONE;
   serial->messages[0] = SERIAL_MESSAGE_NONE;
   serial->msgPtrIn = 0;
   serial->msgPtrOut = 0;
}

const unsigned char nibbleToASCII[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

void IDtoASCII(unsigned char * input, unsigned char * output)
{
   for(unsigned char i = 0; i < 4; i++)
   {
      *output = nibbleToASCII[(*input)>>4];
      ++output;
      *output = nibbleToASCII[(*input)&0x0F];
      ++output;
      ++input;
   }
}