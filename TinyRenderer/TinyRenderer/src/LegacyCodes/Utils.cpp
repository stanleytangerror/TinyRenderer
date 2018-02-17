#include "Utils.h"

//////////////////////////////////////////////
// BMP interface
//////////////////////////////////////////////
BYTE* ConvertRGBToBMPBuffer(IUINT32 const * const * Buffer, int width, int height,
	long & newsize)
{
	if ((NULL == Buffer) || (width == 0) || (height == 0))
		return NULL;

	// find the number of bytes the buffer has to be padded with and length of padded scanline :
	int padding = 0;
	int scanlinebytes = width * 3;
	while ((scanlinebytes + padding) % 4 != 0)
		padding++;
	int psw = scanlinebytes + padding;

	// calculate the size of the padded buffer and create it :
	newsize = height * psw;
	BYTE* newbuf = new BYTE[newsize];

	// Now we could of course copy the old buffer to the new one, 
	// flip it and change RGB to GRB and then fill every scanline 
	// with zeroes to the next DWORD boundary, but we are smart coders of course, 
	// so we don't bother with any actual padding at all, but just initialize the 
	// whole buffer with zeroes and then copy the color values to their new positions 
	// without touching those padding-bytes anymore :)

	memset(newbuf, 0, newsize);

	long bufpos = 0;
	long newpos = 0;
	for (int y = 0; y < height; y++) for (int x = 0; x < width; x += 1)
	{
		//bufpos = y * width + x;     // position in original buffer
		newpos = (height - y - 1) * psw + x * 3; // position in padded buffer
		newbuf[newpos] = Buffer[y][x] >> 16;       // swap r and b
		newbuf[newpos + 1] = Buffer[y][x] >> 8; // g stays
		newbuf[newpos + 2] = Buffer[y][x];     // swap b and r
	}
	return newbuf;
}

bool SaveBMP(BYTE* Buffer, int width, int height, long paddedsize, LPCTSTR bmpfile)
{
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER info;
	memset(&bmfh, 0, sizeof(BITMAPFILEHEADER));
	memset(&info, 0, sizeof(BITMAPINFOHEADER));

	// Next we fill the file header with data :
	bmfh.bfType = 0x4d42;       // 0x4d42 = 'BM'
	bmfh.bfReserved1 = 0;
	bmfh.bfReserved2 = 0;
	bmfh.bfSize = sizeof(BITMAPFILEHEADER) +
		sizeof(BITMAPINFOHEADER) + paddedsize;
	bmfh.bfOffBits = 0x36;

	// and the info header :
	info.biSize = sizeof(BITMAPINFOHEADER);
	info.biWidth = width;
	info.biHeight = height;
	info.biPlanes = 1;
	info.biBitCount = 24;
	info.biCompression = BI_RGB;
	info.biSizeImage = 0;
	info.biXPelsPerMeter = 0x0ec4;
	info.biYPelsPerMeter = 0x0ec4;
	info.biClrUsed = 0;
	info.biClrImportant = 0;


	HANDLE file = CreateFile(bmpfile, GENERIC_WRITE, FILE_SHARE_READ,
		NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (NULL == file)
	{
		CloseHandle(file);
		return false;
	}

	// Now we write the file header and info header :
	unsigned long bwritten;
	if (WriteFile(file, &bmfh, sizeof(BITMAPFILEHEADER),
		&bwritten, NULL) == false)
	{
		CloseHandle(file);
		return false;
	}

	if (WriteFile(file, &info, sizeof(BITMAPINFOHEADER),
		&bwritten, NULL) == false)
	{
		CloseHandle(file);
		return false;
	}

	// and finally the image data :
	if (WriteFile(file, Buffer, paddedsize, &bwritten, NULL) == false)
	{
		CloseHandle(file);
		return false;
	}

	CloseHandle(file);
	return true;
}