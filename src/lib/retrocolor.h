//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROCOLOR_H_
#define _RETROCOLOR_H_

#include "retro.h"

// *******************************************************************
// Private variables
// *******************************************************************

#define RETRO_BASECOLORS 4096		// 32 colors * 128 shades

#define RETRO_PAL_SIZE 255
#define RETRO_PAL_OFFSET 1

#define RETRO_CUBE_SIZE 64

#define RETRO_K_AMBIENT 0.2
#define RETRO_K_DIFFUSE 0.9
#define RETRO_K_SPECULAR 0.7
#define RETRO_K_ATTENUATION 1.0
#define RETRO_K_FALLOFF 150

#define RETRO_FACE_R 0.86
#define RETRO_FACE_G 0.23
#define RETRO_FACE_B 0.59

#define RETRO_AMBIENT_R 0.0
#define RETRO_AMBIENT_G 0.0
#define RETRO_AMBIENT_B 0.0

#define RETRO_LIGHT_R 0.83
#define RETRO_LIGHT_G 0.83
#define RETRO_LIGHT_B 0.83

#define RETRO_COLOR_RED 0
#define RETRO_COLOR_GREEN 1
#define RETRO_COLOR_BLUE 2

struct {
	unsigned char shadetable[RETRO_BASECOLORS];
	RETRO_Palette basecolor[RETRO_BASECOLORS];
	RETRO_Palette palette[RETRO_COLORS];
	int basecolors;
	int palettecolors;
	int colorcube[RETRO_CUBE_SIZE];
} RETRO_Color;

// *******************************************************************
// Private functions
// *******************************************************************

int	RETRO_ColorMatch(RETRO_Palette basecolor)
{
	int match = 0;
	int mindistance = 12000;
	RETRO_Palette color;

	for (int i = 0; i < RETRO_COLORS; i++) {
		color = RETRO_Color.palette[i];

		// Calculate distance to this color
		int deltar = ((int)color.r - (int)basecolor.r);
		int deltag = ((int)color.g - (int)basecolor.g);
		int deltab = ((int)color.b - (int)basecolor.b);

		int distance = deltar * deltar + deltag * deltag + deltab * deltab;

		// Is this distance shorter than the current match
		if (distance < mindistance) {
			mindistance = distance;
			match = i;
		}
	}

	color = RETRO_Color.palette[match];
	//	printf("base color (%i %i %i) - match (%i %i %i) - distance (%i)\n", basecolor.r, basecolor.g, basecolor.b, color.r, color.g, color.b, mindistance);

	return match;
}

void RETRO_ShrinkColorCube(RETRO_Palette *min, RETRO_Palette *max)
{
	RETRO_Palette newmin = *max;
	RETRO_Palette newmax = *min;
	int colors = 0;

	// printf("Shrink");

	for (int i = 0; i < RETRO_Color.basecolors; i++) {
		RETRO_Palette color = RETRO_Color.basecolor[i];

		// Is it inside the color cube?
		if (color.r >= min->r && color.r < max->r &&
			color.g >= min->g && color.g < max->g &&
			color.b >= min->b && color.b < max->b) {

			// Number of colors in the cube
			colors++;

			// Does this expand out boundaries?
			if (color.r < newmin.r) newmin.r = color.r;
			if (color.g < newmin.g) newmin.g = color.g;
			if (color.b < newmin.b) newmin.b = color.b;

			// Does this expand out boundaries?
			if (color.r >= newmax.r) newmax.r = color.r + 1;
			if (color.g >= newmax.g) newmax.g = color.g + 1;
			if (color.b >= newmax.b) newmax.b = color.b + 1;
		}
	}

	// printf("change (%i %i %i) - (%i %i %i) colors (%i)\n", newmin.r - min->r, newmin.g - min->g, newmin.b - min->b, max->r - newmax.r, max->g - newmax.g, max->b - newmax.b, numcolors);
	*min = newmin;
	*max = newmax;
}

void RETRO_DetermineDivision(RETRO_Palette min, RETRO_Palette max, int longest, RETRO_Palette *minsplit, RETRO_Palette *maxsplit)
{
	int colors = 0;

	// Clear count
	for (int i = 0; i < RETRO_CUBE_SIZE; i++) {
		RETRO_Color.colorcube[i] = 0;
	}

	for (int i = 0; i < RETRO_Color.basecolors; i++) {
		RETRO_Palette color = RETRO_Color.basecolor[i];

		// Is it inside the color cube?
		if (color.r >= min.r && color.r < max.r &&
			color.g >= min.g && color.g < max.g &&
			color.b >= min.b && color.b < max.b) {

			// Number of colors in the cube
			colors++;

			switch (longest) {
			case RETRO_COLOR_RED:
				RETRO_Color.colorcube[color.r]++;
				break;
			case RETRO_COLOR_GREEN:
				RETRO_Color.colorcube[color.g]++;
				break;
			case RETRO_COLOR_BLUE:
				RETRO_Color.colorcube[color.b]++;
				break;
			}
		}
	}

	// printf("colors (%i) ", colors);

	// How many colors until the median
	int mediancolor = colors / 2;
	int mediancomp = 0;

	// printf("mcount (%i) ", mediancolor);

	// Determine at what color is the median reached
	for (int i = 0; i < RETRO_CUBE_SIZE; i++) {
		mediancolor -= RETRO_Color.colorcube[i];

		if (mediancolor <= 0) {
			mediancomp = i;
			break;
		}
	}
	mediancomp += 1;
	//	printf ( "median (%i)\n", mediancomp );

	*minsplit = max;
	*maxsplit = min;

	switch (longest) {
	case RETRO_COLOR_RED:
		minsplit->r = mediancomp;
		maxsplit->r = mediancomp;
		break;
	case RETRO_COLOR_GREEN:
		minsplit->g = mediancomp;
		maxsplit->g = mediancomp;
		break;
	case RETRO_COLOR_BLUE:
		minsplit->b = mediancomp;
		maxsplit->b = mediancomp;
		break;
	}
}

void RETRO_ProcessColorCube(RETRO_Palette min, RETRO_Palette max, int level)
{
	// printf("incoming (%i) (%i %i %i) - (%i %i %i)\n", level, min.r, min.g, min.b, max.r, max.g, max.b );

	// Shrink the color cube to contain just the used colors
	RETRO_ShrinkColorCube(&min, &max);

	// printf ( "skrunk cube (%i %i %i) - (%i %i %i)\n", min.r, min.g, min.b, max.r, max.g, max.b );

	// Find the length of the sides of the color cube
	int deltar = max.r - min.r;
	int deltag = max.g - min.g;
	int deltab = max.b - min.b;

	// printf("deltas (%i %i %i)", deltar, deltag, deltab);

	// If this is the last level then stop
	if (level == 0) {
		RETRO_Palette color;

		color.r = min.r + (deltar / 2);
		color.g = min.g + (deltag / 2);
		color.b = min.b + (deltab / 2);

		if (color.r > 63) color.r = 63;
		if (color.g > 63) color.g = 63;
		if (color.b > 63) color.b = 63;

		RETRO_Color.palette[RETRO_Color.palettecolors] = color;
		RETRO_Color.palettecolors++;

		// printf("color selected (%i) (%i %i %i)\n", palcount, color.r, color.g, color.b);
	} else {
		// Subdivide the cube...

		// Determine which size is longest
		int longest = 0;
		if (deltab >= deltar && deltab >= deltag) {
			longest = RETRO_COLOR_BLUE;
		} else if (deltar >= deltag && deltar >= deltab) {
			longest = RETRO_COLOR_RED;
		} else if (deltag >= deltar && deltag >= deltab) {
			longest = RETRO_COLOR_GREEN;
		}

		// Split the color cube at the median point on the longest side
		RETRO_Palette minsplit, maxsplit;
		RETRO_DetermineDivision(min, max, longest, &minsplit, &maxsplit);

		// printf(" dividing (%i)\n", level);

		// printf("upper cube (%i %i %i) - (%i %i %i)\n", min.r, min.g, min.b, minsplit.r, minsplit.g, minsplit.b);
		// printf("lower cube (%i %i %i) - (%i %i %i)\n", maxsplit.r, maxsplit.g, maxsplit.b, max.r, max.g, max.b);

		// Reduce the level counter
		level--;

		// Continue the process ..
		RETRO_ProcessColorCube(min, minsplit, level);
		RETRO_ProcessColorCube(maxsplit, max, level);
	}
}

float RETRO_PhongIllumination(float diffusecolor, float specularcolor, float lightcolor, float ambientcolor, float theta)
{
	float phongcolor = (RETRO_K_DIFFUSE * diffusecolor * cos(theta));
	if (theta < M_PI / 4) {
		phongcolor += (RETRO_K_SPECULAR * specularcolor * pow(cos(theta * 2), RETRO_K_FALLOFF));
	}
	phongcolor *= (RETRO_K_ATTENUATION * lightcolor);
	phongcolor += (ambientcolor * RETRO_K_AMBIENT * diffusecolor);

	// Clip phong color to between 0.0 and 1.0
	if (phongcolor > 1.0) {
		phongcolor = 1.0;
	} else if (phongcolor < 0.0) {
		phongcolor = 0.0;
	}

	return phongcolor;
}

void RETRO_CreateBaseColors(RETRO_Palette *palette, int colors)
{
	RETRO_Color.basecolors = 0;

	for (int i = 0; i < colors; i++) {
		RETRO_Palette texcolor = palette[i];

		if (texcolor.r > 63) texcolor.r = 63;
		if (texcolor.g > 63) texcolor.g = 63;
		if (texcolor.b > 63) texcolor.b = 63;

		for (int shade = 0; shade < 128; shade++) {
			float incedent = ((float)(128 - (shade + 1)) / 128) * (M_PI / 2);

			RETRO_Palette color;
			color.r = 63 * RETRO_PhongIllumination(texcolor.r / 63.0, 1.0, RETRO_LIGHT_R, RETRO_AMBIENT_R, incedent);
			color.g = 63 * RETRO_PhongIllumination(texcolor.g / 63.0, 1.0, RETRO_LIGHT_G, RETRO_AMBIENT_G, incedent);
			color.b = 63 * RETRO_PhongIllumination(texcolor.b / 63.0, 1.0, RETRO_LIGHT_B, RETRO_AMBIENT_B, incedent);

			RETRO_Color.basecolor[RETRO_Color.basecolors] = color;
			RETRO_Color.basecolors++;
		}
	}
}

// *******************************************************************
// Public functions
// *******************************************************************

RETRO_Palette *RETRO_OptimalPalette(void)
{
	return RETRO_Color.palette;
}

unsigned char *RETRO_ShadeTable(void)
{
	return RETRO_Color.shadetable;
}

void RETRO_CreateOptimalPaletteAndShadeTable(RETRO_Palette *palette, int colors)
{
	RETRO_CreateBaseColors(palette, colors);

	RETRO_Palette min = { 0, 0, 0 };
	RETRO_Palette max = { RETRO_CUBE_SIZE, RETRO_CUBE_SIZE, RETRO_CUBE_SIZE };
	RETRO_ProcessColorCube(min, max, 8);

	for (int i = 0; i < RETRO_Color.basecolors; i++) {
		RETRO_Color.shadetable[i] = RETRO_ColorMatch(RETRO_Color.basecolor[i]);
	}
}

void RETRO_CreatePlasticPhongPalette(RETRO_Palette *palette)
{
	// Create phong palette
	palette[0].r = 0;
	palette[0].g = 0;
	palette[0].b = 0;

	for (int i = 0; i < RETRO_PAL_SIZE; i++) {
		// what is the incedent angle that we are calculating a color for?
		// Scale loop to 0 to 1, flip then scale it up to 0 to 90 degrees.
		// I say degrees but remember that the computer works in radians.
		float incedent = ((float)(RETRO_PAL_SIZE - (i + 1)) / RETRO_PAL_SIZE) * (M_PI / 2);
		int baseindex = (i + RETRO_PAL_OFFSET);

		// Determine the rgb components
		palette[baseindex].r = 63 * RETRO_PhongIllumination(RETRO_FACE_R, 1.0, RETRO_LIGHT_R, RETRO_AMBIENT_R, incedent);
		palette[baseindex].g = 63 * RETRO_PhongIllumination(RETRO_FACE_G, 1.0, RETRO_LIGHT_G, RETRO_AMBIENT_G, incedent);
		palette[baseindex].b = 63 * RETRO_PhongIllumination(RETRO_FACE_B, 1.0, RETRO_LIGHT_B, RETRO_AMBIENT_B, incedent);
	}
}

void RETRO_CreatePhongMap(unsigned char *buffer, int width, int height)
{
	// clear the map
	memset(buffer, 0, width * height);

	// calculate the top left quadrant of the phong enviroment map
	for (int x = 0; x < 128; x++) {
		for (int y = 0; y < 128; y++) {
			// determine the angle that will have this pixel mapped to
			// it in radian.
			float xcomp = (127.5 - x) / 127.5 * (M_PI / 2);
			float ycomp = (127.5 - y) / 127.5 * (M_PI / 2);

			// lets find "1 - (x^2 + y^2)" from above.
			float temp = 1.0 - (pow(sin(xcomp), 2) + pow(sin(ycomp), 2));

			// we can only that the sqrt if temp is positive.  Also if temp is negative
			//	the pixel in question will never be mapped onto the object so we don't
			// care about it.
			if (temp >= 0.0) {
				// we now get the z component of the normal vector
				float zcomp = sqrt(temp);

				// find the angle
				float incedent = asin(zcomp);

				// now that we have the angle lets scale down to 0 to 1 since it is
				// an angle between 0 and 90 except it's in radians.
				// Then scale it up the be a palette index
				unsigned char paletteindex = (unsigned char)(incedent / (M_PI / 2) * RETRO_PAL_SIZE + RETRO_PAL_OFFSET);

				// put the index into the phongmap
				buffer[y * 256 + x] = paletteindex;
			}
		}
	}

	// copy the quadrant we just calculated to the other three (we flip)
	for (int x = 0; x < 128; x++) {
		for (int y = 0; y < 128; y++) {
			buffer[y * 256 + (255 - x)] = buffer[y * 256 + x]; // top right
			buffer[(255 - y) * 256 + x] = buffer[y * 256 + x]; // bottom left
			buffer[(255 - y) * 256 + (255 - x)] = buffer[y * 256 + x]; // bottom right
		}
	}
}

void RETRO_CreateMiniPhongMap(unsigned char *buffer, int width, int height)
{
	// clear the map
	memset(buffer, 0, width * height);

	// calculate the top left quadrant of the phong enviroment map
	for (int x = 0; x < 128; x++) {
		for (int y = 0; y < 128; y++) {
			// determine the angle that will have this pixel mapped to
			// it in radian.
			float xcomp = (127.5 - x) / 127.5 * (M_PI / 2);
			float ycomp = (127.5 - y) / 127.5 * (M_PI / 2);

			// lets find "1 - (x^2 + y^2)" from above.
			float temp = 1.0 - (pow(sin(xcomp), 2) + pow(sin(ycomp), 2));

			// we can only that the sqrt if temp is positive.  Also if temp is negative
			//	the pixel in question will never be mapped onto the object so we don't
			// care about it.
			if (temp >= 0.0) {
				// we now get the z component of the normal vector
				float zcomp = sqrt(temp);

				// find the angle
				float incedent = asin(zcomp);

				// now that we have the angle lets scale down to 0 to 1 since it is
				// an angle between 0 and 90 except it's in radians.
				// Then scale it up the be a palette index
				unsigned char paletteindex = (unsigned char)(incedent / (M_PI / 2) * 128);

				// put the index into the phongmap
				buffer[y * 256 + x] = paletteindex;
			}
		}
	}

	// copy the quadrant we just calculated to the other three (we flip)
	for (int x = 0; x < 128; x++) {
		for (int y = 0; y < 128; y++) {
			buffer[y * 256 + (255 - x)] = buffer[y * 256 + x]; // top right
			buffer[(255 - y) * 256 + x] = buffer[y * 256 + x]; // bottom left
			buffer[(255 - y) * 256 + (255 - x)] = buffer[y * 256 + x]; // bottom right
		}
	}
}

#endif
