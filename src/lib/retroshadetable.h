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

// *******************************************************************
// Private variables
// *******************************************************************

// A palette is fitted by halving a 6-bit color cube once per level, which
// leaves one cube, and one palette entry, per leaf
#define RETRO_CUBE_SIZE 64
#define RETRO_CUBE_LEVELS 8
static_assert((1 << RETRO_CUBE_LEVELS) == RETRO_COLORS, "The color cube must have one leaf per palette entry");

enum { RETRO_COMPONENT_RED, RETRO_COMPONENT_GREEN, RETRO_COMPONENT_BLUE };

//
// Every texture color lit at every shade, ramp after ramp, with 6-bit
// components
//
struct RETRO_ShadingRamps {
	RETRO_Palette colors[RETRO_SHADE_TABLE_SIZE];
	int count;
};

//
// The half open box [min, max) of colors, so a color on the min face is inside
// it and a color on the max face is not
//
struct RETRO_ColorCube {
	RETRO_Palette min;
	RETRO_Palette max;
};

// *******************************************************************
// Private functions
// *******************************************************************

//
// One component of a color, selected by axis. Returns a reference so the caller
// can read or write the component it picked
//
inline unsigned char &RETRO_ColorComponent(RETRO_Palette &color, int axis)
{
	if (axis == RETRO_COMPONENT_RED) return color.r;
	if (axis == RETRO_COMPONENT_GREEN) return color.g;
	return color.b;
}

//
// Light every color of a texture palette across every shade. The texture
// palette is taken to be 6-bit, as a palette read from a PCX written by a VGA
// demo is, and only so many texture colors fit in a shade table
//
inline void RETRO_CreateShadingRamps(const RETRO_Palette *texturepalette, int texturecolors, float specularity, float falloff, RETRO_ShadingRamps *ramps)
{
	texturecolors = MIN(texturecolors, RETRO_SHADE_TABLE_COLORS);

	for (int i = 0; i < texturecolors; i++) {
		RETRO_Palette texcolor = texturepalette[i];

		texcolor.r = CLAMP64(texcolor.r);
		texcolor.g = CLAMP64(texcolor.g);
		texcolor.b = CLAMP64(texcolor.b);

		// The median cut counts the shaded colors in a histogram of RETRO_CUBE_SIZE
		// bins, indexed by a raw component, so a component must never reach
		// RETRO_CUBE_SIZE
		RETRO_CreatePhongRamp(&ramps->colors[i * RETRO_SHADE_TABLE_SHADES], RETRO_SHADE_TABLE_SHADES, texcolor, specularity, falloff, RETRO_CUBE_SIZE - 1);
	}

	ramps->count = texturecolors * RETRO_SHADE_TABLE_SHADES;
}

inline bool RETRO_InsideColorCube(RETRO_Palette color, const RETRO_ColorCube &cube)
{
	return color.r >= cube.min.r && color.r < cube.max.r &&
		color.g >= cube.min.g && color.g < cube.max.g &&
		color.b >= cube.min.b && color.b < cube.max.b;
}

//
// The tightest cube that still holds every shaded color inside the given one.
// A cube with no colors inside comes back inside out, min above max
//
inline RETRO_ColorCube RETRO_ShrinkColorCube(const RETRO_ShadingRamps &ramps, const RETRO_ColorCube &cube)
{
	// Seed the bounds inside out, so the first color inside sets them both
	RETRO_ColorCube shrunk = { cube.max, cube.min };

	for (int i = 0; i < ramps.count; i++) {
		RETRO_Palette color = ramps.colors[i];

		if (!RETRO_InsideColorCube(color, cube)) continue;

		// Does this color push the bounds out?
		if (color.r < shrunk.min.r) shrunk.min.r = color.r;
		if (color.g < shrunk.min.g) shrunk.min.g = color.g;
		if (color.b < shrunk.min.b) shrunk.min.b = color.b;

		if (color.r >= shrunk.max.r) shrunk.max.r = color.r + 1;
		if (color.g >= shrunk.max.g) shrunk.max.g = color.g + 1;
		if (color.b >= shrunk.max.b) shrunk.max.b = color.b + 1;
	}

	return shrunk;
}

//
// Cut a color cube in two across the given axis, just past the median of the
// shaded colors inside it. The lower half takes the colors below the cut and
// the upper half the rest
//
// With one color or none inside, half of nothing is already behind the walk at
// the first bin, so the cut lands at 1 wherever the cube is
//
inline void RETRO_SplitColorCube(const RETRO_ShadingRamps &ramps, const RETRO_ColorCube &cube, int axis, RETRO_ColorCube *lower, RETRO_ColorCube *upper)
{
	// Count the shaded colors inside the cube, by their position along the axis
	int histogram[RETRO_CUBE_SIZE] = { 0 };
	int colors = 0;

	for (int i = 0; i < ramps.count; i++) {
		RETRO_Palette color = ramps.colors[i];

		if (!RETRO_InsideColorCube(color, cube)) continue;

		histogram[RETRO_ColorComponent(color, axis)]++;
		colors++;
	}

	// Walk the histogram until half of the colors are behind us, and cut after
	// the bin that got us there
	int remaining = colors / 2;
	int cut = 0;

	do {
		remaining -= histogram[cut++];
	} while (remaining > 0);

	*lower = cube;
	*upper = cube;
	RETRO_ColorComponent(lower->max, axis) = cut;
	RETRO_ColorComponent(upper->min, axis) = cut;
}

//
// Halve a color cube level times, and write the center of each leaf to the
// palette, lower halves first. A cube at level fills exactly 1 << level
// entries, so each half is handed its own run of the palette
//
inline void RETRO_SubdivideColorCube(const RETRO_ShadingRamps &ramps, const RETRO_ColorCube &cube, int level, RETRO_Palette *palette)
{
	RETRO_ColorCube shrunk = RETRO_ShrinkColorCube(ramps, cube);

	int deltar = shrunk.max.r - shrunk.min.r;
	int deltag = shrunk.max.g - shrunk.min.g;
	int deltab = shrunk.max.b - shrunk.min.b;

	// At the last level take the center of the cube as its palette entry. An
	// inside out cube, which holds no colors, still gets one, clamped into range
	//
	// Heckbert takes the mean of the colors inside the cube instead. That is the
	// textbook choice, but it is worth nothing here: the cube has already been
	// pulled tight around its colors, so its center and its mean nearly coincide.
	// Measured over a 32-color model texture the mean moves the fit from rms 1.53 to 1.54
	// and makes the worst match slightly worse, so the center stays
	if (level == 0) {
		palette->r = CLAMP64(shrunk.min.r + deltar / 2);
		palette->g = CLAMP64(shrunk.min.g + deltag / 2);
		palette->b = CLAMP64(shrunk.min.b + deltab / 2);
		return;
	}

	// Cut across the longest side, settling a tie on blue then red
	int longest = RETRO_COMPONENT_GREEN;
	if (deltab >= deltar && deltab >= deltag) {
		longest = RETRO_COMPONENT_BLUE;
	} else if (deltar >= deltag && deltar >= deltab) {
		longest = RETRO_COMPONENT_RED;
	}

	RETRO_ColorCube lower, upper;
	RETRO_SplitColorCube(ramps, shrunk, longest, &lower, &upper);

	RETRO_SubdivideColorCube(ramps, lower, level - 1, palette);
	RETRO_SubdivideColorCube(ramps, upper, level - 1, palette + (1 << (level - 1)));
}

// *******************************************************************
// Public functions
// *******************************************************************

//
// Fit a 6-bit palette of RETRO_COLORS entries to a material by median cut:
// light every texture color at every shade, then halve the cube of the shaded
// colors until there is one cube per palette entry. At most
// RETRO_SHADE_TABLE_COLORS colors are taken
//
// This is Heckbert's median cut with two simplifications. Heckbert keeps a queue
// and always splits whichever box currently holds the most colors; here every
// box is split once per level, so the tree is a fixed RETRO_CUBE_LEVELS deep.
// And Heckbert takes each box's representative as the mean of the colors inside
// it, where this takes the center of the box, which measures no worse here since
// the box has already been shrunk around its colors. The fixed depth is the
// cheaper choice and costs a little accuracy. It is also why some leaves come
// out empty: a box holding one color still gets split, and one half is then
// empty and spends a palette entry on the center of nothing
//
inline void RETRO_CreatePhongShadeTablePalette(const RETRO_Palette *texturepalette, int texturecolors, RETRO_Palette *palette, float specularity = RETRO_K_SPECULAR, float falloff = RETRO_K_FALLOFF)
{
	RETRO_ShadingRamps ramps;
	RETRO_CreateShadingRamps(texturepalette, texturecolors, specularity, falloff, &ramps);

	RETRO_ColorCube cube = { { 0, 0, 0 }, { RETRO_CUBE_SIZE, RETRO_CUBE_SIZE, RETRO_CUBE_SIZE } };
	RETRO_SubdivideColorCube(ramps, cube, RETRO_CUBE_LEVELS, palette);
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
	RETRO_ShadingRamps ramps;
	RETRO_CreateShadingRamps(texturepalette, texturecolors, specularity, falloff, &ramps);

	for (int i = 0; i < ramps.count; i++) {
		shadetable[i] = RETRO_NearestPaletteIndex(ramps.colors[i], palette);
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
inline void RETRO_CreateShadeTable(const RETRO_Palette *palette, int colors, int shades, unsigned char *shadetable, float ambient = 0.0f)
{
	for (int source = 0; source < colors; source++) {
		for (int shade = 0; shade < shades; shade++) {
			float level = shades > 1 ? (float)shade / (shades - 1) : 1;
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

#endif
