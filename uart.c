/* \file uart.c
 * \brief Implementacao para porta serial.
 */

#include "uart.h"
#include "cc430x513x.h"

// define do sistema
#define SYS_CLK       12000000UL

// prototipos dos metodos
void uartInit      (void * puart);
void uartConfig    (void * puart, UART_SPEED speed, UART_BITS bits, UART_PARITY parity, UART_STOP_BITS stopBits);
void uartStart     (void * puart);
void uartPutBuffTx (void * puart, unsigned char data);
char uartGetBuffTx (void * puart, unsigned char * data);
void uartPutBuffRx (void * puart, unsigned char data);
char uartGetBuffRx (void * puart, unsigned char * data);
void uartStop      (void * puart);
void uartReset     (void * puart);

// Instancias de porta serial
UART uart1 = {uartInit};

/* \brief Inicializacao dos metodos  e variaveis do objeto. */
void uartInit      (void * puart)
{
   UART * uart = (UART *)puart;
   
   uart->config    = uartConfig;
   uart->start     = uartStart;
   uart->putBuffTx = uartPutBuffTx;
   uart->getBuffTx = uartGetBuffTx;
   uart->putBuffRx = uartPutBuffRx;
   uart->getBuffRx = uartGetBuffRx;
   uart->stop      = uartStop;
   uart->reset     = uartReset;
   
   uart->state = UART_STATE_NOT_INITIALIZED;
   uart->reset(uart);
   
   // configura os pinos como serial
   UART_PDIR |= UART_TX;
   UART_PDIR &= ~UART_RX;
   UART_PSEL |= (UART_TX + UART_RX);
}

/* \brief Configura os parametros da porta serial. */
void uartConfig    (void * puart, UART_SPEED speed, UART_BITS bits, UART_PARITY parity, UART_STOP_BITS stopBits)
{
   unsigned char tempReg = 0;
   
   UART * uart = (UART *)puart;
   
   uart->speed = speed;
   uart->bits = bits;
   uart->parity = parity;
   uart->stopBits = stopBits;

   // coloca o periferico em reset
   UCA0CTL1 |= UCSWRST;
   
   // SMCLK
   UCA0CTL1 = UCSSEL_3 + UCSWRST;
   
   // Configura a taxa de comunicacao
   switch(uart->speed)
   {
      case UART_SPEED_2400:
         UCA0BRW = (SYS_CLK / 2400);
         break;
      case UART_SPEED_4800:
         UCA0BRW = (SYS_CLK / 4800);
         break;
      case UART_SPEED_9600:
         UCA0BRW = (SYS_CLK / 9600);
         break;
      case UART_SPEED_19200:
         UCA0BRW = (SYS_CLK / 19200);
         break;
      case UART_SPEED_38400:
         UCA0BRW = (SYS_CLK / 38400);
         break;
      case UART_SPEED_57600:
         UCA0BRW = (SYS_CLK / 57600);
         break;
      case UART_SPEED_115200:
         UCA0BRW = (SYS_CLK / 115200);
         break;
   }
   
   if (uart->bits == UART_BITS_7) tempReg |= UC7BIT;
   if (uart->stopBits == UART_STOP_BITS_2) tempReg |= UCSPB;
   if (uart->parity != UART_PARITY_NONE) tempReg |= UCPEN;
   if (uart->parity == UART_PARITY_EVEN) tempReg |= UCPAR;
   UCA0CTL0 = tempReg;

   uart->state = UART_STATE_OFF;
}

/* \brief Habilita a recepcao e transmissao da porta serial. */
void uartStart     (void * puart)
{
   UART * uart = (UART *)puart;
   
   // tira o periferico do estado de reset
   UCA0CTL1 &= ~(UCSWRST);
   
   // Configura as interrupcoes
   UCA0IE = UCTXIE + UCRXIE;
   
   uart->state = UART_STATE_ON;
}

/* \brief Coloca um caracter no buffer de transmissao da porta serial. */
void uartPutBuffTx (void * puart, unsigned char data)
{
   UART * uart = (UART *)puart;
   
   uart->txBuffer[uart->txPtrIn] = data;
   uart->txPtrIn++;
   uart->txPtrIn &= (UART_TX_BUFFER_SIZE-1);
   
   // tem que verificar se o buffer de transmissao esta livre
   if(!(UCA0IE & UCTXIE))
   {
      UCA0TXBUF = data;
      uart->txPtrOut++;
      uart->txPtrOut &= (UART_TX_BUFFER_SIZE-1);
      
      // Habilita a interrupcao do transmissor
      UCA0IE |= UCTXIE;
   }
}

/* \brief Pega um caracter do buffer de transmissao da porta serial. */
char uartGetBuffTx (void * puart, unsigned char * data)
{
   UART * uart = (UART *)puart;
   
   if(uart->txPtrIn != uart->txPtrOut)
   {
      *data = uart->txBuffer[uart->txPtrOut++];
      uart->txPtrOut &= (UART_TX_BUFFER_SIZE-1);
      return(1);
   }
   else
   {
      return(0);
   }
}

/* \brief Coloca um caracter no buffer de recepcao da porta serial. */
void uartPutBuffRx (void * puart, unsigned char data)
{
   UART * uart = (UART *)puart;
   
   uart->rxBuffer[uart->rxPtrIn] = data;
   uart->rxPtrIn++;
   uart->rxPtrIn &= (UART_RX_BUFFER_SIZE-1);
}

/* \brief Pega um caracter do buffer de recepcao da porta serial. */
char uartGetBuffRx (void * puart, unsigned char * data)
{
   UART * uart = (UART *)puart;
   
   if(uart->rxPtrIn != uart->rxPtrOut)
   {
      *data = uart->rxBuffer[uart->rxPtrOut++];
      uart->rxPtrOut &= (UART_RX_BUFFER_SIZE-1);
          return(1);
   }
   else
   {
      return(0);
   }
}

/* \brief Para a porta serial. */
void uartStop      (void * puart)
{
   UART * uart = (UART *)puart;
   
   // coloca o periferico em reset
   UCA0CTL1 |= UCSWRST;
   
   uart->state = UART_STATE_OFF;
}

/* \brief Reseta os buffers da serial. */
void uartReset     (void * puart)
{
   UART * uart = (UART *)puart;
   
   uart->txBuffer[0] = 0;
   uart->txPtrIn = 0;
   uart->txPtrOut = 0;
   uart->rxBuffer[0] = 0;
   uart->rxPtrIn = 0;
   uart->rxPtrOut = 0;
}


// INTERRUPT SERVICE ROUTINES
#pragma vector=USCI_A0_VECTOR
__interrupt void radioInterrupt(void)
{
   unsigned char interruptSource = UCA0IV;
   if (interruptSource & 0x02)
   {
      uart1.putBuffRx(&uart1, UCA0RXBUF);
   }
   if (interruptSource & 0x04)
   {
      unsigned char data;
      if(uart1.getBuffTx(&uart1, &data))
      {
         UCA0TXBUF = data;
      }
      else
      {
         UCA0IE &= ~UCTXIE;
      }
           
   }
}
