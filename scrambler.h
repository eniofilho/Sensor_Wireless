/*! \file scrambler.h
 *  \brief interface publica para o scrambler.
 */

#define SCRAMBLER_SEED1 0x01
#define SCRAMBLER_SEED2 0x23
#define SCRAMBLER_SEED3 0x45

void scrambler (unsigned char * dataIn, unsigned char * dataOut, unsigned char len, unsigned char seed[3]);
void descrambler (unsigned char * dataIn, unsigned char * dataOut, unsigned char len, unsigned char seed[3]);