#include "cc430x513x.h"
#include "watchdog.h"

void wdtStop(void)
{
    //Para o timer
    WDTCTL = WDTPW + WDTHOLD;
}

void wdtClear(void)
{
    //Limpa o wdt e coloca com o maior tempo possível
    //Corrigir para analisar o melhor tempo e colocar o teste na interrupção de timer
    WDTCTL = WDTPW + WDTSSEL_0 + WDTCNTCL + WDTIS_3;
}

void wdtPucReset(void)
{
   WDTCTL = 0;
}