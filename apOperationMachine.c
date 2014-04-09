/*! \file apOperationMachine.c
 *  \brief implementacao da maquina de operacao do modo access point.
 */

#include "apOperationMachine.h"
#include "cc430x513x.h"
#include "scrambler.h"

// defines
#define SYS_CLK       12000000UL
#define PRE_SCALER    8
#define OP_FREQ       100
#define FREQ_COUNTER  ((SYS_CLK / PRE_SCALER) / OP_FREQ)

#define OPERATION_MACHINE_MAX_CHANNELS 8

#define DEFAULT_COMM_TIMEOUT 5

#define COMM_MAX_TIMEOUT 5
const unsigned short timeoutList[] = {1*OP_FREQ, 2*OP_FREQ, 4*OP_FREQ, 8*OP_FREQ, 16*OP_FREQ, 32*OP_FREQ};

unsigned char discoveryAckPkg[] = {   14,                  // tamanho do pacote
                                    0x00,                  // endereco do AP
                                    0x00, 0x00, 0x00,      // semente do scrambler
                                    'D','A','C','K',       // payload
                                    0x31, 0x32, 0x33, 0x34,// ID do sensor
                                    0x30,                  // tipo do sensor
                                    0x46                   // checksum
                                  };

unsigned char statusAckPkg[]   =  {   14,                  // tamanho do pacote
                                    0x00,                  // endereco do AP
                                    0x00, 0x00, 0x00,      // semente do scrambler
                                    'S','A','C','K',       // payload
                                    0x31, 0x32, 0x33, 0x34,// ID do sensor
                                    0x30,                  // tipo do sensor
                                    0x46                   // checksum
                                  };

// prototipos dos metodos do objeto
void opInit       (void * pOp);
void opRun        (void * pOp);
void opSetState   (void * pOp, OPERATION_MACHINE_STATE state);
void opSetTimeout (void * pOp, unsigned short timeout);
void opIncTimer   (void * pOp);
SENSOR_WRITE_STATUS opSensorWrite(void * pOp, unsigned char * sensorID, unsigned char sensorLen);
SENSOR_ERASE_STATUS opSensorErase(void * pOp, unsigned char * sensorID, unsigned char sensorLen);
signed char opSensorGetPos       (void * pOp, unsigned char * sensorID);
unsigned char opSensorGetCount   (void * pOp);

void valueToASCII (unsigned char * input, unsigned char * output);

__no_init OPERATION_MACHINE operationMachine;// = {opInit};

// implementacao dos metodos
void opInit       (void * pOp)
{
   OPERATION_MACHINE * op = (OPERATION_MACHINE *)pOp;
   
   unsigned short tempIV = 0;
   
   op->run = opRun;
   op->setState = opSetState;
   op->setTimeout = opSetTimeout;
   op->incTimer = opIncTimer;
   op->sensorWrite = opSensorWrite;
   op->sensorErase = opSensorErase;
   op->sensorGetPos = opSensorGetPos;
   op->sensorGetCount = opSensorGetCount;
   
   op->radio = &radio1;
   op->serial = &serial1;
   op->flash = &flashParam;
   
   op->channel = 0;
   
   op->commTimeout = DEFAULT_COMM_TIMEOUT;
   
   op->earlyMessage = 0;

   //inicializa a lista de sensores
   op->sensorsFound = op->sensorGetCount(op);
   tempIV = SYSRSTIV;
   if ((tempIV != 0x16) && (tempIV != 0x18)) // se nao resetou pelo watchdog.
   {
      for (unsigned char i = 0; i < op->sensorsFound; i++)
      {
         op->sensorStatus[i] = SENSOR_COMM_STATUS_NOK;
         op->sensorValue[i][0] = 0;
         op->sensorValue[i][1] = 0; 
         op->sensorValue[i][2] = 0; 
         op->sensorValue[i][3] = 0; 
         op->sensorType[i] = 0;
      }
   }
   for (unsigned char i = 0; i < SENSOR_LIST_SIZE; i++)
   {
      op->sensorStatus[i] = SENSOR_COMM_STATUS_NOT_PRESENT;
   }
   // inicializa o radio
   op->radio->init(op->radio);
   
   // inicializa a serial
   op->serial->init(op->serial);
   
   // faz com que va direto para o modo receive
   op->serial->putMessage(op->serial, SERIAL_MESSAGE_MODE_RECEIVE_INIT);
   
   // inicializa a flash
   op->flash->init();
   
   // manda pro estado inicial da maquina
   op->setState(op, OPERATION_MACHINE_STATE_IDLE);
   //op->setState(op, OPERATION_MACHINE_STATE_DEBUG);
   //op->radio->receiveOn(op->radio);
   
   // inicializa o timer para a temporizacao
   // configura o timer
   TA1CCTL0 = CCIE;                          // CCR0 interrupt enabled
   TA1CCR0 = FREQ_COUNTER;
   TA1CTL = TASSEL_2 + MC_1 + TACLR + ID_3;  // SMCLK, upmode, pre-scaler /8, clear TAR
   
   op->serial->timeoutSerial = 0;
   op->serial->transmit(op->serial, "#");
   
   op->wdtControl = 0;
}

void opRun        (void * pOp)
{
   SERIAL_MESSAGE serialMessage;
   unsigned char i = 0;
   unsigned char * tempPtr;
   SENSOR_WRITE_STATUS ret;
   SENSOR_ERASE_STATUS retE;
   
   OPERATION_MACHINE * op = (OPERATION_MACHINE *)pOp;
   
   op->serial->processBuffRx(op->serial);
   if (op->serial->getMessage(op->serial, &serialMessage))
   {
      switch(serialMessage)
      {
         case SERIAL_MESSAGE_SENSOR_WRITE:
            ret = op->sensorWrite(op, op->serial->var1, op->serial->var1Len);
            if (ret == SENSOR_WRITE_STATUS_OK)
            {
               op->sensorsFound++;
               op->serial->transmit(op->serial, "\rOK\r");
            }
            else if (ret == SENSOR_WRITE_STATUS_ALREADY_ON_LIST)
            {
               op->serial->transmit(op->serial, "\rALREADY ON LIST\r");
            }
            else
            {
               op->serial->transmit(op->serial, "\rERROR\r");
            }
            break;
         case SERIAL_MESSAGE_SENSOR_ERASE:
            retE = op->sensorErase(op, op->serial->var1, op->serial->var1Len);
            if (retE == SENSOR_ERASE_STATUS_OK)
            {
               op->sensorsFound--;
               op->serial->transmit(op->serial, "\rOK\r");
            }
            else if (retE == SENSOR_ERASE_STATUS_NOT_FOUND)
            {
               op->serial->transmit(op->serial, "\rSENSOR NOT ON LIST\r");
            }
            else
            {
               op->serial->transmit(op->serial, "\rERRO\r");
            }
            break;
         case SERIAL_MESSAGE_SENSOR_LIST:
            op->serial->transmit(op->serial, "{");
            i = 0;
            tempPtr = op->tempBuff;
            while( (op->flash->sensors[i][SENSOR_ID_SIZE] != 0xFF) && (i < SENSOR_LIST_SIZE))
            {
               for (unsigned char j = 0; j < (SENSOR_ID_SIZE); j++)
               {
                  *tempPtr = op->flash->sensors[i][j];
                  ++tempPtr;
               }
               *tempPtr = '\r';
               ++tempPtr;
               ++i;
            }
            *tempPtr = '\n';
            ++tempPtr;
            *tempPtr = '}';
            ++tempPtr;
            *tempPtr = 0;
            ++tempPtr;
            op->serial->transmit(op->serial, op->tempBuff);
            break;
         case SERIAL_MESSAGE_CHANNEL_SET:
            if ( (op->serial->var1[0]  > '0') && (op->serial->var1[0] < ( '0' + OPERATION_MACHINE_MAX_CHANNELS)))
            {
               op->channel = op->serial->var1[0] - '0';
               op->serial->transmit(op->serial, "\rOK\r");
            }
            else
            {
               op->serial->transmit(op->serial, "\rERRO\r");
            }
            break;
         case SERIAL_MESSAGE_CHANNEL_READ:
            op->serial->transmit(op->serial, "\rCHANNEL: %c\r", (op->channel + '0'));
            break;
         case SERIAL_MESSAGE_MODE_SEARCH:
            op->serial->transmit(op->serial, "\rMODE: SEARCH\r");
            
            op->radio->receiveOn(op->radio);
            
            op->setState(op, OPERATION_MACHINE_STATE_SCAN_WAIT);
            op->setTimeout(op, 10 * 100);
            //op->sensorsFound = 0;
            op->serial->transmit(op->serial, "[Modo Busca      \r %c%c encontrados ]", ((op->sensorsFound)/10) + '0', ((op->sensorsFound%10) + '0'));
            break;
         case SERIAL_MESSAGE_MODE_RECEIVE:
           op->serial->transmit(op->serial, "\rMODE: RECEIVE\r");
           
           op->radio->receiveOn(op->radio);
            
            op->setState(op, OPERATION_MACHINE_STATE_RECEIVE_WAIT);
            op->setTimeout(op, timeoutList[op->commTimeout]);
            
            op->sensorsFound = op->sensorGetCount(op);
            for (unsigned char i = 0; i < op->sensorsFound; i++)
            {
               op->sensorStatus[i] = SENSOR_COMM_STATUS_NOK;
            }
            for (unsigned char i = 0; i < SENSOR_LIST_SIZE; i++)
            {
               op->sensorStatus[i] = SENSOR_COMM_STATUS_NOT_PRESENT;
            }
            break;
            
         case SERIAL_MESSAGE_MODE_RECEIVE_INIT:
            op->radio->receiveOn(op->radio);
            
            op->setState(op, OPERATION_MACHINE_STATE_RECEIVE_WAIT);
            op->setTimeout(op, timeoutList[op->commTimeout]);
            
            op->sensorsFound = op->sensorGetCount(op);
            for (unsigned char i = 0; i < SENSOR_LIST_SIZE; i++)
            {
               op->sensorStatus[i] = SENSOR_COMM_STATUS_NOT_PRESENT;
            }
            break;
         case SERIAL_MESSAGE_MODE_INVENTORY:
           op->serial->transmit(op->serial, "\rMODE: INVENTORY\r");
            
            op->radio->receiveOn(op->radio);
            
            op->setState(op, OPERATION_MACHINE_STATE_INVENTORY_WAIT);
            op->setTimeout(op, timeoutList[op->commTimeout]);
            break;
         case SERIAL_MESSAGE_MODE_DEBUG:
            op->serial->transmit(op->serial, "\rMODE: DEBUG\r");
            
            op->radio->receiveOn(op->radio);
            
            op->setState(op, OPERATION_MACHINE_STATE_DEBUG);
            break;
         case SERIAL_MESSAGE_TIMEOUT_READ:
            op->serial->transmit(op->serial, "\rTIMEOUT ATUAL : %c\r", (op->commTimeout + '0'));
            break;
         case SERIAL_MESSAGE_TIMEOUT_SET:
            if ( (op->serial->var1[0]  > '0') && (op->serial->var1[0] <= ( '0' + COMM_MAX_TIMEOUT)))
            {
               op->commTimeout = op->serial->var1[0] - '0';
               op->serial->transmit(op->serial, "\rOK\r");
            }
            else
            {
               op->serial->transmit(op->serial, "\rERRO\r");
            }
            break;
         case SERIAL_MESSAGE_ACK:
            op->serial->timeoutSerial = 0;
            break;
      }
   }
   
   switch(op->state)
   {
      case OPERATION_MACHINE_STATE_IDLE:
         
         break;
      case OPERATION_MACHINE_STATE_SCAN_WAIT:
         op->radio->isr();
      
         if (op->radio->getData(op->radio, op->tempBuff, &(op->tempLen)))
         {
            if ((op->tempBuff[0] - 6) > 14) op->tempBuff[0] = 20; // so processa mensagens de ate 20 caracteres
            descrambler (&(op->tempBuff[5]), op->message, op->tempBuff[0] - 6, &(op->tempBuff[2]));
            
            if ( (op->message[0] == 'D') &&
                 (op->message[1] == 'I') &&
                 (op->message[2] == 'S') &&
                 (op->message[3] == 'C')   )
            {
               ret = op->sensorWrite(op, &(op->message[4]), 4);
               if (ret == SENSOR_WRITE_STATUS_OK)
               {
                  //op->serial->transmit(op->serial, "\rNEW SENSOR: %s\r", &(op->message[4]));
                  op->setState(op, OPERATION_MACHINE_STATE_SCAN_ACK);
                  ++op->sensorsFound;
                  op->serial->transmit(op->serial, "[Modo Busca      \r %c%c encontrados ]\r", ((op->sensorsFound)/10) + '0', ((op->sensorsFound%10) + '0'));
               }
               else if (ret == SENSOR_WRITE_STATUS_ALREADY_ON_LIST)
               {
                  //op->serial->transmit(op->serial, "\rSENSOR ALREADY ON LIST: %s\r", &(op->message[4]));
                  op->setState(op, OPERATION_MACHINE_STATE_SCAN_ACK);
                  //++op->sensorsFound;
               }
               else
               {
                  //op->serial->transmit(op->serial, "\rSENSOR LIST FULL OR SENSOR ALREADY ON LIST\r");
               }
            }
         }
         else if (op->timer >= op->timeout)
         {
            //op->serial->transmit(op->serial, "\rSENSOR SCAN TIMEOUT. %c SENSORS FOUND!\r", (op->sensorsFound + '0'));
            //op->setState(op, OPERATION_MACHINE_STATE_IDLE);
            op->timer = 0;
         }
         break;
      case OPERATION_MACHINE_STATE_SCAN_ACK:
         //embaralha a mensagem antes de enviar
         op->tempBuff[0 ] = sizeof(discoveryAckPkg) - 1; // tamanho do payload
         op->tempBuff[1 ] = 0x00;                     // endereco do ED
         op->tempBuff[2 ] = SCRAMBLER_SEED1;          // semente do scrambler
         op->tempBuff[3 ] = SCRAMBLER_SEED2;          // semente do scrambler
         op->tempBuff[4 ] = SCRAMBLER_SEED3;          // semente do scrambler
         discoveryAckPkg[9 ] = op->message[4];    // ID do sensor
         discoveryAckPkg[10] = op->message[5];    // ID do sensor
         discoveryAckPkg[11] = op->message[6];    // ID do sensor
         discoveryAckPkg[12] = op->message[7];    // ID do sensor
         discoveryAckPkg[13] = op->message[8];    // ID do sensor
         
         scrambler (&(discoveryAckPkg[5]), &(op->tempBuff[5]), sizeof(discoveryAckPkg) - 5, &(op->tempBuff[2]));
         
         op->radio->transmit(op->radio, op->tempBuff, sizeof(discoveryAckPkg));
         op->radio->receiveOn(op->radio);
         
         op->setState(op, OPERATION_MACHINE_STATE_SCAN_WAIT);
         op->setTimeout(op, 50 * 100);
         break;
      case OPERATION_MACHINE_STATE_RECEIVE_WAIT:
         op->radio->isr();
      
         if (op->radio->getData(op->radio, op->tempBuff, &(op->tempLen)))
         {
            signed char tempPos;
            if ((op->tempBuff[0] - 4) > 14) op->tempBuff[0] = 18; // so processa mensagens de ate 20 caracteres
            descrambler (&(op->tempBuff[5]), op->message, op->tempBuff[0] - 4, &(op->tempBuff[2]));
            op->message[op->tempBuff[0]-5] = 0;
            //op->serial->transmit(op->serial, "\r RX DATA: %s\r", op->message);
            
            tempPos = op->sensorGetPos(op, &(op->message[0]));
            if (tempPos != -1)
            {
               unsigned char tempValue[4];
               op->sensorStatus[tempPos] = SENSOR_COMM_STATUS_OK;
               valueToASCII ((unsigned char *)&(op->message[5]), tempValue);
               if (tempValue[0] != op->sensorValue[tempPos][0])
               {
                  op->setTimeout(op, 1);
                  op->earlyMessage = 1;
               }
               for (unsigned char j = 0; j < 4; j++) op->sensorValue[tempPos][j] = tempValue[j];
               //op->sensorType[tempPos] = op->message[4];
            }
            
            //op->setState(op, OPERATION_MACHINE_STATE_RECEIVE_ACK);
            op->state = OPERATION_MACHINE_STATE_RECEIVE_ACK; // para nao mexer no timeout
         }
         else if (op->timer >= op->timeout)
         {
            unsigned char i;
            op->sensorStatus[SENSOR_LIST_SIZE] = SENSOR_COMM_STATUS_NOT_PRESENT;
            //op->serial->transmit(op->serial, "\rSENSOR COMM STATUS: %s \r", op->sensorStatus);
            op->serial->transmit(op->serial, "<");
            for (i = 0; i < op->sensorsFound; i++)
            {
               op->serial->transmit(op->serial, "%I%c%s", &(op->flash->sensors[i]),op->flash->sensors[i][4], (op->sensorStatus[i] == SENSOR_COMM_STATUS_OK)?(op->sensorValue[i][1] == '0')?"0000":"FFFF":"????" );
               if (i <= (op->sensorsFound) - 2) op->serial->transmit(op->serial, ",");
            }
            op->serial->transmit(op->serial, ">\r");
            op->setTimeout(op, timeoutList[op->commTimeout]);
            
            if(op->earlyMessage == 0)
            {
              for (i = 0; i < op->sensorsFound; i++)
              {
                op->sensorStatus[i] = SENSOR_COMM_STATUS_NOK;
              }
            }
            else
            {
              op->earlyMessage = 0;
            }
            
            for (; i < SENSOR_LIST_SIZE; i++)
            {
               op->sensorStatus[i] = SENSOR_COMM_STATUS_NOT_PRESENT;
            }
         }
         break;
      case OPERATION_MACHINE_STATE_RECEIVE_ACK:
         //embaralha a mensagem antes de enviar
         op->tempBuff[0 ] = sizeof(discoveryAckPkg) - 1; // tamanho do payload
         op->tempBuff[1 ] = 0x00;                     // endereco do ED
         op->tempBuff[2 ] = SCRAMBLER_SEED1;          // semente do scrambler
         op->tempBuff[3 ] = SCRAMBLER_SEED2;          // semente do scrambler
         op->tempBuff[4 ] = SCRAMBLER_SEED3;          // semente do scrambler
         statusAckPkg[9 ] = op->message[4];    // ID do sensor
         statusAckPkg[10] = op->message[5];    // ID do sensor
         statusAckPkg[11] = op->message[6];    // ID do sensor
         statusAckPkg[12] = op->message[7];    // ID do sensor
         statusAckPkg[13] = op->message[8];    // ID do sensor
         
         scrambler (&(statusAckPkg[5]), &(op->tempBuff[5]), sizeof(statusAckPkg) - 5, &(op->tempBuff[2]));
         
         op->radio->transmit(op->radio, op->tempBuff, sizeof(discoveryAckPkg));
         op->radio->receiveOn(op->radio);
         
         //op->setState(op, OPERATION_MACHINE_STATE_RECEIVE_WAIT);
         op->state = OPERATION_MACHINE_STATE_RECEIVE_WAIT; // para nao mexer no timeout
         
         break;
      case OPERATION_MACHINE_STATE_INVENTORY_WAIT:
         op->radio->isr();
      
         if (op->radio->getData(op->radio, op->tempBuff, &(op->tempLen)))
         {
            if ((op->tempBuff[0] - 6) > 14) op->tempBuff[0] = 20; // so processa mensagens de ate 20 caracteres
            descrambler (&(op->tempBuff[5]), op->message, op->tempBuff[0] - 6, &(op->tempBuff[2]));
            op->message[4] = 0;
            op->serial->transmit(op->serial, "(%I)\r", &(op->message[0]));
         }
         else if (op->timer >= op->timeout)
         {
            op->setState(op, OPERATION_MACHINE_STATE_IDLE);
         }
         break;
      case OPERATION_MACHINE_STATE_DEBUG:
         op->radio->isr();
      
         if (op->radio->getData(op->radio, op->tempBuff, &(op->tempLen)))
         {
            descrambler (&(op->tempBuff[5]), op->message, op->tempBuff[0], &(op->tempBuff[2]));
            op->message[op->tempBuff[0]-1] = 0;
            op->serial->transmit(op->serial, "\r DEBUG DATA: %s\r", op->message[2]);
            op->tempBuff[op->tempBuff[0]-1] = 0;
            op->serial->transmit(op->serial, "%s", &(op->tempBuff[2]));
         }
         break;
   }
}

void opSetState   (void * pOp, OPERATION_MACHINE_STATE state)
{
   OPERATION_MACHINE * op = (OPERATION_MACHINE *)pOp;
   
   op->state = state;
   op->setTimeout(op, 0);
}

void opSetTimeout (void * pOp, unsigned short timeout)
{
   OPERATION_MACHINE * op = (OPERATION_MACHINE *)pOp;
   
   op->timeout = timeout;
   op->timer = 0;
}

void opIncTimer   (void * pOp)
{
   OPERATION_MACHINE * op = (OPERATION_MACHINE *)pOp;
   
   ++op->timer;
   ++op->serial->timeoutSerial;
   ++op->radio->timer;
   op->wdtControl = 1;
}

SENSOR_WRITE_STATUS opSensorWrite(void * pOp, unsigned char * sensorID, unsigned char sensorLen)
{
   unsigned char sensorCount = 0;
   unsigned char match = 0;
   
   OPERATION_MACHINE * op = (OPERATION_MACHINE *)pOp;
   
   // verifica se o sensor ja esta cadastrado
   while ((op->flash->sensors[sensorCount][SENSOR_ID_SIZE] != 0xFF) && (sensorCount < SENSOR_LIST_SIZE) && (match == 0))
   {
      match = 1;
      for (unsigned char j = 0; j < SENSOR_ID_SIZE; j++)
      {
         if (op->flash->sensors[sensorCount][j] != sensorID[j])
         {
            match = 0;
         }
      }
      ++sensorCount;
   }
   
   if ((match == 1) && (sensorCount < SENSOR_LIST_SIZE))
   {  // ja esta cadastrado retorna erro
      return SENSOR_WRITE_STATUS_ALREADY_ON_LIST;
   }
   
   // procura o primeiro espaco vazio da lista
   sensorCount = 0;
   while((op->flash->sensors[sensorCount][SENSOR_ID_SIZE] != 0xFF) && (sensorCount < SENSOR_LIST_SIZE))
   {
      ++sensorCount;
   }
   
   // grava o ID do sensor
   if ((sensorCount < (SENSOR_LIST_SIZE)) && (sensorLen == SENSOR_ID_SIZE))
   {
      for (unsigned char j = 0; j < SENSOR_ID_SIZE; j++)
      {
         op->flash->sensors[sensorCount][j] = sensorID[j];
      }
      op->flash->sensors[sensorCount][SENSOR_ID_SIZE] = 0x30;
      op->flash->update();
      return SENSOR_WRITE_STATUS_OK;
   }
   else
   {
      return SENSOR_WRITE_STATUS_ERROR;
   }
}

SENSOR_ERASE_STATUS opSensorErase(void * pOp, unsigned char * sensorID, unsigned char sensorLen)
{
   unsigned char sensorCount = 0;
   unsigned char match;

   OPERATION_MACHINE * op = (OPERATION_MACHINE *)pOp;

   match = 0;
   while((op->flash->sensors[sensorCount][SENSOR_ID_SIZE] != 0xFF) && (sensorCount < SENSOR_LIST_SIZE) && (match == 0))
   {
      match = 1;
      for (unsigned char j = 0; j < SENSOR_ID_SIZE; j++)
      {
         if (sensorID[j] != op->flash->sensors[sensorCount][j])
         {
            match = 0;
         }
      }
      if (match)
      {
         if (sensorLen == SENSOR_ID_SIZE)
         {
            unsigned char * tempPtr = &(op->flash->sensors[sensorCount][0]);
            while (tempPtr < ((&(op->flash->sensors[SENSOR_LIST_SIZE][0])) - 1 - SENSOR_ID_SIZE - SENSOR_TYPE_SIZE))
            {
               *tempPtr = *(tempPtr + SENSOR_ID_SIZE + SENSOR_TYPE_SIZE);
               ++tempPtr;
            }
            for (unsigned char j = 0; j < (SENSOR_ID_SIZE + SENSOR_TYPE_SIZE); j++)
            {
               *tempPtr = 0xFF;
               ++tempPtr;
            }
            op->flash->update();
            return SENSOR_ERASE_STATUS_OK;
         }
         else
         {
            return SENSOR_ERASE_STATUS_ERROR;
         }
      }
      else
      {
         ++sensorCount;
      }
   }
   return SENSOR_ERASE_STATUS_NOT_FOUND;
}

signed char opSensorGetPos (void * pOp, unsigned char * sensorID)
{
   unsigned char match;
   unsigned char sensorCount = 0;

   OPERATION_MACHINE * op = (OPERATION_MACHINE *)pOp;

   match = 0;
   while((op->flash->sensors[sensorCount][SENSOR_ID_SIZE] != 0xFF) && (sensorCount < SENSOR_LIST_SIZE) && (match == 0))
   {
      match = 1;
      for (unsigned char j = 0; j < SENSOR_ID_SIZE; j++)
      {
         if (sensorID[j] != op->flash->sensors[sensorCount][j])
         {
            match = 0;
         }
      }
      if (match)
      {
         break;
      }
      else
      {
         ++sensorCount;
      }
   }
   if (match)
   {
      return (char)sensorCount;
   }
   return -1;
}

unsigned char opSensorGetCount (void * pOp)
{
   unsigned char sensorCount = 0;

   OPERATION_MACHINE * op = (OPERATION_MACHINE *)pOp;

   while((op->flash->sensors[sensorCount][SENSOR_ID_SIZE] != 0xFF) && (sensorCount < SENSOR_LIST_SIZE))
   {
      ++sensorCount;
   }
   return (char)sensorCount;
}


// Timer1 A0 interrupt service routine
#pragma vector=TIMER1_A0_VECTOR
__interrupt void TIMER1_A0_ISR(void)
{
   operationMachine.incTimer(&operationMachine);
}

const unsigned char nibbleToASCIITable[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

void valueToASCII (unsigned char * input, unsigned char * output)
{
   for(unsigned char i = 0; i < 2; i++)
   {
      *output = nibbleToASCIITable[(*input)>>4];
      ++output;
      *output = nibbleToASCIITable[(*input)&0x0F];
      ++output;
      ++input;
   }
}