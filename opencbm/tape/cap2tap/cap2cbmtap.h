/*
 *  CBM 1530/1531 tape routines.
 *  Copyright 2012 Arnd Menge, arnd(at)jonnz(dot)de
*/

#ifndef __CAP2CBMTAP_H_
#define __CAP2CBMTAP_H_

#include "arch.h"

// Convert CAP to CBM TAP format.
int32_t CAP2CBMTAP(HANDLE hCAP, HANDLE hTAP);

#endif
