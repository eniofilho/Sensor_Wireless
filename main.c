#include "cc430x513x.h"
#include "watchdog.h"

#ifdef ACCESS_POINT
#include "apOperationMachine.h"
#endif
#ifdef END_DEVICE
#include "edOperationMachine.h"
#endif

void SetVCoreUp(unsigned char level);
void SetVCoreDown(unsigned char level);
void SetVCore (unsigned char level);
void initCore(void);
void wdtStop(void);
void wdtClear(void);

int main( void )
{
   //Para o watchdog
   wdtStop();
  
   // configura a CPU e os clocks do sistema
   initCore();
   operationMachine.init = opInit;
   operationMachine.init(&operationMachine);
   
   // habilita interrupcoes
   __enable_interrupt();
   
   while(1)
   {
#ifdef ACCESS_POINT
      if (operationMachine.wdtControl && (operationMachine.serial->timeoutSerial < (100 * 40)))
      {
         operationMachine.wdtControl = 0;
         wdtClear();
      }
#endif
      operationMachine.run(&operationMachine);
   }
}

// FUNCOES DO PMM //
void SetVCoreUp (unsigned char level)
{
   // libera o acesso de escrita para os registradores do PMM
   PMMCTL0_H = 0xA5;

   // seta a parte alta do SVS/M para o novo nivel
   SVSMHCTL = SVSHE + SVSHRVL0 * level + SVMHE + SVSMHRRL0 * level;

   // seta o novo nivel do SVM
   SVSMLCTL = SVSLE + SVMLE + SVSMLRRL0 * level;

   // espera o SVM estabilizar
   while ((PMMIFG & SVSMLDLYIFG) == 0);

   // configura o VCore
   PMMCTL0_L = PMMCOREV0 * level;

   // limpa as flags
   PMMIFG &= ~(SVMLVLRIFG + SVMLIFG);

   // espera o nivel configurado ser alcancado
   if ((PMMIFG & SVMLIFG))
   {
       while ((PMMIFG & SVMLVLRIFG) == 0);
   }

   // configura a parte baixa do SVS/M para o novo nivel
   SVSMLCTL = SVSLE + SVSLRVL0 * level + SVMLE + SVSMLRRL0 * level;

   // bloqueia o acesso de escrita aos registradores do PMM
   PMMCTL0_H = 0x00;
}


void SetVCoreDown (unsigned char level)
{
   // libera o acesso de escrita para os registradores do PMM
   PMMCTL0_H = 0xA5;

   // configura a parte baixa do SVS/M para o novo nivel
   SVSMLCTL = SVSLE + SVSLRVL0 * level + SVMLE + SVSMLRRL0 * level;

   // espera o SVM estabilizar
   while ((PMMIFG & SVSMLDLYIFG) == 0);

   // configura o VCore para 1.85V para frequencia maxima
   PMMCTL0_L = (level * PMMCOREV0);

   // bloqueia o acesso de escrita aos registradores do PMM
   PMMCTL0_H = 0x00;
}

void SetVCore (unsigned char level)
{
   unsigned char actLevel;

   do
   {
      actLevel = PMMCTL0_L & PMMCOREV_3;
      if (actLevel < level)
      {
         SetVCoreUp(++actLevel);               /* Set VCore (step by step) */
      }
      if (actLevel > level)
      {
         SetVCoreDown(--actLevel);             /* Set VCore (step by step) */
      }
  }while (actLevel != level);
}

void initCore(void)
{
   // configura o PMM
   SetVCore(2);

   // configura a solicitacao de high power
   PMMCTL0_H = 0xA5;
   PMMCTL0_L |= PMMHPMRE;
   PMMCTL0_H = 0x00;

   // configura o REFOCLK interno de 32768Hz como referencia do FLL
   UCSCTL3 = SELREF__REFOCLK;
   UCSCTL4 = SELA__REFOCLK | SELS__DCOCLKDIV | SELM__DCOCLKDIV;

   // configura o clock da CPU para 12MHz
   // desabilita o loop de controle do FLL
   _BIS_SR(SCG0);
   // configura os menores DCOx e MODx possiveis
   UCSCTL0 = 0x0000;
   // seleciona a faixa adequada
   UCSCTL1 = DCORSEL_5;
   // configura o multiplicador do DCO
   UCSCTL2 = FLLD_1 + 0x16E;
   // habilitao loop de controle do FLL
   _BIC_SR(SCG0);

   // espera a estabilizacao do DCO
   __delay_cycles(375000); 

   // espera a estabilizacao do XT1 e do DCO
   // usa do-while para executar o bloco pelo menos 1 vez
   do
   {
      UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + XT1HFOFFG + DCOFFG);
      SFRIFG1 &= ~OFIFG;      // Clear fault flags
   }
   while ((SFRIFG1 & OFIFG));

   // espera a estabilizacao do DCO
   __delay_cycles(375000);
   _BIS_SR(SCG0);
   UCSCTL4 = SELA__REFOCLK | SELS__DCOCLKDIV | SELM__DCOCLKDIV;
   
   // coloca todo mundo como saída e em zero
   PADIR = 0xFFFF;
   PAOUT = 0x0000;
   PBDIR = 0xFFFF;
   PBOUT = 0x0000;
   P5DIR = 0xFF;
   P5OUT = 0x00;
   PJDIR = 0xFF;
   PJOUT = 0x00;
}