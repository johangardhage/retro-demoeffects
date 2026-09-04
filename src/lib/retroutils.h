//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROUTILS_H_
#define _RETROUTILS_H_

#include "retro.h"

#pragma pack(push, 1)
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
#pragma pack(pop)

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

struct RETRO_Image24 {
	unsigned char *data; // width * height * 3, R, G, B per pixel
	int width;
	int height;
};

//
// Read a 24-bit PCX: three 8-bit planes (red, green, blue, in that order) per
// scanline rather than one indexed plane, so no trailing palette. A plane's
// scanline is its own RLE run - encoders do not carry a run across the
// plane or scanline boundary - and may be padded past width to BytesPerLine,
// which this discards rather than folding into the image
//
RETRO_Image24 RETRO_LoadImage24(const char *filename)
{
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		RETRO_RageQuit("Cannot open file: %s\n", filename);
	}

	unsigned char header[128];
	if (fread(header, 128, 1, fp) != 1 || header[0] != 10) {
		RETRO_RageQuit("Cannot read file: %s\n", filename);
	}
	if (header[3] != 8 || header[65] != 3) {
		RETRO_RageQuit("Not a 24-bit PCX, expected 3 planes of 8 bits: %s\n", filename);
	}

	int xmin = header[4] + (header[5] << 8);
	int ymin = header[6] + (header[7] << 8);
	int xmax = header[8] + (header[9] << 8);
	int ymax = header[10] + (header[11] << 8);
	int width = xmax - xmin + 1;
	int height = ymax - ymin + 1;
	int bytesperline = header[66] + (header[67] << 8);

	unsigned char *data = (unsigned char *)malloc(width * height * 3);
	if (data == NULL) {
		RETRO_RageQuit("Cannot allocate image data memory\n");
	}
	unsigned char *scanline = (unsigned char *)malloc(bytesperline);
	if (scanline == NULL) {
		RETRO_RageQuit("Cannot allocate scanline memory\n");
	}

	for (int y = 0; y < height; y++) {
		for (int plane = 0; plane < 3; plane++) {
			int index = 0;
			while (index < bytesperline) {
				int value = getc(fp);
				if (value == EOF) {
					RETRO_RageQuit("Cannot read file: %s\n", filename);
				}
				if (value < 192) {
					scanline[index++] = value;
				} else {
					int num = value - 192;
					value = getc(fp);
					if (value == EOF) {
						RETRO_RageQuit("Cannot read file: %s\n", filename);
					}
					while (num-- > 0 && index < bytesperline) {
						scanline[index++] = value;
					}
				}
			}
			for (int x = 0; x < width; x++) {
				data[(y * width + x) * 3 + plane] = scanline[x];
			}
		}
	}

	free(scanline);
	fclose(fp);

	return RETRO_Image24{ data, width, height };
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
