//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROUTILS_H_
#define _RETROUTILS_H_

#include "retro.h"

struct {
	unsigned char Identifier = 10;            // PCX Id Number (Always 0x0A)
	unsigned char Version = 5;                // Version Number
	unsigned char Encoding = 1;               // Encoding Format
	unsigned char BitsPerPixel = 8;           // Bits per Pixel
	unsigned short int XStart = 0;            // Left of image
	unsigned short int YStart = 0;            // Top of Image
	unsigned short int XEnd = 319;            // Right of Image
	unsigned short int YEnd = 199;            // Bottom of image
	unsigned short int HorzRes = 320;         // Horizontal Resolution
	unsigned short int VertRes = 200;         // Vertical Resolution
	unsigned char Palette[48];                // 16-Color EGA Palette
	unsigned char Reserved1 = 0;              // Reserved (Always 0)
	unsigned char NumBitPlanes = 1;           // Number of Bit Planes
	unsigned short int BytesPerLine = 320;    // Bytes per Scan-line
	unsigned short int PaletteType = 1;       // Palette Type
	unsigned short int HorzScreenSize = 0;    // Horizontal Screen Size
	unsigned short int VertScreenSize = 0;    // Vertical Screen Size
	unsigned char Reserved2[54];              // Reserved (Always 0)
} RETRO_PcxHead;

void RETRO_SaveImage(const char *filename, unsigned char *image, RETRO_Palette *palette, int width, int height)
{
	// Populate header
	RETRO_PcxHead.XEnd = width - 1;
	RETRO_PcxHead.YEnd = height - 1;
	RETRO_PcxHead.HorzRes = width;
	RETRO_PcxHead.VertRes = height;
	RETRO_PcxHead.BytesPerLine = width;

	// Open file
	FILE *fp = fopen(filename, "wb");
	if (fp == NULL) {
		RETRO_RageQuit("Cannot open file: %s\n", filename);
	}

	// Write header
	fwrite(&RETRO_PcxHead, sizeof(RETRO_PcxHead), 1, fp);

	// Encode and write image data
	for (int y = 0; y < height; y++) {
		unsigned char *buffer = image + y * width;
		unsigned char last = *(buffer++);
		int runcount = 1;
		for (int x = 1; x < width; x++) {
			unsigned char current = *(buffer++);
			// There is a "run" in the data, encode it
			if (current == last) {
				runcount++;
				if (runcount == 63) {
					putc(0xC0 | runcount, fp);
					putc(last, fp);
					runcount = 0;
				}
			} else {
				if (runcount) {
					if ((runcount == 1) && (0xC0 != (0xC0 & last))) {
						putc(last, fp);
					} else {
						putc(0xC0 | runcount, fp);
						putc(last, fp);
					}
				}
				last = current;
				runcount = 1;
			}
		}
		// Finish up
		if (runcount) {
			if ((runcount == 1) && (0xC0 != (0xC0 & last))) {
				putc(last, fp);
			} else {
				putc(0xC0 | runcount, fp);
				putc(last, fp);
			}
		}
	}

	// Write VGA palette marker
	fputc(0x0C, fp);

	// Write palette
	for (int i = 0; i < 256; i++) {
		fputc(palette[i].r, fp);
		fputc(palette[i].g, fp);
		fputc(palette[i].b, fp);
	}

	fclose(fp);
}

void RETRO_LoadAsset(const char *filename, void *buffer, int size = 0, int number = 1)
{
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		RETRO_RageQuit("Cannot open file: %s\n", filename);
	}
	if (size == 0) {
		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
	}
	fread(buffer, size, number, fp);
	fclose(fp);
}

#endif
