//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROSHADETABLE_H_
#define _RETROSHADETABLE_H_

#include "retropalette.h"

// *******************************************************************
// Public variables
// *******************************************************************

// Lighting every texture color at every shade gives the shaded colors, so a
// texture mapper can look up shadetable[texel * RETRO_SHADE_TABLE_SHADES + shade]
#define RETRO_SHADE_TABLE_COLORS 32
#define RETRO_SHADE_TABLE_SHADES 128
#define RETRO_SHADE_TABLE_SIZE (RETRO_SHADE_TABLE_COLORS * RETRO_SHADE_TABLE_SHADES)

// Light levels a shade table may carry beside its shade, one per coloured
// light lifting a channel of its own
#define RETRO_MAX_TINTS 2

// A shade table and the shape it was read at. The lookup is
// table[color * shades + shade], which is what RETRO_CreatePhongShadeTable and
// RETRO_CreateShadeTable both write. The two dimensions travel with the
// pointer because they are not the same for every texture: one drawn from a
// palette built for shading has few colors and a long ramp, one that is a
// picture in its own palette has all of them and a short ramp, and a table read
// at the wrong shape is read at the wrong stride.
//
// A table may carry further lights across it, for light that is not the same
// in every channel: a coloured lamp beside a white one lifts one channel
// further than the rest, and one shade per texel cannot say how far. Each
// such light is a tint, with tints[i] entries, and each shade holds all of
// them in turn, the last varying fastest. With one tint the lookup is
// table[(color * shades + shade) * tints[0] + tint[0]], and every further tint
// multiplies in the same way. A drawer that takes a table reads as many of
// PolygonPoint::tint as the table has tints beside c. The tints end at the
// first with no entries, so a table without any, the default, is the plain
// table, and no tint is read.
struct RETRO_ShadeTable {
	unsigned char *table;	// Rows of shades, one row per texture color
	int colors;				// Texture colors the table has a row for
	int shades;				// Entries in each row
	int tints[RETRO_MAX_TINTS] = {};	// Entries along each tint, up to the first with none
};

// *******************************************************************
// Private functions
// *******************************************************************

//
// The level of one of a table's steps, a shade or a tint's, as a fraction
// from 0 at the first to 1 at the last. With a single step there is only
// the first, so a table of one shade is its floor
//
inline float RETRO_ShadeTableLevel(int step, int steps)
{
	return steps > 1 ? (float)step / (steps - 1) : 0;
}

//
// How many tint entries each shade of a table holds: every tint's levels
// multiplied, up to the first tint with none
//
inline int RETRO_ShadeTableTintEntries(const RETRO_ShadeTable &shadetable)
{
	int entries = 1;
	for (int i = 0; i < RETRO_MAX_TINTS && shadetable.tints[i] > 0; i++) entries *= shadetable.tints[i];
	return entries;
}

//
// A tint entry's level along each tint, as a fraction from 0 to 1, the last
// tint varying fastest, and 0 for a tint the table does not have
//
inline void RETRO_ShadeTableTintLevels(const RETRO_ShadeTable &shadetable, int entry, float *t)
{
	const int *tints = shadetable.tints;
	int stride = RETRO_ShadeTableTintEntries(shadetable);
	for (int i = 0; i < RETRO_MAX_TINTS; i++) t[i] = 0;
	for (int i = 0; i < RETRO_MAX_TINTS && tints[i] > 0; i++) {
		stride /= tints[i];
		t[i] = RETRO_ShadeTableLevel(entry / stride % tints[i], tints[i]);
	}
}

//
// Light every color of a texture palette across every shade, ramp after ramp,
// and return how many colors that wrote. The texture palette is taken to be
// 6-bit, as a palette read from a PCX written by a VGA demo is, and only so
// many texture colors fit in a shade table
//
inline int RETRO_CreateShadingRamps(const RETRO_Palette *texturepalette, int texturecolors, float specularity, float falloff, RETRO_Palette *ramps)
{
	texturecolors = MIN(texturecolors, RETRO_SHADE_TABLE_COLORS);

	for (int i = 0; i < texturecolors; i++) {
		RETRO_Palette texcolor = texturepalette[i];

		texcolor.r = CLAMP64(texcolor.r);
		texcolor.g = CLAMP64(texcolor.g);
		texcolor.b = CLAMP64(texcolor.b);

		RETRO_CreatePhongRamp(&ramps[i * RETRO_SHADE_TABLE_SHADES], RETRO_SHADE_TABLE_SHADES, texcolor, specularity, falloff, 63);
	}

	return texturecolors * RETRO_SHADE_TABLE_SHADES;
}

// *******************************************************************
// Public functions
// *******************************************************************

//
// Fit a 6-bit palette to a material: light every texture color at every shade,
// and fit the palette to the shaded colors by RETRO_FitPalette. At most
// RETRO_SHADE_TABLE_COLORS colors are taken
//
// Entry 0 is kept black, as in every phong palette
//
inline void RETRO_CreatePhongShadeTablePalette(const RETRO_Palette *texturepalette, int texturecolors, RETRO_Palette *palette, float specularity = RETRO_K_SPECULAR, float falloff = RETRO_K_FALLOFF)
{
	RETRO_Palette ramps[RETRO_SHADE_TABLE_SIZE];
	int count = RETRO_CreateShadingRamps(texturepalette, texturecolors, specularity, falloff, ramps);

	static vec3 color[RETRO_SHADE_TABLE_SIZE];
	static float weight[RETRO_SHADE_TABLE_SIZE];
	for (int i = 0; i < count; i++) {
		color[i] = { (float)ramps[i].r, (float)ramps[i].g, (float)ramps[i].b };
		weight[i] = 1;
	}

	palette[0] = RETRO_BLACK;
	RETRO_FitPalette(color, weight, count, palette, 1, RETRO_COLORS, 16, 63);
}

//
// Build shadetable[texel * RETRO_SHADE_TABLE_SHADES + shade] for a material by
// lighting every texture color at every shade and finding the nearest entry in
// a 6-bit palette, normally the one RETRO_CreatePhongShadeTablePalette fitted.
// Several materials can share that palette while keeping their lighting ramps
// separate
//
// The caller owns the table, one per material, and hands it to a model through
// Model3D::shadetable. A model without one draws nothing rather than drawing
// wrong, since the texture mappers stop on a table they were not given
//
// Only the rows of the first texturecolors texels are written. The rest keep
// whatever the table held, so a texel past them draws entry 0 of a zeroed table
// rather than being lit
//
inline void RETRO_CreatePhongShadeTable(const RETRO_Palette *texturepalette, int texturecolors, const RETRO_Palette *palette, unsigned char *shadetable, float specularity = RETRO_K_SPECULAR, float falloff = RETRO_K_FALLOFF)
{
	RETRO_Palette ramps[RETRO_SHADE_TABLE_SIZE];
	int count = RETRO_CreateShadingRamps(texturepalette, texturecolors, specularity, falloff, ramps);

	for (int i = 0; i < count; i++) {
		shadetable[i] = RETRO_NearestPaletteIndex(ramps[i], palette);
	}
}

//
// Build shadetable[color * shades + shade] by scaling every source color from
// the ambient level through full brightness, then finding the nearest entry in
// the same palette. This lets an indexed image use shading without reserving
// palette entries for separate color ramps
//
// The ambient level is where the darkest shade starts, and by default it is
// black. A palette holding one picture's colors rather than ramps is the case
// for raising it: an entry with no dark relatives of its own is matched to the
// nearest thing the palette does have, and towards black that is whichever few
// dark colors the picture happened to contain, whatever the entry started as.
// Lighting off the bottom of such a palette turns faces into holes. A floor
// under the darkening keeps every shade among colors it has plenty of
//
// A table of one shade is that floor alone: every color darkened by the same
// amount, a remap for a shadow cast over whatever is already drawn
//
inline void RETRO_CreateShadeTable(const RETRO_Palette *palette, int colors, int shades, unsigned char *shadetable, float ambient = 0.0f)
{
	for (int source = 0; source < colors; source++) {
		for (int shade = 0; shade < shades; shade++) {
			float level = RETRO_ShadeTableLevel(shade, shades);
			float brightness = ambient + (1.0f - ambient) * level;
			RETRO_Palette target = {
				(unsigned char)(palette[source].r * brightness),
				(unsigned char)(palette[source].g * brightness),
				(unsigned char)(palette[source].b * brightness),
			};
			shadetable[source * shades + shade] =
				RETRO_NearestPaletteIndex(target, palette, colors);
		}
	}
}

//
// A shade table for colors lit by a light of the caller's, with tints
//
// Fills a RETRO_ShadeTable to its own shape: every one of its colors, taken from
// source, is handed to light at every shade and combination of tint levels,
// each level as a fraction from 0 to 1 and a tint the table does not have as
// 0, and the nearest entry in palette is taken. The source colors need not
// be palette's own: a texture keeping a palette of its own is lit onto the
// screen's this way. A table of one color gives the light alone, for
// drawing in it.
//
inline void RETRO_CreateShadeTable(const RETRO_Palette *source, const RETRO_Palette *palette, const RETRO_ShadeTable &shadetable, RETRO_Palette (*light)(RETRO_Palette color, float shade, const float *tint))
{
	int shades = shadetable.shades;
	int entries = RETRO_ShadeTableTintEntries(shadetable);

	for (int color = 0; color < shadetable.colors; color++) {
		for (int shade = 0; shade < shades; shade++) {
			float s = RETRO_ShadeTableLevel(shade, shades);
			for (int entry = 0; entry < entries; entry++) {
				float t[RETRO_MAX_TINTS];
				RETRO_ShadeTableTintLevels(shadetable, entry, t);
				shadetable.table[(color * shades + shade) * entries + entry] = RETRO_NearestPaletteIndex(light(source[color], s, t), palette);
			}
		}
	}
}

//
// Gather the colours a shade table will ask the palette for, to fit one to
//
// Every source color is handed to light at every shade and combination of
// tint levels, as RETRO_CreateShadeTable does, and what comes back is added
// to the histogram weighed by how common that color is, colorweight[color],
// and how common that light is, lightweight[shade * entries + entry], with
// the entries laid out as the table lays them. A light the scene never
// shows weighs nothing and asks for nothing.
//
// brightness darkens every color gathered, for the same colors under a
// shadow remapped over them afterwards
//
inline void RETRO_AddShadeTableColors(RETRO_ColorHistogram *histogram, const RETRO_Palette *source, const float *colorweight, const RETRO_ShadeTable &shadetable, const float *lightweight, RETRO_Palette (*light)(RETRO_Palette color, float shade, const float *tint), float weight = 1.0f, float brightness = 1.0f)
{
	int shades = shadetable.shades;
	int entries = RETRO_ShadeTableTintEntries(shadetable);

	for (int shade = 0; shade < shades; shade++) {
		float s = RETRO_ShadeTableLevel(shade, shades);
		for (int entry = 0; entry < entries; entry++) {
			float w = lightweight[shade * entries + entry] * weight;
			if (w <= 0) continue;
			float t[RETRO_MAX_TINTS];
			RETRO_ShadeTableTintLevels(shadetable, entry, t);
			for (int color = 0; color < shadetable.colors; color++) {
				if (colorweight[color] <= 0) continue;
				RETRO_Palette lit = light(source[color], s, t);
				lit = { (unsigned char)(lit.r * brightness), (unsigned char)(lit.g * brightness), (unsigned char)(lit.b * brightness) };
				RETRO_AddHistogramColor(histogram, lit, colorweight[color] * w);
			}
		}
	}
}

//
// The same for an image, each of its colors weighed by how many of its
// texels have it, in the image's own palette
//
inline void RETRO_AddShadeTableColors(RETRO_ColorHistogram *histogram, int image, const RETRO_ShadeTable &shadetable, const float *lightweight, RETRO_Palette (*light)(RETRO_Palette color, float shade, const float *tint), float weight = 1.0f, float brightness = 1.0f)
{
	const RETRO_Image *source = RETRO.image[image];
	float texels[RETRO_COLORS] = {};
	for (int i = 0; i < source->width * source->height; i++) texels[source->data[i]]++;
	RETRO_AddShadeTableColors(histogram, source->palette, texels, shadetable, lightweight, light, weight, brightness);
}

#endif
