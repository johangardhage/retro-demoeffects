//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROSHADETABLE_H_
#define _RETROSHADETABLE_H_

#include "retropalette.h"

// *******************************************************************
// Private variables
// *******************************************************************

// Lighting every texture color at every shade gives the shaded colors, so a
// texture mapper can look up shadetable[texel * RETRO_SHADES + shade]
#define RETRO_TEXTURE_COLORS 32
#define RETRO_SHADES 128
#define RETRO_MAX_SHADING_COLORS (RETRO_TEXTURE_COLORS * RETRO_SHADES)

// An optimal palette is found by halving a 6-bit color cube once per level, which
// leaves one cube, and one palette entry, per leaf
#define RETRO_CUBE_SIZE 64
#define RETRO_CUBE_LEVELS 8
static_assert((1 << RETRO_CUBE_LEVELS) == RETRO_COLORS, "The color cube must have one leaf per palette entry");

enum { RETRO_COLOR_RED, RETRO_COLOR_GREEN, RETRO_COLOR_BLUE };

struct {
	RETRO_Palette shadingramps[RETRO_MAX_SHADING_COLORS];
	RETRO_Palette palette[RETRO_COLORS];
	int shadingcolorcount;
	int palettecolors;
} RETRO_Color;

// *******************************************************************
// Private functions
// *******************************************************************

//
// One component of a color, selected by axis. Returns a reference so the caller
// can read or write the component it picked
//
unsigned char &RETRO_ColorComponent(RETRO_Palette &color, int axis)
{
	if (axis == RETRO_COLOR_RED) return color.r;
	if (axis == RETRO_COLOR_GREEN) return color.g;
	return color.b;
}

//
// Light every color of a texture palette across every shade, giving the shaded
// colors that an optimal palette is then fitted to. The texture palette is
// taken to be 6-bit, as a palette read from a PCX written by a VGA demo is
//
void RETRO_CreateShadingRamps(RETRO_Palette *texturepalette, int texturecolors, float specularity, float falloff)
{
	// Only so many texture colors fit in the shade table
	texturecolors = MIN(texturecolors, RETRO_TEXTURE_COLORS);

	for (int i = 0; i < texturecolors; i++) {
		RETRO_Palette texcolor = texturepalette[i];

		texcolor.r = CLAMP64(texcolor.r);
		texcolor.g = CLAMP64(texcolor.g);
		texcolor.b = CLAMP64(texcolor.b);

		// The ramps are quantised into a RETRO_CUBE_SIZE cube by the median cut,
		// and RETRO_SplitColorCube indexes a histogram of that size with a raw
		// component, so a component must never reach RETRO_CUBE_SIZE
		RETRO_CreatePhongRamp(&RETRO_Color.shadingramps[i * RETRO_SHADES], RETRO_SHADES, texcolor, specularity, falloff, RETRO_CUBE_SIZE - 1);
	}

	RETRO_Color.shadingcolorcount = texturecolors * RETRO_SHADES;
}

//
// A color cube is the half open box [min, max), so a color on the min face is
// inside it and a color on the max face is not
//
bool RETRO_InsideColorCube(RETRO_Palette color, RETRO_Palette min, RETRO_Palette max)
{
	return color.r >= min.r && color.r < max.r &&
		color.g >= min.g && color.g < max.g &&
		color.b >= min.b && color.b < max.b;
}

//
// Shrink a color cube to the tightest box that still holds every shaded color
// inside it
//
void RETRO_ShrinkColorCube(RETRO_Palette *min, RETRO_Palette *max)
{
	// Seed the new bounds inside out, so the first color inside sets them both
	RETRO_Palette newmin = *max;
	RETRO_Palette newmax = *min;

	for (int i = 0; i < RETRO_Color.shadingcolorcount; i++) {
		RETRO_Palette color = RETRO_Color.shadingramps[i];

		if (!RETRO_InsideColorCube(color, *min, *max)) continue;

		// Does this color push the bounds out?
		if (color.r < newmin.r) newmin.r = color.r;
		if (color.g < newmin.g) newmin.g = color.g;
		if (color.b < newmin.b) newmin.b = color.b;

		if (color.r >= newmax.r) newmax.r = color.r + 1;
		if (color.g >= newmax.g) newmax.g = color.g + 1;
		if (color.b >= newmax.b) newmax.b = color.b + 1;
	}

	*min = newmin;
	*max = newmax;
}

//
// Split a color cube in two along the given axis, at the median of the shaded
// colors inside it. The halves come back as [min, minsplit) and [maxsplit, max)
//
void RETRO_SplitColorCube(RETRO_Palette min, RETRO_Palette max, int axis, RETRO_Palette *minsplit, RETRO_Palette *maxsplit)
{
	// Count the shaded colors inside the cube, by their position along the axis
	int histogram[RETRO_CUBE_SIZE] = { 0 };
	int colors = 0;

	for (int i = 0; i < RETRO_Color.shadingcolorcount; i++) {
		RETRO_Palette color = RETRO_Color.shadingramps[i];

		if (!RETRO_InsideColorCube(color, min, max)) continue;

		histogram[RETRO_ColorComponent(color, axis)]++;
		colors++;
	}

	// Walk the histogram until half of the colors are behind us
	int remaining = colors / 2;
	int median = 0;

	for (int i = 0; i < RETRO_CUBE_SIZE; i++) {
		remaining -= histogram[i];

		if (remaining <= 0) {
			median = i;
			break;
		}
	}
	median += 1;

	// Both halves keep the cube they came from, cut at the median
	*minsplit = max;
	*maxsplit = min;
	RETRO_ColorComponent(*minsplit, axis) = median;
	RETRO_ColorComponent(*maxsplit, axis) = median;
}

//
// Halve a color cube once per level, and take the center of each leaf as a
// palette entry
//
void RETRO_SubdivideColorCube(RETRO_Palette min, RETRO_Palette max, int level)
{
	// Shrink the color cube to contain just the used colors
	RETRO_ShrinkColorCube(&min, &max);

	// Find the length of the sides of the color cube
	int deltar = max.r - min.r;
	int deltag = max.g - min.g;
	int deltab = max.b - min.b;

	// If this is the last level then stop, and take the center of the box as its
	// palette entry
	//
	// Heckbert takes the mean of the colors inside the box instead. That is the
	// textbook choice, but it is worth nothing here: RETRO_ShrinkColorCube has
	// already pulled the box tight around its colors, so its center and its mean
	// nearly coincide. Measured over the mask texture the mean moves the fit from
	// rms 1.53 to 1.54 and makes the worst match slightly worse, so the center
	// stays
	if (level == 0) {
		RETRO_Palette color;
		color.r = CLAMP64(min.r + deltar / 2);
		color.g = CLAMP64(min.g + deltag / 2);
		color.b = CLAMP64(min.b + deltab / 2);

		RETRO_Color.palette[RETRO_Color.palettecolors++] = color;
		return;
	}

	// Determine which side is the longest, settling a tie on blue then red
	int longest = RETRO_COLOR_GREEN;
	if (deltab >= deltar && deltab >= deltag) {
		longest = RETRO_COLOR_BLUE;
	} else if (deltar >= deltag && deltar >= deltab) {
		longest = RETRO_COLOR_RED;
	}

	// Split the color cube at the median point on the longest side
	RETRO_Palette minsplit, maxsplit;
	RETRO_SplitColorCube(min, max, longest, &minsplit, &maxsplit);

	// Subdivide both halves, until they bottom out as palette entries
	RETRO_SubdivideColorCube(min, minsplit, level - 1);
	RETRO_SubdivideColorCube(maxsplit, max, level - 1);
}

//
// Nearest palette entry in RGB, by d² = Δr² + Δg² + Δb².
int RETRO_NearestPaletteIndex(RETRO_Palette targetcolor)
{
	int match = 0;
	int mindistance = INT_MAX;

	for (int i = 0; i < RETRO_Color.palettecolors; i++) {
		RETRO_Palette palettecolor = RETRO_Color.palette[i];

		// Calculate distance to this color
		int deltar = (int)palettecolor.r - (int)targetcolor.r;
		int deltag = (int)palettecolor.g - (int)targetcolor.g;
		int deltab = (int)palettecolor.b - (int)targetcolor.b;

		int distance = deltar * deltar + deltag * deltag + deltab * deltab;

		// Is this distance shorter than the current match
		if (distance < mindistance) {
			mindistance = distance;
			match = i;
		}
	}

	return match;
}

// Nearest entry in an explicit palette, by squared RGB distance.
unsigned char RETRO_ClosestPaletteColor(RETRO_Palette target, RETRO_Palette *palette, int colors = RETRO_COLORS)
{
	int match = 0;
	int min_distance = 3 * 255 * 255 + 1;

	for (int color = 0; color < colors; color++) {
		int dr = (int)palette[color].r - target.r;
		int dg = (int)palette[color].g - target.g;
		int db = (int)palette[color].b - target.b;
		int distance = dr * dr + dg * dg + db * db;
		if (distance < min_distance) {
			min_distance = distance;
			match = color;
		}
	}

	return match;
}

// *******************************************************************
// Public functions
// *******************************************************************

RETRO_Palette *RETRO_OptimalPalette(void)
{
	return RETRO_Color.palette;
}

//
// Build a shade table for a material against the current optimal palette. This
// lets several materials share one palette while keeping their lighting ramps
// separate
//
// The caller owns the table, one per material, and hands it to a model through
// Model3D::shadetable. A model without one draws nothing rather than drawing
// wrong, since the texture mappers stop on a table they were not given
//
void RETRO_CreateShadeTable(RETRO_Palette *texturepalette, int texturecolors, float specularity, float falloff, unsigned char *shadetable)
{
	// There has to be a palette to match against. Without this the match below
	// would find nothing, leave every entry pointing at color 0, and the model
	// would draw solid black with nothing to say why
	if (RETRO_Color.palettecolors == 0) {
		RETRO_RageQuit("RETRO_CreateShadeTable needs a palette, call RETRO_CreateOptimalPalette first\n");
	}

	RETRO_CreateShadingRamps(texturepalette, texturecolors, specularity, falloff);

	for (int i = 0; i < RETRO_Color.shadingcolorcount; i++) {
		shadetable[i] = RETRO_NearestPaletteIndex(RETRO_Color.shadingramps[i]);
	}
}

//
// Fit a 6-bit palette to a material by median cut: light every texture color at
// every shade, then halve the cube of the shaded colors until there is one cube
// per palette entry. At most RETRO_TEXTURE_COLORS colors are taken
//
// This is Heckbert's median cut with two simplifications. Heckbert keeps a queue
// and always splits whichever box currently holds the most colors; here every
// box is split once per level, so the tree is a fixed RETRO_CUBE_LEVELS deep.
// And Heckbert takes each box's representative as the mean of the colors inside
// it, where this takes the center of the box. Both are the cheaper choice and
// both cost a little accuracy. The fixed depth is also why some leaves come out
// empty: a box holding one color still gets split, and one half is then empty
// and spends a palette entry on the center of nothing
//
void RETRO_CreateOptimalPalette(RETRO_Palette *texturepalette, int texturecolors, float specularity = RETRO_K_SPECULAR, float falloff = RETRO_K_FALLOFF)
{
	RETRO_CreateShadingRamps(texturepalette, texturecolors, specularity, falloff);

	RETRO_Color.palettecolors = 0;
	RETRO_Palette min = { 0, 0, 0 };
	RETRO_Palette max = { RETRO_CUBE_SIZE, RETRO_CUBE_SIZE, RETRO_CUBE_SIZE };
	RETRO_SubdivideColorCube(min, max, RETRO_CUBE_LEVELS);
}

// Build shadetable[color * shades + shade] by scaling every source color from
// the ambient level through full brightness, then finding the nearest entry in
// the same palette. This lets an indexed image use shading without reserving
// palette entries for separate color ramps.
//
// The ambient level is where the darkest shade starts, and by default it is
// black. A palette holding one picture's colors rather than ramps is the case
// for raising it: an entry with no dark relatives of its own is matched to the
// nearest thing the palette does have, and towards black that is whichever few
// dark colors the picture happened to contain, whatever the entry started as.
// Lighting off the bottom of such a palette turns faces into holes. A floor
// under the darkening keeps every shade among colors it has plenty of.
void RETRO_CreatePaletteShadeTable(RETRO_Palette *palette, int colors, int shades, unsigned char *shadetable, float ambient = 0.0f)
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
				RETRO_ClosestPaletteColor(target, palette, colors);
		}
	}
}

#endif
