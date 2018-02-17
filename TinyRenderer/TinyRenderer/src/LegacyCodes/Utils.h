#ifndef UTILS_H
#define UTILS_H

#include "Types.h"

#include <tchar.h>
#include <Windows.h>


//////////////////////////////////////////////
// BMP interface
//////////////////////////////////////////////

BYTE* ConvertRGBToBMPBuffer(IUINT32 const * const * Buffer, int width, int height, long & newsize);

bool SaveBMP(BYTE* Buffer, int width, int height, long paddedsize, LPCTSTR bmpfile);

#endif
