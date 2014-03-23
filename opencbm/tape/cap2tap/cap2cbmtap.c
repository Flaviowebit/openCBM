/*
 *  CBM 1530/1531 tape routines.
 *  Copyright 2012 Arnd Menge, arnd(at)jonnz(dot)de
*/

#include <stdio.h>
#include <stdlib.h>

#include "cap.h"
#include "tap-cbm.h"
#include "misc.h"
#include "arch.h"

#define FREQ_C64_PAL    985248
#define FREQ_C64_NTSC  1022727
#define FREQ_VIC_PAL   1108405
#define FREQ_VIC_NTSC  1022727
#define FREQ_C16_PAL    886724
#define FREQ_C16_NTSC   894886

#define NeedEvenSplitNumber  0
#define NeedOddSplitNumber   1
#define NeedSplit            2
#define NoSplit              3


int32_t HandlePause(HANDLE hTAP, uint64_t ui64Len, uint8_t uiNeededSplit, uint8_t TAPv, uint32_t *puiCounter)
{
	uint32_t numsplits, i;

	if (TAPv == TAPv2)
	{
		if (ui64Len > 0x00ffffff)
		{
			numsplits = (uint32_t) (ui64Len/0x00ffffff);
			if ((ui64Len % 0x00ffffff) != 0) numsplits++;

			if (((uint8_t)(numsplits % 2)) != uiNeededSplit) // NeedEvenSplitNumber=0 / NeedOddSplitNumber=1
			{
				// Split last 2 parts into 3, make sure last halfwave is not too short.
				// Write (n-2) long ones, last two /3: 0.33 < length < 0.67.
				//             even     odd      even
				// |        |        |        |       *|
				// |        |        |        |*       |
				// |        |        |       *|        |
				// |        |        |*       |        |
				// |        |       *|        |        |
				// |        |*       |        |        |

				// First two 2/3-fractions, last 1/3 remaining.
				for (i=1; i<=2; i++)
				{
					// Write 32bit unsigned integer to image file: LSB first, MSB last.
					Check_TAP_CBM_Error_TextRetM1(TAP_CBM_WriteSignal_4Bytes(hTAP, 0x55555500, puiCounter));
					ui64Len -= 0x00555555;
				}
			}
			else if ((ui64Len % 0x00ffffff) < 0x007fffff)
			{
				// Make sure last halfwave is not too short: Pull 0x007fffff.
				// Does not change numsplits.
				// Write 32bit unsigned integer to image file: LSB first, MSB last.
				Check_TAP_CBM_Error_TextRetM1(TAP_CBM_WriteSignal_4Bytes(hTAP, 0x7fffff00, puiCounter));
				ui64Len -= 0x007fffff;
			}
		}
	}

	if ((TAPv == TAPv1) || (TAPv == TAPv2))
	{
		while (ui64Len > 0x00ffffff)
		{
			// Write 32bit unsigned integer to image file: LSB first, MSB last.
			Check_TAP_CBM_Error_TextRetM1(TAP_CBM_WriteSignal_4Bytes(hTAP, 0xffffff00, puiCounter));
			ui64Len -= 0x00ffffff;
		}
		if (ui64Len > 0)
		{
			// Write 32bit unsigned integer to image file: LSB first, MSB last.
			Check_TAP_CBM_Error_TextRetM1(TAP_CBM_WriteSignal_4Bytes(hTAP, (uint32_t) ((ui64Len << 8) & 0xffffff00), puiCounter));
		}
	}

	if (TAPv == TAPv0)
	{
		while (ui64Len > 2040)
		{
			Check_TAP_CBM_Error_TextRetM1(TAP_CBM_WriteSignal_1Byte(hTAP, 0, puiCounter));
			ui64Len -= 2040;
		}
		if (ui64Len > 0)
		{
			Check_TAP_CBM_Error_TextRetM1(TAP_CBM_WriteSignal_1Byte(hTAP, 0, puiCounter));
			ui64Len = 0;
		}
	}

	return 0;
}


int32_t Initialize_TAP_header_and_return_frequencies(HANDLE hCAP, HANDLE hTAP, uint32_t *puiTimer_Precision_MHz, uint32_t *puiFreq)
{
	uint8_t CAP_Machine, CAP_Video, TAP_Machine, TAP_Video, TAP_Version;
	int32_t FuncRes;

	// Seek to start of image file and read image header, extract & verify header contents, seek to start of image data.
	FuncRes = CAP_ReadHeader(hCAP);
	if (FuncRes != CAP_Status_OK)
	{
		CAP_OutputError(FuncRes);
		return -1;
	}

	// Return target machine type from header.
	FuncRes = CAP_GetHeader_Machine(hCAP, &CAP_Machine);
	if (FuncRes != CAP_Status_OK)
	{
		CAP_OutputError(FuncRes);
		return -1;
	}

	// Return target video type from header.
	FuncRes = CAP_GetHeader_Video(hCAP, &CAP_Video);
	if (FuncRes != CAP_Status_OK)
	{
		CAP_OutputError(FuncRes);
		return -1;
	}

	if (CAP_Machine == CAP_Machine_C64)
	{
		printf("* C64\n");
		TAP_Machine = TAP_Machine_C64;
		TAP_Version = TAPv1;
	}
	else if (CAP_Machine == CAP_Machine_C16)
	{
		printf("* C16\n");
		TAP_Machine = TAP_Machine_C16;
		TAP_Version = TAPv2;
	}
	else if (CAP_Machine == CAP_Machine_VC20)
	{
		printf("* VC20\n");
		TAP_Machine = TAP_Machine_C64;
		TAP_Version = TAPv1;
	}
	else return -1;

	if (CAP_Video == CAP_Video_PAL)
	{
		printf("* PAL\n");
		TAP_Video = TAP_Video_PAL;
	}
	else if (CAP_Video == CAP_Video_NTSC)
	{
		printf("* NTSC\n");
		TAP_Video = TAP_Video_NTSC;
	}
	else return -1;

	// Set all header entries at once.
	FuncRes = TAP_CBM_SetHeader(hTAP, TAP_Machine, TAP_Video, TAP_Version, 0);
	if (FuncRes != TAP_CBM_Status_OK)
	{
		TAP_CBM_OutputError(FuncRes);
		return -1;
	}

	// Seek to start of file & write image header.
	FuncRes = TAP_CBM_WriteHeader(hTAP);
	if (FuncRes != TAP_CBM_Status_OK)
	{
		TAP_CBM_OutputError(FuncRes);
		return -1;
	}

	// Determine frequencies.

	// Return timestamp precision from header.
	FuncRes = CAP_GetHeader_Precision(hCAP, puiTimer_Precision_MHz);
	if (FuncRes != CAP_Status_OK)
	{
		CAP_OutputError(FuncRes);
		return -1;
	}

	if (     (CAP_Machine == CAP_Machine_C64)  && (CAP_Video == CAP_Video_PAL))
		*puiFreq = FREQ_C64_PAL;
	else if ((CAP_Machine == CAP_Machine_C64)  && (CAP_Video == CAP_Video_NTSC))
		*puiFreq = FREQ_C64_NTSC;
	else if ((CAP_Machine == CAP_Machine_VC20) && (CAP_Video == CAP_Video_PAL))
		*puiFreq = FREQ_VIC_PAL;
	else if ((CAP_Machine == CAP_Machine_VC20) && (CAP_Video == CAP_Video_NTSC))
		*puiFreq = FREQ_VIC_NTSC;
	else if ((CAP_Machine == CAP_Machine_C16)  && (CAP_Video == CAP_Video_PAL))
		*puiFreq = FREQ_C16_PAL;
	else if ((CAP_Machine == CAP_Machine_C16)  && (CAP_Video == CAP_Video_NTSC))
		*puiFreq = FREQ_C16_NTSC;
	else
	{
		printf("Error: Can't determine machine frequency.\n");
		return -1;
	}

	printf("\n");

	return 0;
}


// Convert CAP to CBM TAP format.
int32_t CAP2CBMTAP(HANDLE hCAP, HANDLE hTAP)
{
	uint64_t ui64Delta, ui64Delta2, ui64Len;
	uint32_t Timer_Precision_MHz, uiFreq;
	uint8_t  TAPv; // TAP file format version.
	uint8_t  ch;   // Single TAP data byte.
	uint32_t TAP_Counter = 0; // CAP & TAP file byte counters.
	int32_t  FuncRes, ReadFuncRes; // Function call results.

	if (Initialize_TAP_header_and_return_frequencies(hCAP, hTAP, &Timer_Precision_MHz, &uiFreq) != 0)
		return -1;

	// Start conversion CAP->TAP.

	// Get target TAP version from header.
	FuncRes = TAP_CBM_GetHeader_TAPversion(hTAP, &TAPv);
	if (FuncRes != TAP_CBM_Status_OK)
	{
		TAP_CBM_OutputError(FuncRes);
		return -1;
	}

	// Skip first halfwave (time until first pulse starts).
	FuncRes = CAP_ReadSignal(hCAP, &ui64Delta, NULL);
	if (FuncRes == CAP_Status_OK_End_of_file)
	{
		printf("Error: Empty image file.");
		return -1;
	}
	else if (FuncRes == CAP_Status_Error_Reading_data)
	{
		CAP_OutputError(FuncRes);
		return -1;
	}

	// Convert while CAP file signal available.
	while ((ReadFuncRes = CAP_ReadSignal(hCAP, &ui64Delta, NULL)) == CAP_Status_OK)
	{
		if ((TAPv == TAPv0) || (TAPv == TAPv1))
		{
			// Get and add timestamp of falling edge.
			FuncRes = CAP_ReadSignal(hCAP, &ui64Delta2, NULL);
			if (FuncRes == CAP_Status_Error_Reading_data)
			{
				CAP_OutputError(FuncRes);
				return -1;
			}
			else if (FuncRes == CAP_Status_OK_End_of_file)
				break;

			ui64Delta += ui64Delta2;
		}

		ui64Len = (ui64Delta*uiFreq/Timer_Precision_MHz+500000)/1000000;

		if (ui64Len > 2040) // 8*0xff=2040
		{
			// We have a pause.
			if ((TAPv == TAPv0) || (TAPv == TAPv1))
			{
				if (HandlePause(hTAP, ui64Len, NeedEvenSplitNumber, TAPv, &TAP_Counter) == -1)
					return -1;
			}
			else
			{
				if (HandlePause(hTAP, ui64Len, NeedOddSplitNumber, TAPv, &TAP_Counter) == -1)
					return -1;
			}
		}
		else
		{
			// We have a data byte.
			ch = (uint8_t) ((ui64Len+4)/8);
			Check_TAP_CBM_Error_TextRetM1(TAP_CBM_WriteSignal_1Byte(hTAP, ch, &TAP_Counter));
		}
	} // Convert while CAP file signal available.

	if (ReadFuncRes == CAP_Status_Error_Reading_data)
	{
		CAP_OutputError(ReadFuncRes);
		return -1;
	}

	// Set signal byte count in header (sum of all signal bytes).
	FuncRes = TAP_CBM_SetHeader_ByteCount(hTAP, TAP_Counter);
	if (FuncRes != TAP_CBM_Status_OK)
	{
		TAP_CBM_OutputError(FuncRes);
		return -1;
	}

	// Seek to start of file & write image header.
	FuncRes = TAP_CBM_WriteHeader(hTAP);
	if (FuncRes != TAP_CBM_Status_OK)
	{
		TAP_CBM_OutputError(FuncRes);
		return -1;
	}

	return 0;
}
