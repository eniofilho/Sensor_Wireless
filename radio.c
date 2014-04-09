/* \file radio.c
 * \brief Implementacao da comunicacao via radio.
 */

#include "cc430x513x.h"
#include "radio.h"

// MACROS
#define ENTER_CRITICAL_SECTION(x)         { x = __get_interrupt_state(); __disable_interrupt(); }
#define EXIT_CRITICAL_SECTION(x)          __set_interrupt_state(x)
#define MRFI_RADIO_INST_WRITE_WAIT()      while( !(RF1AIFCTL1 & RFINSTRIFG));
#define MRFI_RADIO_DATA_WRITE_WAIT()      while( !(RF1AIFCTL1 & RFDINIFG)  );
#define MRFI_RADIO_STATUS_READ_CLEAR()    RF1AIFCTL1 &= ~(RFSTATIFG);

// defines
#define BSP_TIMER_CLK_MHZ   12       // 12 MHz MCLKC and SMCLK
#define MAX_RXFIFO_SIZE     (64u)
// PATABLE ANTIGO (Verificar por que funcionava) TODO
//#define SETTING_PATABLE     0x8D
// max dBm
#define SETTING_PATABLE     0xC0
// +10dBm
//#define SETTING_PATABLE     0xC6
// 0dB
//#define SETTING_PATABLE     0x50
// -6dBm
//#define SETTING_PATABLE     0x2D

// Configuracao do radio 
const unsigned char RF1A_REG_SMARTRF_SETTING[][2] =
{ // internal radio configuration
  {  IOCFG0,    0x1B  },
  {  IOCFG1,    0x1E  },
  {  IOCFG2,    0x29  },
  {  MCSM2,     0x07  },  
  {  MCSM1,     0x3C  },
  {  MCSM0,     0x18  },
  {  SYNC1,     0xD3  },
  {  SYNC0,     0x91  },
  {  PKTLEN,    0xFE  },
  {  PKTCTRL1,  0x05  },
  {  PKTCTRL0,  0x45  },
  {  ADDR, 	0x00  },
  {  FIFOTHR,   0x07  },
  {  WOREVT1,   0x87  },  
  {  WOREVT0,   0x6B  },   
  {  WORCTRL,   0xF8  },
  {  CHANNR,    0x00  },
  {  FSCTRL1,   0x0C  },
  {  FSCTRL0,   0x00  },
  {  FREQ2,     0x10  },
  {  FREQ1,     0xB0  },
  {  FREQ0,     0x71  },
  {  MDMCFG4,   0x2D  },
  {  MDMCFG3,   0x3B  },
  {  MDMCFG2,   0x13  },
  {  MDMCFG1,   0x22  },
  {  MDMCFG0,   0xF8  },
  {  DEVIATN,   0x62  },
  {  FOCCFG,    0x1D  },
  {  BSCFG,     0x1C  },
  {  AGCCTRL2,  0xC7  },
  {  AGCCTRL1,  0x00  },
  {  AGCCTRL0,  0xB0  },
  {  FREND1,    0xB6  },
  {  FREND0,    0x10  },
  {  FSCAL3,    0xEA  },
  {  FSCAL2,    0x2A  },
  {  FSCAL1,    0x00  },
  {  FSCAL0,    0x1F  },
  {  AGCTEST,   0x88  },
  {  PTEST,     0x7F  },
  {  FSTEST,    0x59  },
  {  TEST2,     0x88  },
  {  TEST1,     0x31  },
  {  TEST0,     0x09  },
};


// Prototipos das funcoes de apoio para a interface com o radio
void BSP_Delay(unsigned short usec);
void radioWriteReg(unsigned char addr, unsigned char value);
unsigned char radioReadReg(unsigned char addr);
void radioReadRxFifo(unsigned char * pData, unsigned char len);
void radioWriteTxFifo(unsigned char * pData, unsigned char len);

// Prototipos dos metodos do objeto radio
void radioInit       (void * pradio);
void radioPowerOff   (void * pradio);
void radioReset      (void * pradio);
void radioConfig     (void * pradio);
void radioReceiveOn  (void * pradio);
void radioReceiveOff (void * pradio);
char radioTransmit   (void * pradio, unsigned char * data,  unsigned char len);
char radioGetData    (void * pradio, unsigned char * buff, unsigned char * len);

void radioIsr(void);

// Instancia do objeto radio
extern RADIO radio1 = {radioInit};

// Implementacao dos metodos
void radioInit       (void * pradio)
{
   RADIO * radio = (RADIO *)pradio;
   
   radio->powerOff = radioPowerOff;
   radio->reset = radioReset;
   radio->config = radioConfig;
   radio->receiveOn = radioReceiveOn;
   radio->receiveOff = radioReceiveOff;
   radio->transmit = radioTransmit;
   radio->getData = radioGetData;
   
   radio->isr = radioIsr;
   
   radio->state = RADIO_STATE_OFF;
   radio->rxLen = 0;
   
   radio->reset(radio);
   radio->config(radio);
   
   radio->timer = 0;
}

void radioPowerOff   (void * pradio)
{
   RF1AIFG = 0;                  // Clear any radio pending interrupt
   radioStrobe(RF_SRES);         // Reset radio core
}

void radioReset      (void * pradio)
{
   volatile unsigned short i;
   unsigned char x;

   RF1AIFG = 0;                  // Clear any radio pending interrupt
   radioStrobe(RF_SRES);         // Reset radio core
   for (i=0; i<100; i++);        // Wait before checking IDLE 
   do 
   {
      x = radioStrobe(RF_SIDLE);
   } 
   while ((x&0x70)!=0x00);  
   RF1AIFERR = 0;                // Clear radio error register
}

void radioConfig     (void * pradio)
{
   // Escreve o PA Table
   unsigned char readbackPATableValue = 0;
   unsigned short int_state;

   ENTER_CRITICAL_SECTION(int_state);
   while (readbackPATableValue != SETTING_PATABLE)
   {
      while (!(RF1AIFCTL1 & RFINSTRIFG)) ;
      RF1AINSTRW = 0x7E00 + SETTING_PATABLE; // PA Table write (burst)
      while (!(RF1AIFCTL1 & RFINSTRIFG)) ;
      RF1AINSTRB = RF_SNOP;        // reset pointer
      while (!(RF1AIFCTL1 & RFINSTRIFG)) ;
      RF1AINSTRB = 0xFE;           // PA Table read (burst)
      while (!(RF1AIFCTL1 & RFDINIFG)) ;
      RF1ADINB = 0x00;             //dummy write
      while (!(RF1AIFCTL1 & RFDOUTIFG)) ;
      readbackPATableValue = RF1ADOUT0B;
      while (!(RF1AIFCTL1 & RFINSTRIFG)) ;
      RF1AINSTRB = RF_SNOP;
   }
   EXIT_CRITICAL_SECTION(int_state);
   
   // Inicializa os registradores de configuracao do radio
   for (unsigned char i=0; i < (sizeof(RF1A_REG_SMARTRF_SETTING)/2); i++)
   {
      radioWriteReg(RF1A_REG_SMARTRF_SETTING[i][0], RF1A_REG_SMARTRF_SETTING[i][1]);
   }
}

void radioReceiveOn  (void * pradio)
{
   RADIO * radio = (RADIO *)pradio;
   
   RF1AIFG =0;                         // Clear a pending interrupt
   radio->state = RADIO_STATE_RX_MODE;
   radioStrobe( RF_SRX );             
}

void radioReceiveOff (void * pradio)
{
   unsigned char  x;
   RADIO * radio = (RADIO *)pradio;
   
   radio->state = RADIO_STATE_IDLE_MODE;    // Update Mode Flag
   
   do 
   {  // Wait for XOSC to be stable and radio in IDLE state
      x = radioStrobe(RF_SIDLE);
   } while (x&0x70);  
  
   radioStrobe( RF_SFRX);              // Limpa a FIFO de rx
   RF1AIFG =0;                         // Clear pending IFG
}

char radioTransmit   (void * pradio, unsigned char * data,  unsigned char len)
{
   unsigned char ccaRetries;
   unsigned char x;
   
   RADIO * radio = (RADIO *)pradio;
   
   radio->receiveOff(radio);
   
   //radioWriteTxFifo(&len,1);
   
   radioWriteTxFifo(data,len);
    
   ccaRetries = 4;
    
   while(1)
   {
      radioStrobe (RF_SRX);
      while(!(RF1AIN & BIT1)); //Wait for valid RSSI
      RF1AIFG &= ~BIT0;
      radioStrobe( RF_STX );       // Strobe STX    to initiate transfer
      x = 0;
      while((!(RF1AIFG & BIT0)) && (++x < 100) )
      {
         BSP_Delay(100);
      }
      if(RF1AIFG & BIT0)
      { // CCA PASSED
         //Clear the PA_PD pin interrupt flag.
         RF1AIFG &= ~BIT0;
         while(!(RF1AIN & BIT0)); // wait for transmit to complete.
         // Transmit done, break of loop
         break;      
      }
      else
      { // CCA FAILED
        // Turn off radio to save power
         do
         {
            x = radioStrobe(RF_SIDLE);
         }while(x & 0x70); // Wait for XOSC to be stable and radio in IDLE state.
         radioStrobe (RF_SFRX ); // Flush receive FIFO of residual Data
         if (ccaRetries != 0)
         {
            BSP_Delay(15); // delay for a number of us.
            ccaRetries--;
         }
         else // No CCA retries are left, abort.
         {
            return 0;
         }   
      }   
   }
   radioStrobe (RF_SFTX ); // Flush transmit FIFO, Radio is already in IDLE state due to Register configuration
   return 1;
}

char radioGetData    (void * pradio, unsigned char * buff, unsigned char * len)
{
   RADIO * radio = (RADIO *)pradio;
   if (radio->rxLen)
   {
      istate_t s;
      ENTER_CRITICAL_SECTION(s); // Lock out access to Radio IF
      
      if (radio->rxLen > 20) radio->rxLen = 20;
      for(unsigned char i = 0; i < radio->rxLen; i++)
      {
         buff[i] = radio->rxBuffer[i];
      }
      *len = radio->rxLen;
      radio->rxLen = 0;
      EXIT_CRITICAL_SECTION(s); // Allow access to Radio IF
      
      if (*len > 16)
      {
         *len = 0;
         return 0;
      }
   }
   else
   {
      *len = 0;
      return 0;
   }
   return 1;
}



void radioIsr(void)
{
   unsigned char rxBytes;
   unsigned char tL;
   unsigned char *tmpRxBuffer;
   unsigned short coreIntSource = RF1AIV;
   unsigned char tempRxBufferLength, RxBufferLength;

   RF1AIFG &= ~BIT9; // Clear RX Interrupt Flag
	
   if(radio1.state != RADIO_STATE_RX_MODE)
   { // We should only be here in RX mode, not in TX mode, nor if RX mode was turned on during CCA
      return;
   }
   
   for(int i=0; i < sizeof(radio1.rxBuffer); i++) radio1.rxBuffer[i] = 0; // Clean Buffer to help protect against spurious frames

   tmpRxBuffer = radio1.rxBuffer; // Use a pointer to move through the Buffer

   rxBytes = radioReadReg( RXBYTES ); // Read the number of bytes ready on the FIFO
	
   do
   {
      tL = rxBytes;
      rxBytes = radioReadReg( RXBYTES );
   }
   while(tL != rxBytes);   // Due to a chip bug, the RXBYTES has to read the same value twice for it to be correct
	
   if(rxBytes == 0)   // Check if the RX FIFO is empty, this may happen if address check is enabled and the FIFO is flushed when address doesn't match
   {
      return;
   }
	
   radioReadRxFifo(tmpRxBuffer, 1);
   RxBufferLength = *(tmpRxBuffer) + 2; // Add 2 for the status bytes which are appended by the Radio    
   ++tmpRxBuffer;
   
   tempRxBufferLength = RxBufferLength;
    
   // Check if number of bytes in Fifo exceed the FIFO Size, if so, assume FIFO overflow due to something
   // gone wrong with radio, and the only way to fix it, is to force IDLE mode and then back to RX Mode  
   if(rxBytes > MAX_RXFIFO_SIZE)
   {
      radio1.receiveOff(&radio1);
      radio1.receiveOn(&radio1);
      return;
   }
   // else: everything matches continue
      
   //Copy Rest of packet
   while(RxBufferLength > 1)
   {	
      rxBytes = radioReadReg( RXBYTES );
      do
      {
         tL = rxBytes;
         rxBytes= radioReadReg( RXBYTES );
      }
      while(tL != rxBytes);   // Due to a chip bug, the RXBYTES has to read the same value twice for it to be correct
      if((rxBytes > MAX_RXFIFO_SIZE))
      {
         radio1.receiveOff(&radio1);
         radio1.receiveOn(&radio1);
         return;
      }
      if (rxBytes)
      {
         radioReadRxFifo(tmpRxBuffer, rxBytes);
         tmpRxBuffer +=rxBytes;
         RxBufferLength-=rxBytes;
      }
   }
   radioReadRxFifo(tmpRxBuffer, 1);
   // Signal main program that packet has been received and ready in RxBuffer
   radio1.rxLen = tempRxBufferLength;
   //radioStrobe( RF_SFRX);              // Limpa a FIFO de rx
}


// FUNCOES DE APOIO DO WBSL

void BSP_Delay(unsigned short usec)
{
  TA0R = 0; /* initial count  */
  TA0CCR0 = BSP_TIMER_CLK_MHZ*usec; /* compare count. (delay in ticks) */

  /* Start the timer in UP mode */
  TA0CTL |= MC_1 | TASSEL_2;

  /* Loop till compare interrupt flag is set */
  while(!(TA0CCTL0 & CCIFG));

  /* Stop the timer */
  TA0CTL &= ~(MC_1);

  /* Clear the interrupt flag */
   TA0CCTL0 &= ~CCIFG;
}

unsigned char radioStrobe(unsigned char addr)
{
   unsigned char statusByte, gdoState;
   istate_t s;

   ENTER_CRITICAL_SECTION(s); // Lock out access to Radio IF

   MRFI_RADIO_STATUS_READ_CLEAR(); // Lock out access to Radio IF

   MRFI_RADIO_INST_WRITE_WAIT(); // Wait for radio to be ready for next instruction

   if ((addr > RF_SRES) && (addr < RF_SNOP))
   {
      gdoState = radioReadReg(IOCFG2); // buffer IOCFG2 state
      radioWriteReg(IOCFG2, 0x29); // c-ready to GDO2

      RF1AINSTRB = addr;

      if ((RF1AIN & 0x04) == 0x04) // chip at sleep mode
      {
         if ( (addr == RF_SXOFF) || (addr == RF_SPWD) || (addr == RF_SWOR) )
         {
            // Do nothing
         }
         else
         {
            while ((RF1AIN & 0x04) == 0x04); // c-ready
            BSP_Delay(760); // Delay should be 760us
         }
      }
      radioWriteReg(IOCFG2, gdoState); // restore IOCFG2 setting
   }
   else
   {
      RF1AINSTRB = addr; // chip active mode
   }
   statusByte = RF1ASTAT0B; // Read status byte
   EXIT_CRITICAL_SECTION(s); // Allow access to Radio IF
   return statusByte;
}

void radioWriteReg(unsigned char addr, unsigned char value)
{
   istate_t s;
   ENTER_CRITICAL_SECTION(s); // Lock out access to Radio IF
   MRFI_RADIO_INST_WRITE_WAIT(); // Wait for radio to be ready for next instruction
   RF1AINSTRB = (0x00 | addr); // Write cmd: 'write to register'
   MRFI_RADIO_DATA_WRITE_WAIT(); // Wait for radio to be ready to accept the data
   RF1ADINB   = value; // value to be written to the radio register
   EXIT_CRITICAL_SECTION(s); // Allow access to Radio IF
}

unsigned char radioReadReg(unsigned char addr)
{
   istate_t s;
   unsigned char regValue;

   ENTER_CRITICAL_SECTION(s); // Lock out access to Radio IF

   MRFI_RADIO_INST_WRITE_WAIT(); // Wait for radio to be ready for next instruction

   if( (addr <= 0x2E) || (addr == 0x3E))
   {
      RF1AINSTR1B = (0x80 | addr); // Write cmd: read the Configuration register
   }
   else
   {
      RF1AINSTR1B = (0xC0 | addr); // Write cmd: read the Status register
   }
   // Read out the register value
   regValue   = RF1ADOUT1B; //auto read
   EXIT_CRITICAL_SECTION(s); // Allow access to Radio IF

   return( regValue);
}


void radioReadRxFifo(unsigned char * pData, unsigned char len)
{
   istate_t s;

   ENTER_CRITICAL_SECTION(s); // Lock out access to Radio IF

   do
   {
      MRFI_RADIO_INST_WRITE_WAIT(); // Wait for radio to be ready for next instruction
      RF1AINSTR1B = 0xBF; // Write cmd: SNGLRXRD
      *pData  = RF1ADOUT1B; //auto read register
      pData++;
      len--;
   }while(len);
   EXIT_CRITICAL_SECTION(s); // Allow access to Radio IF
}

void radioWriteTxFifo(unsigned char * pData, unsigned char len)
{
   istate_t s;

   ENTER_CRITICAL_SECTION(s); // Lock out access to Radio IF
   MRFI_RADIO_INST_WRITE_WAIT(); // Wait for radio to be ready for next instruction
   RF1AINSTRB = 0x7F; // Write cmd: TXFIFOWR

   do
   {
      MRFI_RADIO_DATA_WRITE_WAIT(); // Wait for radio to be ready to accept the data
      RF1ADINB   = *pData; //Write one byte to FIFO
      pData++;
      len--;
   }while(len);

   EXIT_CRITICAL_SECTION(s); // Allow access to Radio IF
}
