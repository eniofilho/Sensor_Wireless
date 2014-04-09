/*! \file flashParam.h
 *  \brief interface publica para o objeto flashParam.
 */

#define SENSOR_ID_SIZE 4
#define SENSOR_TYPE_SIZE 1
#define SENSOR_LIST_SIZE 20

#ifdef ACCESS_POINT
#define FLASH_PARAM_DATA_LEN ((SENSOR_ID_SIZE + SENSOR_TYPE_SIZE) * SENSOR_LIST_SIZE)
#endif

#ifdef END_DEVICE
#define FLASH_PARAM_DATA_LEN 1
#endif

typedef struct FLASH_PARAM_STRUCT
{
   void (* init)     (void);
   void (* load)     (void);
   char (* validate) (void);
   void (* reset)    (void);
   void (* update)   (void);
   
#ifdef ACCESS_POINT
   unsigned char sensors[SENSOR_LIST_SIZE][SENSOR_ID_SIZE + SENSOR_TYPE_SIZE];
#endif

#ifdef END_DEVICE
   unsigned char check;
   unsigned char channel;
#endif
   
} FLASH_PARAM;

extern FLASH_PARAM flashParam;

void copyFlashToRam(void);