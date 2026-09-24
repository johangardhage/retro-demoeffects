//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROPALETTE_H_
#define _RETROPALETTE_H_

#include "retro.h"
#include "retrovector.h"

// *******************************************************************
// Public variables
// *******************************************************************

// Named colors, with 8-bit components as used by RETRO_SetPalette. A demo can
// name its own colors the same way, for example #define EMBER RETRO_RGB(0x140014)

#define RETRO_RGB(hex) RETRO_Palette{ ((hex) >> 16) & 0xff, ((hex) >> 8) & 0xff, (hex) & 0xff }

// Whites, grays and blacks
#define RETRO_WHITE RETRO_Palette{ 255, 255, 255 }
#define RETRO_IVORY RETRO_Palette{ 255, 255, 240 }
#define RETRO_SOFTGHOSTWHITE RETRO_Palette{ 250, 250, 255 }
#define RETRO_GHOSTWHITE RETRO_Palette{ 248, 248, 255 }
#define RETRO_SOFTIVORY RETRO_Palette{ 248, 249, 240 }
#define RETRO_LIGHTGRAY RETRO_Palette{ 200, 200, 200 }
#define RETRO_SILVER RETRO_Palette{ 188, 188, 188 }
#define RETRO_STONE RETRO_Palette{ 189, 187, 160 }
#define RETRO_HAZE RETRO_Palette{ 168, 188, 196 }
#define RETRO_SAGE RETRO_Palette{ 164, 164, 140 }
#define RETRO_LIGHTSLATEGRAY RETRO_Palette{ 142, 137, 159 }
#define RETRO_TEALGRAY RETRO_Palette{ 112, 142, 139 }
#define RETRO_OLIVEGRAY RETRO_Palette{ 131, 133, 96 }
#define RETRO_GRAY RETRO_Palette{ 128, 128, 128 }
#define RETRO_SLATEGRAY RETRO_Palette{ 112, 128, 144 }
#define RETRO_ASHGRAY RETRO_Palette{ 72, 72, 72 }
#define RETRO_DIMGRAY RETRO_Palette{ 66, 66, 74 }
#define RETRO_MUTEDDARKSLATEBLUE RETRO_Palette{ 65, 61, 88 }
#define RETRO_UMBER RETRO_Palette{ 61, 58, 48 }
#define RETRO_CHARCOAL RETRO_Palette{ 54, 54, 60 }
#define RETRO_SOOT RETRO_Palette{ 52, 52, 52 }
#define RETRO_GRAPHITE RETRO_Palette{ 40, 40, 40 }
#define RETRO_JET RETRO_Palette{ 8, 8, 8 }
#define RETRO_ONYX RETRO_Palette{ 1, 0, 16 }
#define RETRO_BLACK RETRO_Palette{ 0, 0, 0 }

// Reds
#define RETRO_LIGHTRED RETRO_Palette{ 255, 204, 204 }
#define RETRO_DARKSALMON RETRO_Palette{ 211, 146, 132 }
#define RETRO_SCARLET RETRO_Palette{ 255, 36, 0 }
#define RETRO_FIREBRICK RETRO_Palette{ 187, 40, 32 }
#define RETRO_RED RETRO_Palette{ 255, 0, 0 }
#define RETRO_BRICK RETRO_Palette{ 116, 48, 52 }
#define RETRO_MEDIUMRED RETRO_Palette{ 187, 0, 0 }
#define RETRO_DARKRED RETRO_Palette{ 128, 0, 0 }
#define RETRO_EMBERBLACK RETRO_Palette{ 69, 5, 0 }

// Oranges and browns
#define RETRO_JASMINE RETRO_Palette{ 252, 212, 120 }
#define RETRO_TAN RETRO_Palette{ 210, 180, 140 }
#define RETRO_SAFFRON RETRO_Palette{ 252, 168, 56 }
#define RETRO_CARROT RETRO_Palette{ 240, 132, 20 }
#define RETRO_ORANGE RETRO_Palette{ 255, 128, 0 }
#define RETRO_CHOCOLATE RETRO_Palette{ 191, 119, 43 }
#define RETRO_PERU RETRO_Palette{ 185, 114, 49 }
#define RETRO_FAWN RETRO_Palette{ 143, 122, 78 }
#define RETRO_SIENNA RETRO_Palette{ 180, 80, 44 }
#define RETRO_SADDLEBROWN RETRO_Palette{ 139, 69, 19 }
#define RETRO_SCORCHED RETRO_Palette{ 80, 24, 0 }

// Yellows and golds
#define RETRO_CREAM RETRO_Palette{ 255, 255, 142 }
#define RETRO_BLANCHEDALMOND RETRO_Palette{ 255, 240, 200 }
#define RETRO_ANTIQUEWHITE RETRO_Palette{ 242, 240, 209 }
#define RETRO_YELLOW RETRO_Palette{ 255, 255, 0 }
#define RETRO_KHAKI RETRO_Palette{ 222, 214, 149 }
#define RETRO_BURLYWOOD RETRO_Palette{ 216, 206, 119 }
#define RETRO_GOLD RETRO_Palette{ 254, 204, 0 }
#define RETRO_MARIGOLD RETRO_Palette{ 255, 192, 40 }
#define RETRO_AMBER RETRO_Palette{ 255, 191, 0 }
#define RETRO_DARKKHAKI RETRO_Palette{ 183, 173, 82 }
#define RETRO_BRASSGOLD RETRO_Palette{ 170, 160, 55 }
#define RETRO_DRABGOLD RETRO_Palette{ 130, 119, 34 }
#define RETRO_DUSTYGOLD RETRO_Palette{ 119, 115, 73 }
#define RETRO_DARKBRASS RETRO_Palette{ 98, 89, 8 }
#define RETRO_MUDGOLD RETRO_Palette{ 56, 53, 27 }

// Greens
#define RETRO_PALESAGE RETRO_Palette{ 150, 198, 150 }
#define RETRO_MOSSGREEN RETRO_Palette{ 148, 176, 70 }
#define RETRO_SPRINGGREEN RETRO_Palette{ 30, 230, 90 }
#define RETRO_GREEN RETRO_Palette{ 0, 255, 0 }
#define RETRO_MEDIUMSEAGREEN RETRO_Palette{ 60, 179, 113 }
#define RETRO_SEAGREEN RETRO_Palette{ 46, 139, 87 }
#define RETRO_FORESTGREEN RETRO_Palette{ 61, 122, 53 }
#define RETRO_DARKGREEN RETRO_Palette{ 0, 128, 0 }
#define RETRO_HUNTERGREEN RETRO_Palette{ 42, 86, 36 }
#define RETRO_PINETREE RETRO_Palette{ 30, 74, 40 }
#define RETRO_MOSSBLACK RETRO_Palette{ 0, 20, 0 }

// Cyans and teals
#define RETRO_PALESKYBLUE RETRO_Palette{ 128, 225, 239 }
#define RETRO_CYAN RETRO_Palette{ 0, 255, 255 }
#define RETRO_BRIGHTCYAN RETRO_Palette{ 0, 232, 255 }
#define RETRO_DARKTURQUOISE RETRO_Palette{ 48, 200, 224 }
#define RETRO_DARKCYAN RETRO_Palette{ 0, 139, 139 }
#define RETRO_TEAL RETRO_Palette{ 0, 128, 128 }
#define RETRO_DARKSLATEGRAY RETRO_Palette{ 47, 79, 79 }

// Blues
#define RETRO_LAVENDER RETRO_Palette{ 208, 224, 255 }
#define RETRO_LIGHTSKYBLUE RETRO_Palette{ 135, 206, 250 }
#define RETRO_SKYBLUE RETRO_Palette{ 135, 206, 235 }
#define RETRO_PERIWINKLE RETRO_Palette{ 160, 168, 252 }
#define RETRO_CORNFLOWERBLUE RETRO_Palette{ 128, 160, 255 }
#define RETRO_LIGHTBLUE RETRO_Palette{ 102, 170, 255 }
#define RETRO_GLAUCOUS RETRO_Palette{ 92, 124, 160 }
#define RETRO_STEELBLUE RETRO_Palette{ 70, 130, 180 }
#define RETRO_AZURE RETRO_Palette{ 0, 128, 255 }
#define RETRO_SEABLUE RETRO_Palette{ 27, 123, 166 }
#define RETRO_DEEPSEABLUE RETRO_Palette{ 21, 120, 166 }
#define RETRO_OCEANBLUE RETRO_Palette{ 48, 80, 192 }
#define RETRO_CERULEAN RETRO_Palette{ 0, 106, 167 }
#define RETRO_DARKSLATEBLUE RETRO_Palette{ 72, 61, 139 }
#define RETRO_BRIGHTBLUE RETRO_Palette{ 18, 39, 255 }
#define RETRO_SPACECADET RETRO_Palette{ 28, 40, 88 }
#define RETRO_DEEPCERULEAN RETRO_Palette{ 14, 36, 101 }
#define RETRO_MIDNIGHTBLUE RETRO_Palette{ 25, 25, 112 }
#define RETRO_DARKMIDNIGHTBLUE RETRO_Palette{ 14, 36, 54 }
#define RETRO_BLUE RETRO_Palette{ 0, 0, 255 }
#define RETRO_NIGHTSKY RETRO_Palette{ 20, 24, 42 }
#define RETRO_NAVY RETRO_Palette{ 0, 19, 85 }
#define RETRO_DARKBLUE RETRO_Palette{ 0, 0, 128 }
#define RETRO_BLUEBLACK RETRO_Palette{ 0, 0, 48 }

// Purples and violets
#define RETRO_LILAC RETRO_Palette{ 176, 144, 255 }
#define RETRO_REBECCAPURPLE RETRO_Palette{ 102, 51, 153 }
#define RETRO_DEEPDARKVIOLET RETRO_Palette{ 120, 20, 196 }
#define RETRO_VIOLET RETRO_Palette{ 148, 0, 211 }
#define RETRO_PURPLE RETRO_Palette{ 128, 0, 255 }
#define RETRO_ROYALVIOLET RETRO_Palette{ 103, 23, 171 }
#define RETRO_TWILIGHT RETRO_Palette{ 64, 32, 160 }
#define RETRO_MUTEDINDIGO RETRO_Palette{ 55, 36, 68 }
#define RETRO_ROYALINDIGO RETRO_Palette{ 58, 26, 110 }
#define RETRO_INDIGO RETRO_Palette{ 75, 0, 130 }
#define RETRO_INDIGOBLACK RETRO_Palette{ 15, 3, 40 }
#define RETRO_DARKINDIGO RETRO_Palette{ 8, 0, 40 }

// Pinks and magentas
#define RETRO_PINKLACE RETRO_Palette{ 252, 212, 252 }
#define RETRO_PINK RETRO_Palette{ 255, 128, 192 }
#define RETRO_HOTPINK RETRO_Palette{ 255, 105, 180 }
#define RETRO_LIGHTMAGENTA RETRO_Palette{ 255, 66, 247 }
#define RETRO_ROSE RETRO_Palette{ 172, 92, 132 }
#define RETRO_DEEPPINK RETRO_Palette{ 219, 59, 150 }
#define RETRO_MAGENTA RETRO_Palette{ 255, 0, 255 }
#define RETRO_DARKMAGENTA RETRO_Palette{ 139, 0, 139 }
#define RETRO_WINE RETRO_Palette{ 44, 24, 36 }

// A phong palette keeps entry 0 black and ramps the material over the rest, so a
// renderer shades from RETRO_PHONG_OFFSET across RETRO_PHONG_SHADES entries
#define RETRO_PHONG_OFFSET 1
#define RETRO_PHONG_SHADES (RETRO_COLORS - RETRO_PHONG_OFFSET)

// The highlight a material takes unless it asks for another: how bright it is,
// and how tightly it gathers around face on
#define RETRO_K_SPECULAR 0.7
#define RETRO_K_FALLOFF 150

// *******************************************************************
// Private variables
// *******************************************************************

// The rest of the phong reflection model's coefficients
#define RETRO_K_AMBIENT 0.2
#define RETRO_K_DIFFUSE 0.9
#define RETRO_K_ATTENUATION 1.0

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
// The angle of incidence a shade stands for. A renderer picks a shade as
// floor(RETRO_ShadeFractionFromLambert(N·L) * shades), so shade s is the one
// taken for every theta in the step
//
//   ((shades − (shade + 1)) / shades * (π / 2), (shades − shade) / shades * (π / 2)]
//
// and it is built for the middle of that step, not for either end of it:
//
//   theta = (shades − (shade + 0.5)) / shades * (π / 2)
//
// Built for the step's face-on end instead, every surface would come out up
// to a whole shade brighter than its lighting, and half a shade on average
//
// Shades are spaced evenly in the angle, not in cos(theta). Spacing them in
// cos(theta) would look like the obvious choice, since a renderer finds a
// surface's lighting from the dot product of its normal with the light, but
// cos(theta) is flat near theta = 0 and would crowd almost no shades into the
// small angles where the specular highlight lives. At a falloff of 30 the
// bright core of the highlight gets 18 shades this way and only 2 the other.
// A renderer therefore has to convert its dot product with
// RETRO_ShadeFractionFromLambert before using it to pick a shade
//
inline float RETRO_IncidenceAngle(int shade, int shades)
{
	return ((shades - (shade + 0.5f)) / shades) * (M_PI / 2);
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
// highlight is gone, which is the usual max(R . V, 0) clamp
//
// Only the diffuse and ambient terms are tinted by the face color. The
// highlight stays the color of the light, which is what makes a material read
// as plastic rather than metal. A specularity of 0 drops the highlight and
// leaves plain lambert diffuse, which is the matte end of the same model
//
inline float RETRO_PhongIntensity(float facecolor, float lightcolor, float ambientcolor, float theta, float specularity, float falloff)
{
	float ndotl = cos(theta);
	float rdotv = 2.0f * ndotl * ndotl - 1.0f;

	float diffuse = RETRO_K_DIFFUSE * facecolor * MAX(ndotl, 0.0f);
	float specular = specularity * pow(MAX(rdotv, 0.0f), falloff);
	float ambient = ambientcolor * RETRO_K_AMBIENT * facecolor;

	float intensity = (diffuse + specular) * RETRO_K_ATTENUATION * lightcolor + ambient;

	return CLAMP01(intensity);
}

//
// A face color lit at the angle theta, every channel by the phong reflection
// model under the light's own color. face is in intensities between 0.0 and
// 1.0 and the result is scaled to colormax
//
inline RETRO_Palette RETRO_PhongColor(vec3 face, float theta, float specularity, float falloff, int colormax)
{
	RETRO_Palette color;
	color.r = colormax * RETRO_PhongIntensity(face.x, RETRO_LIGHT_R, RETRO_AMBIENT_R, theta, specularity, falloff);
	color.g = colormax * RETRO_PhongIntensity(face.y, RETRO_LIGHT_G, RETRO_AMBIENT_G, theta, specularity, falloff);
	color.b = colormax * RETRO_PhongIntensity(face.z, RETRO_LIGHT_B, RETRO_AMBIENT_B, theta, specularity, falloff);
	return color;
}

// *******************************************************************
// Public functions
// *******************************************************************

// The palette constructors below share two conventions. Given no palette they
// set the colors on screen, and given one they write it instead. And a
// constructor that takes colormax scales what it writes to that maximum, 255
// for an 8-bit palette or 63 for a 6-bit one

//
// The shade a surface takes, from the dot product of its normal with the light.
// Undoes the angle spacing that RETRO_IncidenceAngle lays down, so that a
// surface lands on the shade actually built for its lighting
//
//   N·L = cos(theta)  ->  1 - acos(N·L) / (pi / 2)  =  1 - theta / (pi / 2)
//
inline float RETRO_ShadeFractionFromLambert(float lambert)
{
	return 1.0f - acos(CLAMP01(lambert)) / (M_PI / 2);
}

//
// Shade one face color across the whole range of incidence, from black at a
// grazing angle up to the specular highlight face on. Components of the face
// color and of the ramp share the same scale, given by colormax, so the same
// call fills a 6-bit ramp as readily as an 8-bit one
//
inline void RETRO_CreatePhongRamp(RETRO_Palette *ramp, int shades, RETRO_Palette face, float specularity, float falloff, int colormax)
{
	vec3 intensity = {
		(float)(face.r / (double)colormax),
		(float)(face.g / (double)colormax),
		(float)(face.b / (double)colormax)
	};

	for (int shade = 0; shade < shades; shade++) {
		ramp[shade] = RETRO_PhongColor(intensity, RETRO_IncidenceAngle(shade, shades), specularity, falloff, colormax);
	}
}

//
// Lay one material's ramp over [start, end), the phong counterpart of
// RETRO_CreateGradientPalette. Index end is not written, so a palette can
// carry a material per range and a model can pick between them per face.
// There is no color at end: the last written entry is the face-on highlight.
// A last range with end = RETRO_COLORS therefore writes that highlight at 255
//
inline void RETRO_CreatePhongPalette(int start, int end, RETRO_Palette face, float specularity = RETRO_K_SPECULAR, float falloff = RETRO_K_FALLOFF, RETRO_Palette *palette = NULL, int colormax = 255)
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
// over the rest
//
inline void RETRO_CreateMaterialPalette(RETRO_Palette face, float specularity, float falloff, RETRO_Palette *palette, int colormax)
{
	RETRO_SetColor(0, RETRO_BLACK, palette);
	RETRO_CreatePhongPalette(RETRO_PHONG_OFFSET, RETRO_PHONG_OFFSET + RETRO_PHONG_SHADES, face, specularity, falloff, palette, colormax);
}

//
// Encode an amount of light for display, by the sRGB transfer function of
// IEC 61966-2-1. Lighting adds and multiplies amounts of light, so it has to
// be worked out in linear values, where 0.5 is half the light of 1.0. A
// palette entry is not linear: the display spends more of its steps on the
// darks, and entry 128 of 255 gives only about a fifth of the light of 255.
// Both sides are on [0, 1], and the input is clamped to it:
//
//   l ≤ 0.0031308:  s = 12.92 l
//   otherwise:      s = 1.055 l^(1 / 2.4) − 0.055
//
// The straight segment near black keeps the slope finite at zero, and the
// two pieces meet at the threshold. Overall the curve is close to l^(1 / 2.2)
//
inline float RETRO_LinearToSRGB(float linear)
{
	float l = CLAMP01(linear);
	return l <= 0.0031308f ? 12.92f * l : 1.055f * pow(l, 1.0f / 2.4f) - 0.055f;
}

//
// The inverse: the amount of light an encoded value stands for, such as a
// component of a color picked on screen divided by its maximum. Both sides
// are on [0, 1], and the input is clamped to it:
//
//   s ≤ 0.04045:  l = s / 12.92
//   otherwise:    l = ((s + 0.055) / 1.055)^2.4
//
// 0.04045 is 12.92 · 0.0031308, the same threshold on the encoded side
//
inline float RETRO_SRGBToLinear(float srgb)
{
	float s = CLAMP01(srgb);
	return s <= 0.04045f ? s / 12.92f : pow((s + 0.055f) / 1.055f, 2.4f);
}

//
// A palette color from linear light, each component encoded by
// RETRO_LinearToSRGB and rounded to colormax
//
inline RETRO_Palette RETRO_LinearToColor(vec3 color, int colormax = 255)
{
	return {
		(unsigned char)(colormax * RETRO_LinearToSRGB(color.x) + 0.5f),
		(unsigned char)(colormax * RETRO_LinearToSRGB(color.y) + 0.5f),
		(unsigned char)(colormax * RETRO_LinearToSRGB(color.z) + 0.5f)
	};
}

//
// Fill [start, end) with a linear interpolation from one color toward another.
// to is the color at end, which is not written, so the next ramp can start
// there and write the knot as its from without both touching it. A last ramp
// with end = RETRO_COLORS therefore leaves 255 one step short of to: that
// index is off the palette. That is the range, not a missing write. The
// components are interpolated on whatever scale they are given in, so there is
// no colormax
//
//   C(i) = from + ((i - start) / (end - start)) * (to - from),  i ∈ [start, end)
//
inline void RETRO_CreateGradientPalette(int start, int end, RETRO_Palette from, RETRO_Palette to, RETRO_Palette *palette = NULL)
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
// black up to a specular highlight. The highlight is the color of the light
// whatever the face color, which is what makes the material read as plastic,
// and a lower falloff spreads it over more of the palette
//
inline void RETRO_CreatePlasticPalette(RETRO_Palette face = RETRO_DEEPPINK, float falloff = RETRO_K_FALLOFF, RETRO_Palette *palette = NULL, int colormax = 255)
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
// highlight at once and blinks. Gouraud and phong interpolate a normal and
// can carry RETRO_CreatePlasticPalette instead
//
inline void RETRO_CreateMattePalette(RETRO_Palette face = RETRO_DEEPPINK, RETRO_Palette *palette = NULL, int colormax = 255)
{
	RETRO_CreateMaterialPalette(face, 0.0, RETRO_K_FALLOFF, palette, colormax);
}

//
// Fill a buffer with an environment map of the lighting on a sphere, as indices
// into a phong palette. The map is read with the screen space normal of a
// surface, so it stands in for the whole lighting calculation at render time
//
// Pixel (x, y) inside the unit disk is the point on the unit sphere facing the
// viewer. Since cos(theta) = nz, its normal's nz is a lambert term, and it goes
// through the same conversion a renderer applies before picking a shade:
//
//   (nx, ny) = ((x - cx)/cx, (y - cy)/cy)
//   nz = sqrt(1 - nx^2 - ny^2)
//   shade = RETRO_ShadeFractionFromLambert(nz) * RETRO_PHONG_SHADES
//
// Outside the disk the darkest material shade is kept rather than black, so a
// grazing lookup never punches a hole
//
inline void RETRO_CreatePhongMap(unsigned char *buffer, int width, int height)
{
	float centerx = (width - 1) * 0.5f;
	float centery = (height - 1) * 0.5f;

	for (int y = 0; y < height; y++) {
		float ny = (y - centery) / centery;
		for (int x = 0; x < width; x++) {
			float nx = (x - centerx) / centerx;
			float radiussquared = nx * nx + ny * ny;

			int paletteindex = RETRO_PHONG_OFFSET;
			if (radiussquared <= 1.0f) {
				float nz = sqrt(1.0f - radiussquared);
				paletteindex += RETRO_ShadeFractionFromLambert(nz) * RETRO_PHONG_SHADES;
			}
			// A pixel landing dead center on an odd-sized map has nz of exactly
			// 1, one past the last shade, so the index still needs clamping
			buffer[y * width + x] = MIN(paletteindex, RETRO_COLORS - 1);
		}
	}
}

//
// The same lighting baked the way DOS-era demos often did it, with the map
// and its palette as a pair. Unlike RETRO_CreatePhongMap, the grid is even
// in angle rather than in the normal, and the entries count down from
// brightest face on, steps of them to a quarter turn, with no black entry
// kept aside. The defaults span the whole palette, 255 face on down to 0 at
// grazing. A map that is to be read as shade levels rather than colors, a
// lighting map over a shade table, takes brightest one below the table's
// height and steps at the height, so it runs out at 0 just short of grazing
//
// Pixel (x, y) stands for a normal tilted by one angle along each axis, a
// quarter turn at the map's edge, and the angle between it and the view
// axis picks the entry:
//
//   (tx, ty) = ((x - cx)/cx, (y - cy)/cy) * π/2
//   nz = sqrt(1 - sin^2 tx - sin^2 ty)
//   alpha = acos(nz)
//   entry = max(brightest - floor(steps * alpha / (π/2)), 0)
//
// Where the two sines add up past 1 there is no such normal, and the entry is
// 0. That bends the rim of the disk in along the diagonals, so the map is not
// round
//
// alpha lands exactly on a step at some pixels (sin^2 0.1π + sin^2 0.3π =
// 3/4, so alpha = π/3), and there rounding decides which side it falls. The
// maps this reproduces were worked out in single precision in this order, and
// broke their ties the way it does; double precision, or the same sum in
// another order, sends a few of them the other way. A compiler that fuses
// the sum of squares into one operation differently could too
//
// It is not a drop-in for RETRO_CreatePhongMap. The environment lookup reads
// the texel at W/2 + radius * N, taking the offset to be the normal itself,
// which is how RETRO_CreatePhongMap lays the ball out. Read that way, this
// map shades a normal as though its angle were (π/2) * Nx along an axis
// rather than asin(Nx): 45 degrees where the surface is at 30, so the
// highlight comes out tighter and the falloff steeper than phong, and a
// radius short of the map's half width never reaches the grazing rim. That
// is the look it was made for, not the lighting
//
inline void RETRO_CreateAnglePhongMap(unsigned char *buffer, int width, int height, int brightest = RETRO_COLORS - 1, float steps = RETRO_COLORS - 1)
{
	const float quarter = (float)M_PI / 2;
	float centerx = (width - 1) * 0.5f;
	float centery = (height - 1) * 0.5f;

	for (int y = 0; y < height; y++) {
		float sy = sin((y - centery) / centery * quarter);
		for (int x = 0; x < width; x++) {
			float sx = sin((x - centerx) / centerx * quarter);
			float radiussquared = sx * sx + sy * sy;

			int paletteindex = 0;
			if (radiussquared < 1.0f) {
				float alpha = acos(sqrt(1.0f - radiussquared));
				paletteindex = MAX(brightest - (int)floor(alpha * steps / quarter), 0);
			}
			buffer[y * width + x] = paletteindex;
		}
	}
}

//
// The palette RETRO_CreateAnglePhongMap is read through: entry i lit by the
// phong model at the angle the map gives it, the face-on end of its step,
//
//   theta = (255 - i) / 255 * π/2
//
// face is the material's color as intensities between 0.0 and 1.0, finer than
// a palette color can hold
//
inline void RETRO_CreateAnglePhongPalette(vec3 face, float specularity = RETRO_K_SPECULAR, float falloff = RETRO_K_FALLOFF, RETRO_Palette *palette = NULL, int colormax = 255)
{
	for (int i = 0; i < RETRO_COLORS; i++) {
		float theta = (RETRO_COLORS - 1 - i) / (float)(RETRO_COLORS - 1) * (M_PI / 2);
		RETRO_SetColor(i, RETRO_PhongColor(face, theta, specularity, falloff, colormax), palette);
	}
}

//
// A ball sprite: the front of a lit sphere, on a ramp of its own
//
// The disk is the hemisphere facing the viewer, shaded by N·L into ramp
// entries [color, color + shades), and everything outside it is entry 0,
// which the sprite drawers read as transparent. RETRO_CreatePhongMap fills
// that outside with the darkest material shade instead, because a lookup has
// to answer everywhere it is asked; a sprite has to stop at its own edge
//
// depthmap, if it is given, takes the front hemisphere itself: nz over the
// disk and zero outside it. That is what RETRO_DrawDepthSprite reads to write
// each pixel at the depth of the sphere's surface, with the ball's radius as
// its thickness
//
// The light needs no normalizing by the caller. Several of these on ramps of
// their own, and one map per ramp, is how a demo dims a ball by depth without
// touching its shading
//
inline void RETRO_CreateBallMap(unsigned char *buffer, float *depthmap, int size, int color, int shades, vec3 light = { -0.4f, -0.4f, 0.82f })
{
	float center = (size - 1) * 0.5f;
	float lightlength = length(light);

	for (int y = 0; y < size; y++) {
		float ny = (y - center) / center;
		for (int x = 0; x < size; x++) {
			float nx = (x - center) / center;
			float radiussquared = nx * nx + ny * ny;

			int paletteindex = 0;
			float nz = 0.0f;
			if (radiussquared <= 1.0f) {
				nz = sqrt(1.0f - radiussquared);
				float lambert = dot(vec3{ nx, ny, nz }, light) / lightlength;
				paletteindex = color + MIN((int)(RETRO_ShadeFractionFromLambert(lambert) * shades), shades - 1);
			}
			buffer[y * size + x] = MIN(paletteindex, RETRO_COLORS - 1);
			if (depthmap) depthmap[y * size + x] = nz;
		}
	}
}

//
// Nearest entry in an explicit palette, by squared RGB distance
//
inline unsigned char RETRO_NearestPaletteIndex(RETRO_Palette target, const RETRO_Palette *palette, int colors = RETRO_COLORS)
{
	int match = 0;
	int mindistance = 3 * 255 * 255 + 1;

	for (int color = 0; color < colors; color++) {
		int dr = (int)palette[color].r - target.r;
		int dg = (int)palette[color].g - target.g;
		int db = (int)palette[color].b - target.b;
		int distance = dr * dr + dg * dg + db * db;
		if (distance < mindistance) {
			mindistance = distance;
			match = color;
		}
	}

	return match;
}

//
// A cuberes^3 RGB cube, each cell mapped ahead of time to the nearest entry
// in an explicit palette. Several demos composite a true-color pixel and
// need it as a palette index every pixel; matching straight against
// RETRO_NearestPaletteIndex's linear scan there would cost a 256-entry scan
// per pixel, so they quantize into this cube once at startup instead. lut
// is flattened row-major (r * cuberes + g) * cuberes + b, the layout a
// caller's own unsigned char lut[cuberes][cuberes][cuberes] already has, so
// it can be passed as &lut[0][0][0]
//
inline void RETRO_CreateColorLUT(const RETRO_Palette *palette, int colors, int cuberes, unsigned char *lut)
{
	for (int r = 0; r < cuberes; r++) {
		for (int g = 0; g < cuberes; g++) {
			for (int b = 0; b < cuberes; b++) {
				RETRO_Palette target = {
					(unsigned char)(r * 255 / (cuberes - 1)),
					(unsigned char)(g * 255 / (cuberes - 1)),
					(unsigned char)(b * 255 / (cuberes - 1)),
				};
				lut[(r * cuberes + g) * cuberes + b] = RETRO_NearestPaletteIndex(target, palette, colors);
			}
		}
	}
}

//
// The palette the VGA BIOS leaves in the DAC in mode 13h: the 16 EGA colors,
// 16 grays, then 216 entries walking hue, saturation and value, and 8 unused
// entries left black. A demo that draws with the colors it finds there, rather
// than setting a palette of its own, is drawing against this
//
static const RETRO_Palette RETRO_Default8bitPalette[256] = {
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

#endif
