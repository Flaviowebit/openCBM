/*
 *  CBM 1530/1531 tape routines.
 *  Copyright 2012 Arnd Menge, arnd(at)jonnz(dot)de
*/

#ifndef __CBMTAP2CAP_H_
#define __CBMTAP2CAP_H_

#include "arch.h"

// Convert CBM TAP to CAP format.
int32_t CBMTAP2CAP(HANDLE hCAP, HANDLE hTAP);

#endif
