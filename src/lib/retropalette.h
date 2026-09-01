//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROPALETTE_H_
#define _RETROPALETTE_H_

#include "retro.h"

// *******************************************************************
// Public variables
// *******************************************************************

// Named colors, with 8-bit components as used by RETRO_SetPalette. A demo can
// name its own colors the same way, for example #define EMBER RETRO_RGB(0x140014)

#define RETRO_RGB(hex) RETRO_Palette{ ((hex) >> 16) & 0xff, ((hex) >> 8) & 0xff, (hex) & 0xff }

#define RETRO_BLACK RETRO_Palette{ 0, 0, 0 }
#define RETRO_GRAY RETRO_Palette{ 128, 128, 128 }
#define RETRO_LIGHTGRAY RETRO_Palette{ 200, 200, 200 }
#define RETRO_HAZE RETRO_Palette{ 168, 188, 196 }
#define RETRO_WHITE RETRO_Palette{ 255, 255, 255 }
#define RETRO_RED RETRO_Palette{ 255, 0, 0 }
#define RETRO_GREEN RETRO_Palette{ 0, 255, 0 }
#define RETRO_BLUE RETRO_Palette{ 0, 0, 255 }
#define RETRO_CYAN RETRO_Palette{ 0, 255, 255 }
#define RETRO_MAGENTA RETRO_Palette{ 255, 0, 255 }
#define RETRO_YELLOW RETRO_Palette{ 255, 255, 0 }
#define RETRO_SCARLET RETRO_Palette{ 255, 36, 0 }
#define RETRO_BRICK RETRO_Palette{ 116, 48, 52 }
#define RETRO_ORANGE RETRO_Palette{ 255, 128, 0 }
#define RETRO_CARROT RETRO_Palette{ 240, 132, 20 }
#define RETRO_SIENNA RETRO_Palette{ 180, 80, 44 }
#define RETRO_AMBER RETRO_Palette{ 255, 191, 0 }
#define RETRO_SAFFRON RETRO_Palette{ 252, 168, 56 }
#define RETRO_GOLD RETRO_Palette{ 254, 204, 0 }
#define RETRO_JASMINE RETRO_Palette{ 252, 212, 120 }
#define RETRO_TAN RETRO_Palette{ 210, 180, 140 }
#define RETRO_SADDLEBROWN RETRO_Palette{ 139, 69, 19 }
#define RETRO_SAGE RETRO_Palette{ 164, 164, 140 }
#define RETRO_PURPLE RETRO_Palette{ 128, 0, 255 }
#define RETRO_INDIGO RETRO_Palette{ 75, 0, 130 }
#define RETRO_DARKINDIGO RETRO_Palette{ 8, 0, 40 }
#define RETRO_VIOLET RETRO_Palette{ 148, 0, 211 }
#define RETRO_PINK RETRO_Palette{ 255, 128, 192 }
#define RETRO_HOTPINK RETRO_Palette{ 255, 105, 180 }
#define RETRO_DEEPPINK RETRO_Palette{ 219, 59, 150 }
#define RETRO_ROSE RETRO_Palette{ 172, 92, 132 }
#define RETRO_PINKLACE RETRO_Palette{ 252, 212, 252 }
#define RETRO_PERIWINKLE RETRO_Palette{ 160, 168, 252 }
#define RETRO_AZURE RETRO_Palette{ 0, 128, 255 }
#define RETRO_CERULEAN RETRO_Palette{ 0, 106, 167 }
#define RETRO_MEDIUMRED RETRO_Palette{ 187, 0, 0 }
#define RETRO_LIGHTRED RETRO_Palette{ 255, 204, 204 }
#define RETRO_LIGHTBLUE RETRO_Palette{ 102, 170, 255 }
#define RETRO_DARKRED RETRO_Palette{ 128, 0, 0 }
#define RETRO_DARKGREEN RETRO_Palette{ 0, 128, 0 }
#define RETRO_HUNTERGREEN RETRO_Palette{ 42, 86, 36 }
#define RETRO_SEAGREEN RETRO_Palette{ 46, 139, 87 }
#define RETRO_MEDIUMSEAGREEN RETRO_Palette{ 60, 179, 113 }
#define RETRO_SPRINGGREEN RETRO_Palette{ 30, 230, 90 }
#define RETRO_MOSSGREEN RETRO_Palette{ 148, 176, 70 }
#define RETRO_TEAL RETRO_Palette{ 0, 128, 128 }
#define RETRO_DARKCYAN RETRO_Palette{ 0, 139, 139 }
#define RETRO_DARKSLATEGRAY RETRO_Palette{ 47, 79, 79 }
#define RETRO_STEELBLUE RETRO_Palette{ 70, 130, 180 }
#define RETRO_GLAUCOUS RETRO_Palette{ 92, 124, 160 }
#define RETRO_LIGHTSKYBLUE RETRO_Palette{ 135, 206, 250 }
#define RETRO_DARKBLUE RETRO_Palette{ 0, 0, 128 }
#define RETRO_SPACECADET RETRO_Palette{ 28, 40, 88 }
#define RETRO_NIGHTSKY RETRO_Palette{ 20, 24, 42 }
#define RETRO_MIDNIGHTBLUE RETRO_Palette{ 25, 25, 112 }
#define RETRO_DARKMAGENTA RETRO_Palette{ 139, 0, 139 }
#define RETRO_REBECCAPURPLE RETRO_Palette{ 102, 51, 153 }
#define RETRO_BLUEBLACK RETRO_Palette{ 0, 0, 48 }
#define RETRO_WINE RETRO_Palette{ 44, 24, 36 }
#define RETRO_SLATEGRAY RETRO_Palette{ 112, 128, 144 }
#define RETRO_DARKSLATEBLUE RETRO_Palette{ 72, 61, 139 }
#define RETRO_SKYBLUE RETRO_Palette{ 135, 206, 235 }
#define RETRO_IVORY RETRO_Palette{ 255, 255, 240 }
#define RETRO_GHOSTWHITE RETRO_Palette{ 248, 248, 255 }
#define RETRO_DARKVIOLET RETRO_Palette{ 148, 0, 211 }
#define RETRO_DARKMIDNIGHTBLUE RETRO_Palette{ 14, 36, 54 }
#define RETRO_MUTEDINDIGO RETRO_Palette{ 55, 36, 68 }
#define RETRO_LIGHTSLATEGRAY RETRO_Palette{ 142, 137, 159 }
#define RETRO_MUTEDDARKSLATEBLUE RETRO_Palette{ 65, 61, 88 }
#define RETRO_PALESKYBLUE RETRO_Palette{ 128, 225, 239 }
#define RETRO_SOFTIVORY RETRO_Palette{ 248, 249, 240 }
#define RETRO_BRIGHTBLUE RETRO_Palette{ 18, 39, 255 }
#define RETRO_BRIGHTCYAN RETRO_Palette{ 0, 232, 255 }
#define RETRO_SOFTGHOSTWHITE RETRO_Palette{ 250, 250, 255 }
#define RETRO_LIGHTMAGENTA RETRO_Palette{ 255, 66, 247 }
#define RETRO_DEEPDARKVIOLET RETRO_Palette{ 120, 20, 196 }
#define RETRO_ROYALINDIGO RETRO_Palette{ 58, 26, 110 }
#define RETRO_DARKTURQUOISE RETRO_Palette{ 48, 200, 224 }
#define RETRO_FORESTGREEN RETRO_Palette{ 61, 122, 53 }
#define RETRO_FIREBRICK RETRO_Palette{ 187, 40, 32 }
#define RETRO_CHARCOAL RETRO_Palette{ 54, 54, 60 }
#define RETRO_DIMGRAY RETRO_Palette{ 66, 66, 74 }
#define RETRO_PINETREE RETRO_Palette{ 30, 74, 40 }

// *******************************************************************
// Private variables
// *******************************************************************

// A phong palette keeps entry 0 black and ramps the material over the rest, so a
// renderer shades from RETRO_PHONG_OFFSET across RETRO_PHONG_SHADES entries
#define RETRO_PHONG_OFFSET 1
#define RETRO_PHONG_SHADES (RETRO_COLORS - RETRO_PHONG_OFFSET)

// Coefficients of the phong reflection model
#define RETRO_K_AMBIENT 0.2
#define RETRO_K_DIFFUSE 0.9
#define RETRO_K_SPECULAR 0.7
#define RETRO_K_ATTENUATION 1.0
#define RETRO_K_FALLOFF 150

// The light a material is lit by, as intensities between 0.0 and 1.0
#define RETRO_AMBIENT_R 0.0
#define RETRO_AMBIENT_G 0.0
#define RETRO_AMBIENT_B 0.0

#define RETRO_LIGHT_R 0.83
#define RETRO_LIGHT_G 0.83
#define RETRO_LIGHT_B 0.83

// *******************************************************************
// Private functions
// *******************************************************************

//
// The angle of incidence a shade stands for. The last shade is face on (0);
// the first is one step short of grazing, so theta lives on [0, π/2):
//
//   theta = (shades − (shade + 1)) / shades * (π / 2)
//
// Shades are spaced evenly in the angle, not in cos(theta). Spacing them in
// cos(theta) would look like the obvious choice, since a renderer finds a
// surface's lighting from the dot product of its normal with the light, but
// cos(theta) is flat near theta = 0 and would crowd almost no shades into the
// small angles where the specular highlight lives. At a falloff of 30 the
// bright core of the highlight gets 18 shades this way and only 2 the other.
// A renderer therefore has to convert its dot product with
// RETRO_ShadeFromLambert before using it to pick a shade
//
float RETRO_IncidenceAngle(int shade, int shades)
{
	return ((float)(shades - (shade + 1)) / shades) * (M_PI / 2);
}

//
// The shade a surface takes, from the dot product of its normal with the light.
// Undoes the angle spacing that RETRO_IncidenceAngle lays down, so that a
// surface lands on the shade actually built for its lighting
//
//   N·L = cos(theta)  ->  1 - acos(N·L) / (pi / 2)  =  1 - theta / (pi / 2)
//
float RETRO_ShadeFromLambert(float lambert)
{
	// theta = acos(N·L) in [0, π/2], then 1 - theta/(π/2) so face-on is 1.
	return 1.0f - acos(CLAMP01(lambert)) / (M_PI / 2);
}

//
// The phong reflection model for one color channel, as an intensity between 0.0
// and 1.0
//
//   N·L       = cos(theta)
//   R·V       = 2(N·L)^2 - 1          (V = L)
//   diffuse   = Kd * face * max(N·L, 0)
//   specular  = Ks * max(R·V, 0)^n
//   ambient   = Ka * ambient * face
//   intensity = (diffuse + specular) * Katt * light + ambient
//
// This is textbook Phong, specialised to a viewer sitting at the light. Phong
// writes the highlight as (R . V)^n for a reflection vector R and a view vector
// V; with V = L the angle between them is twice the angle of incidence, so
// R . V becomes cos(2 * theta) and the whole model collapses to the one angle
// theta. Past 45 degrees the reflection points away from the viewer and the
// highlight is gone, which is the usual max(R . V, 0) clamp.
//
// Only the diffuse and ambient terms are tinted by the face color. The
// highlight stays the color of the light, which is what makes a material read
// as plastic rather than metal. A specularity of 0 drops the highlight and
// leaves plain lambert diffuse, which is the matte end of the same model
//
float RETRO_PhongIntensity(float facecolor, float lightcolor, float ambientcolor, float theta, float specularity, float falloff)
{
	// Viewer sits at the light, so V = L and R · V = 2(N·L)^2 - 1 = cos(2 theta).
	float ndotl = cos(theta);
	float rdotv = 2.0f * ndotl * ndotl - 1.0f;

	float diffuse = RETRO_K_DIFFUSE * facecolor * MAX(ndotl, 0.0f);
	float specular = specularity * pow(MAX(rdotv, 0.0f), falloff);
	float ambient = ambientcolor * RETRO_K_AMBIENT * facecolor;

	float intensity = (diffuse + specular) * RETRO_K_ATTENUATION * lightcolor + ambient;

	return CLAMP01(intensity);
}

//
// Shade one face color across the whole range of incidence, from black at a
// grazing angle up to the specular highlight face on. Components of the face
// color and of the ramp share the same scale, given by colormax, so the same
// call fills a 6-bit ramp as readily as an 8-bit one
//
void RETRO_CreatePhongRamp(RETRO_Palette *ramp, int shades, RETRO_Palette face, float specularity, float falloff, int colormax)
{
	for (int shade = 0; shade < shades; shade++) {
		float theta = RETRO_IncidenceAngle(shade, shades);

		ramp[shade].r = colormax * RETRO_PhongIntensity(face.r / (double)colormax, RETRO_LIGHT_R, RETRO_AMBIENT_R, theta, specularity, falloff);
		ramp[shade].g = colormax * RETRO_PhongIntensity(face.g / (double)colormax, RETRO_LIGHT_G, RETRO_AMBIENT_G, theta, specularity, falloff);
		ramp[shade].b = colormax * RETRO_PhongIntensity(face.b / (double)colormax, RETRO_LIGHT_B, RETRO_AMBIENT_B, theta, specularity, falloff);
	}
}

//
// Lay one material's ramp over [start, end), the phong counterpart of
// RETRO_CreateGradientPalette. Index end is not written, so a palette can
// carry a material per range and a model can pick between them per face.
// There is no color at end: the last written entry is the face-on highlight.
// A last range with end = RETRO_COLORS therefore writes that highlight at 255.
//
void RETRO_CreatePhongPalette(int start, int end, RETRO_Palette face, float specularity = RETRO_K_SPECULAR, float falloff = RETRO_K_FALLOFF, RETRO_Palette *palette = NULL, int colormax = 255)
{
	int shades = MIN(end - start, RETRO_PHONG_SHADES);
	RETRO_Palette ramp[RETRO_PHONG_SHADES];
	RETRO_CreatePhongRamp(ramp, shades, face, specularity, falloff, colormax);

	for (int i = 0; i < shades; i++) {
		RETRO_SetColor(start + i, ramp[i], palette);
	}
}

//
// Fill a palette with one material, keeping entry 0 black and laying the ramp
// over the rest. Without a palette the colors are set directly, otherwise they
// are written to it, in both cases scaled to colormax so that a 6-bit palette
// can be filled as readily as an 8-bit one
//
void RETRO_CreateMaterialPalette(RETRO_Palette face, float specularity, float falloff, RETRO_Palette *palette, int colormax)
{
	RETRO_SetColor(0, RETRO_BLACK, palette);
	RETRO_CreatePhongPalette(RETRO_PHONG_OFFSET, RETRO_PHONG_OFFSET + RETRO_PHONG_SHADES, face, specularity, falloff, palette, colormax);
}

// *******************************************************************
// Public functions
// *******************************************************************

//
// Fill [start, end) with a linear interpolation from one color toward another.
// to is the color at end, which is not written, so the next ramp can start
// there and write the knot as its from without both touching it. A last ramp
// with end = RETRO_COLORS therefore leaves 255 one step short of to: that
// index is off the palette. That is the range, not a missing write.
// Without a palette the colors are set directly, otherwise the components are
// copied as they are given, which allows both 6-bit and 8-bit palettes
//
//   C(i) = from + ((i - start) / (end - start)) * (to - from),  i ∈ [start, end)
void RETRO_CreateGradientPalette(int start, int end, RETRO_Palette from, RETRO_Palette to, RETRO_Palette *palette = NULL)
{
	int steps = end - start;

	for (int i = 0; i < steps; i++) {
		float k = (float)i / steps;

		RETRO_Palette color;
		color.r = from.r + (to.r - from.r) * k;
		color.g = from.g + (to.g - from.g) * k;
		color.b = from.b + (to.b - from.b) * k;

		RETRO_SetColor(start + i, color, palette);
	}
}

//
// Fill the palette with a plastic phong material, shading the face color from
// black up to a specular highlight. The highlight is white whatever the face
// color, which is what makes the material read as plastic, and a lower falloff
// spreads it over more of the palette. Without a palette the colors are set
// directly, otherwise they are written to it, in both cases scaled to colormax
// so that a 6-bit palette can be filled as readily as an 8-bit one
//
void RETRO_CreatePlasticPhongPalette(float falloff = RETRO_K_FALLOFF, RETRO_Palette face = RETRO_DEEPPINK, RETRO_Palette *palette = NULL, int colormax = 255)
{
	RETRO_CreateMaterialPalette(face, RETRO_K_SPECULAR, falloff, palette, colormax);
}

//
// Fill the palette with a matte material, shading the face color from black up
// to full lambert diffuse with no specular highlight at all
//
// This is the material to shade a flat lit model with. A highlight needs a
// normal that varies across a face to be drawn as a highlight, and flat shading
// gives a face one normal for all of it, so the whole face crosses into the
// highlight at once and blinks white. Gouraud and phong interpolate a normal and
// can carry RETRO_CreatePlasticPhongPalette instead
//
void RETRO_CreateMattePalette(RETRO_Palette face = RETRO_DEEPPINK, RETRO_Palette *palette = NULL, int colormax = 255)
{
	RETRO_CreateMaterialPalette(face, 0.0, RETRO_K_FALLOFF, palette, colormax);
}

//
// Fill a buffer with an environment map of the lighting on a sphere, as indices
// into a phong palette. The map is read with the screen space normal of a
// surface, so it stands in for the whole lighting calculation at render time
//
// The pixel at (nx, ny) inside the unit disk is the point on the unit sphere
// facing the viewer, and its normal has nz = sqrt(1 - nx^2 - ny^2). Since
// cos(theta) = nz, that is a lambert term, and it goes through the same
// RETRO_ShadeFromLambert conversion a renderer applies before picking a shade
//
// Pixel (x, y) of the unit disk is the front of the unit sphere:
//
//   (nx, ny) = ((x - cx)/cx, (y - cy)/cy)
//   nz = sqrt(1 - nx^2 - ny^2)
//   shade = ShadeFromLambert(nz)
//
// Outside the disk the darkest material shade is kept, so a grazing lookup
// never punches a black hole.
void RETRO_CreatePhongMap(unsigned char *buffer, int width, int height)
{
	float centerx = (width - 1) * 0.5f;
	float centery = (height - 1) * 0.5f;

	for (int y = 0; y < height; y++) {
		float ny = (y - centery) / centery;
		for (int x = 0; x < width; x++) {
			float nx = (x - centerx) / centerx;
			float radiussquared = nx * nx + ny * ny;

			// The environment lookup stores the lighting of the front-facing
			// hemisphere. Outside its unit disk, retain the darkest material
			// shade rather than introducing transparent-looking black holes.
			int paletteindex = RETRO_PHONG_OFFSET;
			if (radiussquared <= 1.0f) {
				float nz = sqrt(1.0f - radiussquared);
				paletteindex += RETRO_ShadeFromLambert(nz) * RETRO_PHONG_SHADES;
			}
			// A pixel landing dead center on an odd-sized map has nz of exactly
			// 1, one past the last shade, so the index still needs clamping
			buffer[y * width + x] = MIN(paletteindex, RETRO_COLORS - 1);
		}
	}
}

//
// A ball sprite: the front of a lit sphere, on a ramp of its own.
//
// The disk is the hemisphere facing the viewer, shaded by N·L into ramp
// entries [color, color + shades), and everything outside it is entry 0,
// which the sprite drawers read as transparent. RETRO_CreatePhongMap fills
// that outside with the darkest material shade instead, because a lookup has
// to answer everywhere it is asked; a sprite has to stop at its own edge.
//
// depthmap, if it is given, takes the front hemisphere itself: nz over the
// disk and zero outside it. That is what RETRO_DrawDepthSprite reads to write
// each pixel at the depth of the sphere's surface, with the ball's radius as
// its thickness.
//
// The light needs no normalizing by the caller. Several of these on ramps of
// their own, and one map per ramp, is how a demo dims a ball by depth without
// touching its shading.
//
void RETRO_CreateBallMap(unsigned char *buffer, float *depthmap, int size, int color, int shades, float lightx = -0.4f, float lighty = -0.4f, float lightz = 0.82f)
{
	float centre = (size - 1) * 0.5f;
	float length = sqrt(lightx * lightx + lighty * lighty + lightz * lightz);

	for (int y = 0; y < size; y++) {
		float ny = (y - centre) / centre;
		for (int x = 0; x < size; x++) {
			float nx = (x - centre) / centre;
			float radiussquared = nx * nx + ny * ny;

			int paletteindex = 0;
			float nz = 0.0f;
			if (radiussquared <= 1.0f) {
				nz = sqrt(1.0f - radiussquared);
				float lambert = (nx * lightx + ny * lighty + nz * lightz) / length;
				paletteindex = color + MIN((int)(RETRO_ShadeFromLambert(lambert) * shades), shades - 1);
			}
			buffer[y * size + x] = MIN(paletteindex, RETRO_COLORS - 1);
			if (depthmap) depthmap[y * size + x] = nz;
		}
	}
}

//
// The palette the VGA BIOS leaves in the DAC in mode 13h: the 16 EGA colors,
// 16 grays, then 216 entries walking hue, saturation and value, and 8 unused
// entries left black. A demo that draws with the colors it finds there, rather
// than setting a palette of its own, is drawing against this
//
RETRO_Palette RETRO_Default8bitPalette[256] = {
	{ 0, 0, 0 },
	{ 0, 0, 170 },
	{ 0, 170, 0 },
	{ 0, 170, 170 },
	{ 170, 0, 0 },
	{ 170, 0, 170 },
	{ 170, 85, 0 },
	{ 170, 170, 170 },
	{ 85, 85, 85 },
	{ 85, 85, 255 },
	{ 85, 255, 85 },
	{ 85, 255, 255 },
	{ 255, 85, 85 },
	{ 255, 85, 255 },
	{ 255, 255, 85 },
	{ 255, 255, 255 },
	{ 0, 0, 0 },
	{ 20, 20, 20 },
	{ 32, 32, 32 },
	{ 44, 44, 44 },
	{ 56, 56, 56 },
	{ 69, 69, 69 },
	{ 81, 81, 81 },
	{ 97, 97, 97 },
	{ 113, 113, 113 },
	{ 130, 130, 130 },
	{ 146, 146, 146 },
	{ 162, 162, 162 },
	{ 182, 182, 182 },
	{ 203, 203, 203 },
	{ 227, 227, 227 },
	{ 255, 255, 255 },
	{ 0, 0, 255 },
	{ 65, 0, 255 },
	{ 125, 0, 255 },
	{ 190, 0, 255 },
	{ 255, 0, 255 },
	{ 255, 0, 190 },
	{ 255, 0, 125 },
	{ 255, 0, 65 },
	{ 255, 0, 0 },
	{ 255, 65, 0 },
	{ 255, 125, 0 },
	{ 255, 190, 0 },
	{ 255, 255, 0 },
	{ 190, 255, 0 },
	{ 125, 255, 0 },
	{ 65, 255, 0 },
	{ 0, 255, 0 },
	{ 0, 255, 65 },
	{ 0, 255, 125 },
	{ 0, 255, 190 },
	{ 0, 255, 255 },
	{ 0, 190, 255 },
	{ 0, 125, 255 },
	{ 0, 65, 255 },
	{ 125, 125, 255 },
	{ 158, 125, 255 },
	{ 190, 125, 255 },
	{ 223, 125, 255 },
	{ 255, 125, 255 },
	{ 255, 125, 223 },
	{ 255, 125, 190 },
	{ 255, 125, 158 },
	{ 255, 125, 125 },
	{ 255, 158, 125 },
	{ 255, 190, 125 },
	{ 255, 223, 125 },
	{ 255, 255, 125 },
	{ 223, 255, 125 },
	{ 190, 255, 125 },
	{ 158, 255, 125 },
	{ 125, 255, 125 },
	{ 125, 255, 158 },
	{ 125, 255, 190 },
	{ 125, 255, 223 },
	{ 125, 255, 255 },
	{ 125, 223, 255 },
	{ 125, 190, 255 },
	{ 125, 158, 255 },
	{ 182, 182, 255 },
	{ 199, 182, 255 },
	{ 219, 182, 255 },
	{ 235, 182, 255 },
	{ 255, 182, 255 },
	{ 255, 182, 235 },
	{ 255, 182, 219 },
	{ 255, 182, 199 },
	{ 255, 182, 182 },
	{ 255, 199, 182 },
	{ 255, 219, 182 },
	{ 255, 235, 182 },
	{ 255, 255, 182 },
	{ 235, 255, 182 },
	{ 219, 255, 182 },
	{ 199, 255, 182 },
	{ 182, 255, 182 },
	{ 182, 255, 199 },
	{ 182, 255, 219 },
	{ 182, 255, 235 },
	{ 182, 255, 255 },
	{ 182, 235, 255 },
	{ 182, 219, 255 },
	{ 182, 199, 255 },
	{ 0, 0, 113 },
	{ 28, 0, 113 },
	{ 56, 0, 113 },
	{ 85, 0, 113 },
	{ 113, 0, 113 },
	{ 113, 0, 85 },
	{ 113, 0, 56 },
	{ 113, 0, 28 },
	{ 113, 0, 0 },
	{ 113, 28, 0 },
	{ 113, 56, 0 },
	{ 113, 85, 0 },
	{ 113, 113, 0 },
	{ 85, 113, 0 },
	{ 56, 113, 0 },
	{ 28, 113, 0 },
	{ 0, 113, 0 },
	{ 0, 113, 28 },
	{ 0, 113, 56 },
	{ 0, 113, 85 },
	{ 0, 113, 113 },
	{ 0, 85, 113 },
	{ 0, 56, 113 },
	{ 0, 28, 113 },
	{ 56, 56, 113 },
	{ 69, 56, 113 },
	{ 85, 56, 113 },
	{ 97, 56, 113 },
	{ 113, 56, 113 },
	{ 113, 56, 97 },
	{ 113, 56, 85 },
	{ 113, 56, 69 },
	{ 113, 56, 56 },
	{ 113, 69, 56 },
	{ 113, 85, 56 },
	{ 113, 97, 56 },
	{ 113, 113, 56 },
	{ 97, 113, 56 },
	{ 85, 113, 56 },
	{ 69, 113, 56 },
	{ 56, 113, 56 },
	{ 56, 113, 69 },
	{ 56, 113, 85 },
	{ 56, 113, 97 },
	{ 56, 113, 113 },
	{ 56, 97, 113 },
	{ 56, 85, 113 },
	{ 56, 69, 113 },
	{ 81, 81, 113 },
	{ 89, 81, 113 },
	{ 97, 81, 113 },
	{ 105, 81, 113 },
	{ 113, 81, 113 },
	{ 113, 81, 105 },
	{ 113, 81, 97 },
	{ 113, 81, 89 },
	{ 113, 81, 81 },
	{ 113, 89, 81 },
	{ 113, 97, 81 },
	{ 113, 105, 81 },
	{ 113, 113, 81 },
	{ 105, 113, 81 },
	{ 97, 113, 81 },
	{ 89, 113, 81 },
	{ 81, 113, 81 },
	{ 81, 113, 89 },
	{ 81, 113, 97 },
	{ 81, 113, 105 },
	{ 81, 113, 113 },
	{ 81, 105, 113 },
	{ 81, 97, 113 },
	{ 81, 89, 113 },
	{ 0, 0, 65 },
	{ 16, 0, 65 },
	{ 32, 0, 65 },
	{ 48, 0, 65 },
	{ 65, 0, 65 },
	{ 65, 0, 48 },
	{ 65, 0, 32 },
	{ 65, 0, 16 },
	{ 65, 0, 0 },
	{ 65, 16, 0 },
	{ 65, 32, 0 },
	{ 65, 48, 0 },
	{ 65, 65, 0 },
	{ 48, 65, 0 },
	{ 32, 65, 0 },
	{ 16, 65, 0 },
	{ 0, 65, 0 },
	{ 0, 65, 16 },
	{ 0, 65, 32 },
	{ 0, 65, 48 },
	{ 0, 65, 65 },
	{ 0, 48, 65 },
	{ 0, 32, 65 },
	{ 0, 16, 65 },
	{ 32, 32, 65 },
	{ 40, 32, 65 },
	{ 48, 32, 65 },
	{ 56, 32, 65 },
	{ 65, 32, 65 },
	{ 65, 32, 56 },
	{ 65, 32, 48 },
	{ 65, 32, 40 },
	{ 65, 32, 32 },
	{ 65, 40, 32 },
	{ 65, 48, 32 },
	{ 65, 56, 32 },
	{ 65, 65, 32 },
	{ 56, 65, 32 },
	{ 48, 65, 32 },
	{ 40, 65, 32 },
	{ 32, 65, 32 },
	{ 32, 65, 40 },
	{ 32, 65, 48 },
	{ 32, 65, 56 },
	{ 32, 65, 65 },
	{ 32, 56, 65 },
	{ 32, 48, 65 },
	{ 32, 40, 65 },
	{ 44, 44, 65 },
	{ 48, 44, 65 },
	{ 52, 44, 65 },
	{ 60, 44, 65 },
	{ 65, 44, 65 },
	{ 65, 44, 60 },
	{ 65, 44, 52 },
	{ 65, 44, 48 },
	{ 65, 44, 44 },
	{ 65, 48, 44 },
	{ 65, 52, 44 },
	{ 65, 60, 44 },
	{ 65, 65, 44 },
	{ 60, 65, 44 },
	{ 52, 65, 44 },
	{ 48, 65, 44 },
	{ 44, 65, 44 },
	{ 44, 65, 48 },
	{ 44, 65, 52 },
	{ 44, 65, 60 },
	{ 44, 65, 65 },
	{ 44, 60, 65 },
	{ 44, 52, 65 },
	{ 44, 48, 65 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 }
};

//
// The same palette on the 6-bit scale the VGA DAC works in, for
// RETRO_Set6bitPalette
//
RETRO_Palette RETRO_Default6bitPalette[256] = {
	{ 0, 0, 0 },
	{ 0, 0, 42 },
	{ 0, 42, 0 },
	{ 0, 42, 42 },
	{ 42, 0, 0 },
	{ 42, 0, 42 },
	{ 42, 21, 0 },
	{ 42, 42, 42 },
	{ 21, 21, 21 },
	{ 21, 21, 63 },
	{ 21, 63, 21 },
	{ 21, 63, 63 },
	{ 63, 21, 21 },
	{ 63, 21, 63 },
	{ 63, 63, 21 },
	{ 63, 63, 63 },
	{ 0, 0, 0 },
	{ 5, 5, 5 },
	{ 8, 8, 8 },
	{ 11, 11, 11 },
	{ 14, 14, 14 },
	{ 17, 17, 17 },
	{ 20, 20, 20 },
	{ 24, 24, 24 },
	{ 28, 28, 28 },
	{ 32, 32, 32 },
	{ 36, 36, 36 },
	{ 40, 40, 40 },
	{ 45, 45, 45 },
	{ 50, 50, 50 },
	{ 56, 56, 56 },
	{ 63, 63, 63 },
	{ 0, 0, 63 },
	{ 16, 0, 63 },
	{ 31, 0, 63 },
	{ 47, 0, 63 },
	{ 63, 0, 63 },
	{ 63, 0, 47 },
	{ 63, 0, 31 },
	{ 63, 0, 16 },
	{ 63, 0, 0 },
	{ 63, 16, 0 },
	{ 63, 31, 0 },
	{ 63, 47, 0 },
	{ 63, 63, 0 },
	{ 47, 63, 0 },
	{ 31, 63, 0 },
	{ 16, 63, 0 },
	{ 0, 63, 0 },
	{ 0, 63, 16 },
	{ 0, 63, 31 },
	{ 0, 63, 47 },
	{ 0, 63, 63 },
	{ 0, 47, 63 },
	{ 0, 31, 63 },
	{ 0, 16, 63 },
	{ 31, 31, 63 },
	{ 39, 31, 63 },
	{ 47, 31, 63 },
	{ 55, 31, 63 },
	{ 63, 31, 63 },
	{ 63, 31, 55 },
	{ 63, 31, 47 },
	{ 63, 31, 39 },
	{ 63, 31, 31 },
	{ 63, 39, 31 },
	{ 63, 47, 31 },
	{ 63, 55, 31 },
	{ 63, 63, 31 },
	{ 55, 63, 31 },
	{ 47, 63, 31 },
	{ 39, 63, 31 },
	{ 31, 63, 31 },
	{ 31, 63, 39 },
	{ 31, 63, 47 },
	{ 31, 63, 55 },
	{ 31, 63, 63 },
	{ 31, 55, 63 },
	{ 31, 47, 63 },
	{ 31, 39, 63 },
	{ 45, 45, 63 },
	{ 49, 45, 63 },
	{ 54, 45, 63 },
	{ 58, 45, 63 },
	{ 63, 45, 63 },
	{ 63, 45, 58 },
	{ 63, 45, 54 },
	{ 63, 45, 49 },
	{ 63, 45, 45 },
	{ 63, 49, 45 },
	{ 63, 54, 45 },
	{ 63, 58, 45 },
	{ 63, 63, 45 },
	{ 58, 63, 45 },
	{ 54, 63, 45 },
	{ 49, 63, 45 },
	{ 45, 63, 45 },
	{ 45, 63, 49 },
	{ 45, 63, 54 },
	{ 45, 63, 58 },
	{ 45, 63, 63 },
	{ 45, 58, 63 },
	{ 45, 54, 63 },
	{ 45, 49, 63 },
	{ 0, 0, 28 },
	{ 7, 0, 28 },
	{ 14, 0, 28 },
	{ 21, 0, 28 },
	{ 28, 0, 28 },
	{ 28, 0, 21 },
	{ 28, 0, 14 },
	{ 28, 0, 7 },
	{ 28, 0, 0 },
	{ 28, 7, 0 },
	{ 28, 14, 0 },
	{ 28, 21, 0 },
	{ 28, 28, 0 },
	{ 21, 28, 0 },
	{ 14, 28, 0 },
	{ 7, 28, 0 },
	{ 0, 28, 0 },
	{ 0, 28, 7 },
	{ 0, 28, 14 },
	{ 0, 28, 21 },
	{ 0, 28, 28 },
	{ 0, 21, 28 },
	{ 0, 14, 28 },
	{ 0, 7, 28 },
	{ 14, 14, 28 },
	{ 17, 14, 28 },
	{ 21, 14, 28 },
	{ 24, 14, 28 },
	{ 28, 14, 28 },
	{ 28, 14, 24 },
	{ 28, 14, 21 },
	{ 28, 14, 17 },
	{ 28, 14, 14 },
	{ 28, 17, 14 },
	{ 28, 21, 14 },
	{ 28, 24, 14 },
	{ 28, 28, 14 },
	{ 24, 28, 14 },
	{ 21, 28, 14 },
	{ 17, 28, 14 },
	{ 14, 28, 14 },
	{ 14, 28, 17 },
	{ 14, 28, 21 },
	{ 14, 28, 24 },
	{ 14, 28, 28 },
	{ 14, 24, 28 },
	{ 14, 21, 28 },
	{ 14, 17, 28 },
	{ 20, 20, 28 },
	{ 22, 20, 28 },
	{ 24, 20, 28 },
	{ 26, 20, 28 },
	{ 28, 20, 28 },
	{ 28, 20, 26 },
	{ 28, 20, 24 },
	{ 28, 20, 22 },
	{ 28, 20, 20 },
	{ 28, 22, 20 },
	{ 28, 24, 20 },
	{ 28, 26, 20 },
	{ 28, 28, 20 },
	{ 26, 28, 20 },
	{ 24, 28, 20 },
	{ 22, 28, 20 },
	{ 20, 28, 20 },
	{ 20, 28, 22 },
	{ 20, 28, 24 },
	{ 20, 28, 26 },
	{ 20, 28, 28 },
	{ 20, 26, 28 },
	{ 20, 24, 28 },
	{ 20, 22, 28 },
	{ 0, 0, 16 },
	{ 4, 0, 16 },
	{ 8, 0, 16 },
	{ 12, 0, 16 },
	{ 16, 0, 16 },
	{ 16, 0, 12 },
	{ 16, 0, 8 },
	{ 16, 0, 4 },
	{ 16, 0, 0 },
	{ 16, 4, 0 },
	{ 16, 8, 0 },
	{ 16, 12, 0 },
	{ 16, 16, 0 },
	{ 12, 16, 0 },
	{ 8, 16, 0 },
	{ 4, 16, 0 },
	{ 0, 16, 0 },
	{ 0, 16, 4 },
	{ 0, 16, 8 },
	{ 0, 16, 12 },
	{ 0, 16, 16 },
	{ 0, 12, 16 },
	{ 0, 8, 16 },
	{ 0, 4, 16 },
	{ 8, 8, 16 },
	{ 10, 8, 16 },
	{ 12, 8, 16 },
	{ 14, 8, 16 },
	{ 16, 8, 16 },
	{ 16, 8, 14 },
	{ 16, 8, 12 },
	{ 16, 8, 10 },
	{ 16, 8, 8 },
	{ 16, 10, 8 },
	{ 16, 12, 8 },
	{ 16, 14, 8 },
	{ 16, 16, 8 },
	{ 14, 16, 8 },
	{ 12, 16, 8 },
	{ 10, 16, 8 },
	{ 8, 16, 8 },
	{ 8, 16, 10 },
	{ 8, 16, 12 },
	{ 8, 16, 14 },
	{ 8, 16, 16 },
	{ 8, 14, 16 },
	{ 8, 12, 16 },
	{ 8, 10, 16 },
	{ 11, 11, 16 },
	{ 12, 11, 16 },
	{ 13, 11, 16 },
	{ 15, 11, 16 },
	{ 16, 11, 16 },
	{ 16, 11, 15 },
	{ 16, 11, 13 },
	{ 16, 11, 12 },
	{ 16, 11, 11 },
	{ 16, 12, 11 },
	{ 16, 13, 11 },
	{ 16, 15, 11 },
	{ 16, 16, 11 },
	{ 15, 16, 11 },
	{ 13, 16, 11 },
	{ 12, 16, 11 },
	{ 11, 16, 11 },
	{ 11, 16, 12 },
	{ 11, 16, 13 },
	{ 11, 16, 15 },
	{ 11, 16, 16 },
	{ 11, 15, 16 },
	{ 11, 13, 16 },
	{ 11, 12, 16 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 }
};

#endif
