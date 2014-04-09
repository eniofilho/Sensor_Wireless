/*! \file scrambler.c
 *  \brief implementacao do scrambler.
 */

#include "scrambler.h"

void scrambler (unsigned char * dataIn, unsigned char * dataOut, unsigned char len, unsigned char seed[3])
{
   unsigned char shifter[3] = {seed[0], seed[1], seed[2]};
   
   for( unsigned char i = 0; i < len; i++)
   {
      dataOut[i] = 0;
      for (unsigned char j = 0; j < 8; j++)
      {
         unsigned char bitIn = ((dataIn[i] >> (7 - j)) & 0x01);
         unsigned char bit18 = (shifter[2] >> 2) & 0x01;
         unsigned char bit23 = (shifter[2] >> 7) & 0x01;
         unsigned char bitOut = (bitIn ^ (bit18 ^ bit23));
         dataOut[i] = dataOut[i] | (bitOut << (7 - j));
         
         shifter[2] <<= 1;
         shifter[2] |= (shifter[1] >> 7);
         shifter[1] <<= 1;
         shifter[1] |= (shifter[0] >> 7);
         shifter[0] <<= 1;
         shifter[0] |= bitOut;
      }
   }
}

void descrambler (unsigned char * dataIn, unsigned char * dataOut, unsigned char len, unsigned char seed[3])
{
   unsigned char shifter[3] = {seed[0], seed[1], seed[2]};
   
   for( unsigned char i = 0; i < len; i++)
   {
      dataOut[i] = 0;
      for (unsigned char j = 0; j < 8; j++)
      {
         unsigned char bitIn = ((dataIn[i] >> (7 - j)) & 0x01);
         unsigned char bit18 = (shifter[2] >> 2) & 0x01;
         unsigned char bit23 = (shifter[2] >> 7) & 0x01;
         unsigned char bitOut = (bitIn ^ (bit18 ^ bit23));
         dataOut[i] = dataOut[i] | (bitOut << (7 - j));
         
         shifter[2] <<= 1;
         shifter[2] |= (shifter[1] >> 7);
         shifter[1] <<= 1;
         shifter[1] |= (shifter[0] >> 7);
         shifter[0] <<= 1;
         shifter[0] |= bitIn;
      }
   }
}