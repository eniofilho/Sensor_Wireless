/*! \file flashParam.c
 *  \brief objeto que gerencia parametros persistentes da flash.
 */

#include "flashParam.h"
#include "cc430x513x.h"

#define FLASH_INFO_A 0x1980
#define INFO_FLASH_ADDR FLASH_INFO_A

// protitipos das funcoes de apoio
void infoErase(void);
void infoWB (unsigned char * addr, char value);

// prototipos dos metodos
void flashParamInit(void);
void flashParamLoad(void);
char flashParamValidate(void);
void flashParamReset(void);
void flashParamUpdate(void);

FLASH_PARAM flashParam = {flashParamInit};

void flashParamInit(void)
{
   flashParam.load = flashParamLoad;
   flashParam.validate = flashParamValidate;
   flashParam.reset = flashParamReset;
   flashParam.update = flashParamUpdate;
   
   copyFlashToRam();           // copia as funcoes do update para a ram

   flashParam.load();

   // valida os dados
   if (!flashParam.validate())
   {
      flashParam.reset();
      flashParam.update();
   }
}

void flashParamLoad(void)
{
   unsigned char * flashPtr = (unsigned char *)INFO_FLASH_ADDR;

#ifdef ACCESS_POINT
   unsigned char * ramPtr = (unsigned char *)(flashParam.sensors);
   unsigned char i;

   // copia da flash para a ram
   for (i = 0; i < FLASH_PARAM_DATA_LEN; i++)
   {
      *ramPtr++ = *flashPtr++;
   }
#endif
   
#ifdef END_DEVICE
   flashParam.check = *flashPtr++;
   flashParam.channel = *flashPtr;
#endif   
}

char flashParamValidate(void)
{
   // Ainda tem que definir um criterio para validacao dos IDs dos sensores
   return 1;
}

void flashParamReset(void)
{
#ifdef ACCESS_POINT
   unsigned char i,j;

   for (i = 0; i < SENSOR_LIST_SIZE; i++)
   {
      for (j = 0; j < (SENSOR_ID_SIZE + SENSOR_TYPE_SIZE); j++)
      {
         flashParam.sensors[i][j] = 0xFF;
      }
   }
#endif
   
#ifdef END_DEVICE
   flashParam.check = 0xFF;
   flashParam.channel = 0xFF;
#endif 
}

void flashParamUpdate(void)
{
   unsigned char * flashPtr = (unsigned char *)INFO_FLASH_ADDR;

#ifdef ACCESS_POINT
   unsigned char * ramPtr = (unsigned char *)(flashParam.sensors);
   unsigned char i;
   
   // apaga a memoria flash de parametros
   infoErase();

   // grava os novos valores
   for (i = 0; i < FLASH_PARAM_DATA_LEN; i++)
   {
      infoWB (flashPtr, *ramPtr);
      ++flashPtr;
      ++ramPtr;
   }
#endif
   
#ifdef END_DEVICE
   
   // apaga a memoria flash de parametros
   infoErase();

   infoWB (flashPtr, flashParam.check);
   ++flashPtr;
   infoWB (flashPtr, flashParam.channel);
   
#endif
}

#pragma segment="FLASHCODE"
#pragma segment="RAMCODE"

/*!  \brief Copia as funcoes necessarias da flash para a RAM*/
void copyFlashToRam(void)
{
   unsigned char *flash_start_ptr;
   unsigned char *flash_end_ptr;
   unsigned char *RAM_start_ptr;

   // inicializa os enderecos de inicio e de fim da flash e da ram
   flash_start_ptr = (unsigned char *)__segment_begin("FLASHCODE");
   flash_end_ptr = (unsigned char *)__segment_end("FLASHCODE");
   RAM_start_ptr = (unsigned char *)__segment_begin("RAMCODE");

  //calcula o tamanho das fncoes a serem copiadas
   unsigned long function_size = (unsigned long)(flash_end_ptr) - (unsigned long)(flash_start_ptr);

   // copia as funcoes da flash para a RAM
   while (function_size)
   {
      *RAM_start_ptr++ = *flash_start_ptr++;
      --function_size;
   }
}

/*!  \brief Grava um byte na info flash.
 *   \param addr endereco onde vai gravar o dado
 *   \param value valor a ser gravado na flash
 */
void infoWB (unsigned char * addr, char value)
{
   // checa a flag BUSY
   while(FCTL3&BUSY);

   // desbloqieia a flash principal mas mantem a INFO FLASH bloqueada
   FCTL3 = FWKEY + LOCKA;

   // set o bit WRT para permitir escrita
   FCTL1 = FWKEY + WRT;

   // faz a gravacao do valor na flash
   *addr = value;

   // checa a flag BUSY
   while(FCTL3&BUSY);

   // limpa o bit WRT para bloquear novas escritas
   FCTL1 = FWKEY;

   // seta o bit LOCK e LOCKA
   FCTL3 = FWKEY + LOCK + LOCKA;
}

#pragma location="RAMCODE"
/*!  \brief apaga a info flash.
 */
void infoErase (void)
{
   unsigned int * flashPtr;

   // carrega o endereco da secao a ser apagada
   flashPtr = (unsigned int *) INFO_FLASH_ADDR;

   // desbloqieia a flash principal
   FCTL3 = FWKEY + LOCKA;

   // checa a flag BUSY
   while(FCTL3&BUSY);

   // set o bit MERAS para apagar todo uma secao
   FCTL1 = FWKEY + ERASE;

   // faz uma escrita dummy para apagar a secao
   *flashPtr = 0;

   // checa a flag BUSY
   while(FCTL3&BUSY);

   // seta o bit LOCK e LOCKA
   FCTL3 = FWKEY + LOCK + LOCKA;
}



