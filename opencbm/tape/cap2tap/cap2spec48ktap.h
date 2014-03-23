/*
 *  CBM 1530/1531 tape routines.
 *  Copyright 2012 Arnd Menge, arnd(at)jonnz(dot)de
*/

#ifndef __CAP2SPEC48KTAP_H_
#define __CAP2SPEC48KTAP_H_

#include "arch.h"

// Convert CAP to Spectrum48K TAP format. *EXPERIMENTAL*
int32_t CAP2SPEC48KTAP(HANDLE hCAP, FILE *TapFile);

#endif
