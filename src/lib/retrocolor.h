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

// Lighting every texture color at every shade gives the shaded colors, so a
// texture mapper can look up shadetable[texel * RETRO_SHADES + shade]
#define RETRO_TEXTURE_COLORS 32
#define RETRO_SHADES 128
#define RETRO_MAX_SHADING_COLORS (RETRO_TEXTURE_COLORS * RETRO_SHADES)

// A phong palette keeps entry 0 black and ramps the material over the rest, so a
// renderer shades from RETRO_PHONG_OFFSET across RETRO_PHONG_SHADES entries
#define RETRO_PHONG_OFFSET 1
#define RETRO_PHONG_SHADES (RETRO_COLORS - RETRO_PHONG_OFFSET)

// An optimal palette is found by halving a 6-bit color cube once per level, which
// leaves one cube, and one palette entry, per leaf
#define RETRO_CUBE_SIZE 64
#define RETRO_CUBE_LEVELS 8
static_assert((1 << RETRO_CUBE_LEVELS) == RETRO_COLORS, "The color cube must have one leaf per palette entry");

enum { RETRO_COLOR_RED, RETRO_COLOR_GREEN, RETRO_COLOR_BLUE };

// Named colors, with 8-bit components as used by RETRO_SetPalette. A demo can
// name its own colors the same way, for example #define EMBER RETRO_RGB(0x140014)

#define RETRO_RGB(hex) RETRO_Palette{ ((hex) >> 16) & 0xff, ((hex) >> 8) & 0xff, (hex) & 0xff }

#define RETRO_BLACK RETRO_Palette{ 0, 0, 0 }
#define RETRO_GRAY RETRO_Palette{ 128, 128, 128 }
#define RETRO_WHITE RETRO_Palette{ 255, 255, 255 }
#define RETRO_RED RETRO_Palette{ 255, 0, 0 }
#define RETRO_GREEN RETRO_Palette{ 0, 255, 0 }
#define RETRO_BLUE RETRO_Palette{ 0, 0, 255 }
#define RETRO_CYAN RETRO_Palette{ 0, 255, 255 }
#define RETRO_MAGENTA RETRO_Palette{ 255, 0, 255 }
#define RETRO_YELLOW RETRO_Palette{ 255, 255, 0 }
#define RETRO_ORANGE RETRO_Palette{ 255, 128, 0 }
#define RETRO_GOLD RETRO_Palette{ 254, 204, 0 }
#define RETRO_TAN RETRO_Palette{ 210, 180, 140 }
#define RETRO_PURPLE RETRO_Palette{ 128, 0, 255 }
#define RETRO_PINK RETRO_Palette{ 255, 128, 192 }
#define RETRO_HOTPINK RETRO_Palette{ 255, 105, 180 }
#define RETRO_DEEPPINK RETRO_Palette{ 219, 59, 150 }
#define RETRO_PERIWINKLE RETRO_Palette{ 160, 168, 252 }
#define RETRO_AZURE RETRO_Palette{ 0, 128, 255 }
#define RETRO_CERULEAN RETRO_Palette{ 0, 106, 167 }
#define RETRO_MEDIUMRED RETRO_Palette{ 187, 0, 0 }
#define RETRO_LIGHTRED RETRO_Palette{ 255, 204, 204 }
#define RETRO_LIGHTBLUE RETRO_Palette{ 102, 170, 255 }
#define RETRO_DARKRED RETRO_Palette{ 128, 0, 0 }
#define RETRO_DARKGREEN RETRO_Palette{ 0, 128, 0 }
#define RETRO_DARKBLUE RETRO_Palette{ 0, 0, 128 }
#define RETRO_DARKMAGENTA RETRO_Palette{ 139, 0, 139 }
#define RETRO_BLUEBLACK RETRO_Palette{ 0, 0, 48 }

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
// The angle of incidence a shade stands for, running from a grazing 90 degrees
// at the first shade to face on 0 degrees at the last
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
// Fill a palette with one material, keeping entry 0 black and laying the ramp
// over the rest. Without a palette the colors are set directly, otherwise they
// are written to it, in both cases scaled to colormax so that a 6-bit palette
// can be filled as readily as an 8-bit one
//
void RETRO_CreateMaterialPalette(RETRO_Palette face, float specularity, float falloff, RETRO_Palette *palette, int colormax)
{
	RETRO_Palette ramp[RETRO_PHONG_SHADES];
	RETRO_CreatePhongRamp(ramp, RETRO_PHONG_SHADES, face, specularity, falloff, colormax);

	RETRO_SetColor(0, RETRO_BLACK, palette);

	for (int i = 0; i < RETRO_PHONG_SHADES; i++) {
		RETRO_SetColor(RETRO_PHONG_OFFSET + i, ramp[i], palette);
	}
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

//
// Fill the colors start..end with a linear interpolation from one color to
// another. The end color is not written, so consecutive gradients can be
// chained without repeating the shared color. Without a palette the colors
// are set directly, otherwise the components are copied as they are given,
// which allows both 6-bit and 8-bit palettes
//
// Linear blend C(i) = from + (i / (end - start)) * (to - from), i in [0, end).
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

#endif
