/*! \file edOperationMachine.c
 *  \brief implementacao da maquina de operacao do modo end device.
 */

#include "edOperationMachine.h"
#include "cc430x513x.h"
#include "scrambler.h"
#include "watchdog.h"

unsigned char testPkg[] = {0x06, 0x00, 'T','E','S','T','E'};
unsigned char discoveryPkg[] = {  14 ,                  // tamanho do pacote
                                 0x00,                  // endereco do AP
                                 0x00, 0x00, 0x00,      // semente do scrambler
                                 'D','I','S','C',       // payload
                                 0x30, 0x30, 0x30, 0x30,// ID do sensor
                                 0x30,                  // tipo do sensor
                                 0x46                   // checksum
                               };
unsigned char statusPkg[] =    { 12 ,                   // tamanho do pacote
                                0x00,                   // endereco do AP
                                0x00, 0x00, 0x00,       // semente do scrambler
                                0x30, 0x30, 0x30, 0x30, // ID do sensor
                                0x30,                   // tipo do sensor
                                'F', 'F',               // valor do sensor
                                0x46                    // checksum
                               };
//unsigned char tempBuff[15];

#define BT_CFG_PORT 0
#define BT_CFG_BIT  1

#define BT_SEN_PORT 0
#define BT_SEN_BIT  14


// defines
#define SYS_CLK       12000000UL
#define PRE_SCALER    8
#define OP_FREQ       100
#define FREQ_COUNTER  ((SYS_CLK / PRE_SCALER) / OP_FREQ)
#define SYS_CLK_LP    32768UL
#define PRESCALER_WAIT 8
#define TIMEOUT_01S   ((SYS_CLK_LP / PRE_SCALER) / PRESCALER_WAIT)
#define TIMEOUT_02S   (TIMEOUT_01S * 2)
#define TIMEOUT_04S   (TIMEOUT_01S * 4)
#define TIMEOUT_08S   (TIMEOUT_01S * 8)
#define TIMEOUT_16S   (TIMEOUT_01S * 16)

#define OPERATION_MACHINE_MAX_TIMEOUT 5
#define OPERATION_MACHINE_MAX_CHANNELS 8

// prototipos dos metodos do objeto
void opInit       (void * pOp);
void opRun        (void * pOp);
void opSetState   (void * pOp, OPERATION_MACHINE_STATE state);
void opSetTimeout (void * pOp, unsigned short timeout);
void opIncTimer   (void * pOp);

__no_init OPERATION_MACHINE operationMachine;// = {opInit};

// implementacao dos metodos
void opInit       (void * pOp)
{
   OPERATION_MACHINE * op = (OPERATION_MACHINE *)pOp;
   
   op->run = opRun;
   op->setState = opSetState;
   op->setTimeout = opSetTimeout;
   op->incTimer = opIncTimer;
   op->radio = &radio1;
   op->led = &led1;
   op->btConfig = &btConfig;
   op->btSense = &btSense;
   op->flash = &flashParam;
   
   op->channel = 0;
   op->timeoutStatus = 0;
   
   // inicializa o radio
   op->radio->init(op->radio);
   
   // inicializa os pinos dos botoes
   op->btConfig->init(op->btConfig, BT_CFG_PORT, BT_CFG_BIT);
   op->btConfig->init(op->btSense, BT_SEN_PORT, BT_SEN_BIT);
   
   // inicializa os pinos dos LEDs
   op->led->init(op->led, 0, 4);
   
   //Inicia os dados da flash
   op->flash->init();
   if(op->flash->check != 0x55)
   {
       // manda pro estado inicial da maquina
       op->setState(op, OPERATION_MACHINE_STATE_DEEP_SLEEP);
   }
   else
   {            
      // vai para o estado de configuracao
      op->setState(op, OPERATION_MACHINE_STATE_TURN_ON_RADIO);
      op->channel = op->flash->channel;
      
      op->setTimeout(op, 1000);
      op->btConfig->clear(op->btConfig);
   }
   
   // inicializa o timer para a temporizacao
   // configura o timer
   TA1CCTL0 = CCIE;                          // CCR0 interrupt enabled
   TA1CCR0 = FREQ_COUNTER;
   TA1CTL = TASSEL_2 + MC_1 + TACLR + ID_3;  // SMCLK, upmode, pre-scaler /8, clear TAR
   
   // inicializa o comparador para medicao da bateria
   
}

void opRun        (void * pOp)
{
   BUTTON_PRESS_TYPE tempBtType;
   OPERATION_MACHINE * op = (OPERATION_MACHINE *)pOp;
   
   switch(op->state)
   {
      case OPERATION_MACHINE_STATE_DEEP_SLEEP:
         // configura a interrupcao do pino para acordar o processador
         op->btConfig->intEnable(op->btConfig);
         
         // desliga o radio
         op->radio->receiveOff(op->radio);
         
         // desabilita os outros perifericos (interrupcoes)
         op->led->off(op->led);
         
         // entra em baixo consumo
         op->radio->powerOff(op->radio);
         TA1CTL = TASSEL_2 + MC_0 + TACLR + ID_3;  // Para o timer
         __bis_SR_register(LPM4_bits);             // Enter LPM4
         __no_operation();                         // For debugger
         
         // desliga a interrupcao do pino
         op->btConfig->intDisable(op->btConfig);
         
         // reinicializa os outros perifericos
         TA1CTL = TASSEL_2 + MC_1 + TACLR + ID_3;  // SMCLK, upmode, pre-scaler /8, clear TAR
         
         // vai para o estado de configuracao
         op->setState(op, OPERATION_MACHINE_STATE_CONFIG);
         op->setTimeout(op, 1000);
         op->led->on(op->led);
         op->btConfig->clear(op->btConfig);
         op->configState = 0;
         
         if(op->flash->check == 0x55)
         {
           //Vai para o canal correto quando já estiver pareado
           op->channel = op->flash->channel;
           op->setState(op, OPERATION_MACHINE_STATE_TURN_ON_RADIO);
         }
         
         break;
      case OPERATION_MACHINE_STATE_CONFIG:
         op->setState(op, OPERATION_MACHINE_STATE_TURN_ON_RADIO);
         op->channel = 0;
/*
         if (op->btConfig->get(op->btConfig, &tempBtType))
         {
            op->setTimeout(op, 1000);
            if (tempBtType == BUTTON_PRESS_LONG)
            {
               op->setState(op, OPERATION_MACHINE_STATE_TURN_ON_RADIO);
               op->channel = 0;
            }
            else
            {
               if (op->configState == 0)
               {
                  op->configState = 1;
               }
               else
               {
                  ++op->timeoutStatus;
                  if (op->timeoutStatus >= OPERATION_MACHINE_MAX_TIMEOUT)
                  {
                     op->timeoutStatus = 0;
                  }
               }
               op->led->blink(op->led, op->timeoutStatus + 1, 10, 20, 100, 1);
               //op->setState(op, OPERATION_MACHINE_STATE_SEARCH_AP_QUERY);
            }
         }
         else if (op->timer >= op->timeout)
         {
            op->led->blink(op->led, 4, 10, 20, 100, 1);
            op->setState(op, OPERATION_MACHINE_STATE_INFORM_STATUS);
         }
*/
         break;
      case OPERATION_MACHINE_STATE_TURN_ON_RADIO:
         op->radio->init(op->radio);
         op->setState(op, OPERATION_MACHINE_STATE_SEARCH_AP_QUERY);
         break;
      case OPERATION_MACHINE_STATE_TURN_OFF_RADIO:
         op->radio->receiveOff(op->radio);
         op->setState(op, OPERATION_MACHINE_STATE_DEEP_SLEEP);
         break;
      case OPERATION_MACHINE_STATE_SEARCH_AP_QUERY:
         op->led->on(op->led);
         
         //embaralha a mensagem antes de enviar
         op->tempBuff[0] = sizeof(discoveryPkg) - 1; // tamanho do payload
         op->tempBuff[1] = 0x00;                     // endereco do AP
         op->tempBuff[2] = SCRAMBLER_SEED1;          // semente do scrambler
         op->tempBuff[3] = SCRAMBLER_SEED2;          // semente do scrambler
         op->tempBuff[4] = SCRAMBLER_SEED3;          // semente do scrambler
         
         scrambler (&(discoveryPkg[5]), &(op->tempBuff[5]), sizeof(discoveryPkg) - 5, &(op->tempBuff[2]));
         
         op->radio->transmit(op->radio, op->tempBuff, sizeof(discoveryPkg));
         op->radio->receiveOn(op->radio);
         
         op->setState(op, OPERATION_MACHINE_STATE_SEARCH_AP_WAIT);
         op->setTimeout(op, 50);
         break;
      case OPERATION_MACHINE_STATE_SEARCH_AP_WAIT:
         op->radio->isr();
         if (op->radio->getData(op->radio, op->tempBuff, &(op->tempLen)))
         {
            if ((op->tempBuff[0] - 6) > 14) op->tempBuff[0] = 20; // so processa mensagens de ate 20 caracteres
            descrambler (&(op->tempBuff[5]), op->message, op->tempBuff[0] - 6, &(op->tempBuff[2]));
            if ( (op->message[0] == 'D') &&
                 (op->message[1] == 'A') &&
                 (op->message[2] == 'C') &&
                 (op->message[3] == 'K')   )
            {
               op->led->off(op->led);
               op->setState(op, OPERATION_MACHINE_STATE_SEND_STATUS);
               op->flash->channel = op->channel;
               op->flash->check = 0x55;
               op->flash->update();
               
            }
            
            if ( (op->message[0] == 'S') &&
                 (op->message[1] == 'A') &&
                 (op->message[2] == 'C') &&
                 (op->message[3] == 'K')   )
            {
               op->led->off(op->led);
               op->setState(op, OPERATION_MACHINE_STATE_SEND_STATUS);
               //op->flash->update();
               
            }
         }
         else if (op->timer >= op->timeout)
         {
            op->led->off(op->led);
            // muda canal do radio
            if (++op->channel >= OPERATION_MACHINE_MAX_CHANNELS) // se ja procurou em todos os canais, vai dormir
            {
               op->led->blink(op->led, 5, 10, 20, 100, 1);
               op->setState(op, OPERATION_MACHINE_STATE_INFORM_STATUS);
            }
            else
            {
               op->setState(op, OPERATION_MACHINE_STATE_CHANGE_CHANNEL);
               op->setTimeout(op, 10);
            }
         }
         break;
      case OPERATION_MACHINE_STATE_CHANGE_CHANNEL:
         if (op->timer >= op->timeout)
         {
            op->setState(op, OPERATION_MACHINE_STATE_TURN_ON_RADIO);
         }
         
         break;
      case OPERATION_MACHINE_STATE_SEND_STATUS:
         //embaralha a mensagem antes de enviar
         op->tempBuff[0] = sizeof(statusPkg) - 1;    // tamanho do payload
         op->tempBuff[1] = 0x00;                     // endereco do AP
         op->tempBuff[2] = SCRAMBLER_SEED1;          // semente do scrambler
         op->tempBuff[3] = SCRAMBLER_SEED2;          // semente do scrambler
         op->tempBuff[4] = SCRAMBLER_SEED3;          // semente do scrambler
         
         if (!(op->btSense->getPin(op->btSense)))
         {
            statusPkg[10] = '0';
            statusPkg[11] = '0';
         }
         
         scrambler (&(statusPkg[5]), &(op->tempBuff[5]), sizeof(statusPkg) - 5, &(op->tempBuff[2]));
         
         op->radio->transmit(op->radio, op->tempBuff, sizeof(statusPkg));
         
         op->setState(op, OPERATION_MACHINE_STATE_MEASURE_BATT);
         
         op->led->on(op->led);
         break;
      case OPERATION_MACHINE_STATE_MEASURE_BATT:
         op->setState(op, OPERATION_MACHINE_STATE_WAIT_ACK);
         op->radio->receiveOn(op->radio);
         op->setTimeout(op, 10);
         break;
      case OPERATION_MACHINE_STATE_WAIT_ACK:
         op->radio->isr();
         if (op->radio->getData(op->radio, op->tempBuff, &(op->tempLen)))
         {
            if ((op->tempBuff[0] - 6) > 14) op->tempBuff[0] = 20; // so processa mensagens de ate 20 caracteres
            descrambler (&(op->tempBuff[5]), op->message, op->tempBuff[0] - 6, &(op->tempBuff[2]));
            if ( (op->message[0] == 'S') &&
                 (op->message[1] == 'A') &&
                 (op->message[2] == 'C') &&
                 (op->message[3] == 'K')   )
            { // se deu o ack no pacote, pode dormir por mais tempo.
               statusPkg[10] = 'F';
               statusPkg[11] = 'F';
               op->timeoutStatus = 4;
               op->setState(op, OPERATION_MACHINE_STATE_SLEEP);
            }
            
         }
         else if (op->timer >= op->timeout)
         {
            if (statusPkg[10] == '0')
            { // se tem pala, fica tentando transmitir mais rapido
               op->timeoutStatus = 2;
            }
            else
            { // se esta ok, pode dormir mais tempo.
               op->timeoutStatus = 4;
            }
            op->setState(op, OPERATION_MACHINE_STATE_SLEEP);
         }
         break;
      case OPERATION_MACHINE_STATE_SLEEP:
         // desliga os perifericos
         op->radio->receiveOff(op->radio);
         op->radio->powerOff(op->radio);
         op->led->off(op->led);
         
         //configura o timer para acordar o processador no timeout selecionado
         TA1CCR0 = FREQ_COUNTER;
         TA1CTL = TASSEL_1 + MC_1 + TACLR + ID_3;  // SACLK, upmode, pre-scaler /8, clear TAR
         TA1EX0 = TAIDEX_7;
         switch(op->timeoutStatus)
         {
            case 0:
               TA1CCR0 = TIMEOUT_01S;
               break;
            case 1:
               TA1CCR0 = TIMEOUT_02S;
               break;
            case 2:
               TA1CCR0 = TIMEOUT_04S;
               break;
            case 3:
               TA1CCR0 = TIMEOUT_08S;
               break;
            case 4:
               TA1CCR0 = TIMEOUT_16S;
               break;
            default:
               
               break;
         }
         TA1CTL = TASSEL_1 + MC_1 + TACLR + ID_3;  // SMCLK, upmode, pre-scaler /8, clear TAR
         
         // configura a interrupcao do pino para acordar o processador
         op->btSense->intEnable(op->btSense);
         
         //Para o watchdog para economia de energia
         wdtStop();
         
         // vai dormir
         __bis_SR_register(LPM3_bits);             // Enter LPM3
         __no_operation();                         // For debugger
         
         // desliga a interrupcao do pino
         op->btSense->intDisable(op->btSense);
         
         // reajusta o clock
         TA1EX0 = 0;
         TA1CCR0 = FREQ_COUNTER;
         TA1CTL = TASSEL_2 + MC_1 + TACLR + ID_3;  // SMCLK, upmode, pre-scaler /8, clear TAR
         
         //Configura novamente o watchdof
         wdtClear();
         
         // vai para o estado de informar o status
         op->setState(op, OPERATION_MACHINE_STATE_SEND_STATUS);
         op->radio->init(op->radio);
         op->led->on(op->led);
         break;
      case OPERATION_MACHINE_STATE_INFORM_STATUS:
         if (op->led->getState(op->led) == LED_STATE_OFF)
         {
            op->setState(op, OPERATION_MACHINE_STATE_DEEP_SLEEP);
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
   op->led->run(op->led);
   op->btConfig->run(op->btConfig);
   op->btConfig->run(op->btConfig);
}

// Timer1 A0 interrupt service routine
#pragma vector=TIMER1_A0_VECTOR
__interrupt void TIMER1_A0_ISR(void)
{
   if (operationMachine.state == OPERATION_MACHINE_STATE_SLEEP )
   {
      LPM3_EXIT;
   }
   else
   {
      operationMachine.incTimer(&operationMachine);
   }
}