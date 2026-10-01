//
// Shade table
//
// A test bench for the shade tables and the color matching under them: four
// palettes, each with a table built by the library, shown on four pages and
// scored, so that a change to the library can be seen and measured.
//
// A shade table answers, for a color and a shade, with the palette entry
// nearest the color that light would give. The screen holds only the palette,
// so the color asked for cannot be drawn beside the entry that answered. It is
// worked out here instead, unrounded, for every entry of the table, and the
// distance between the two is the table's error.
//
// The distance is not the one the library matches with. The library takes the
// nearest entry by a weighted squared RGB distance, and scored by that
// distance its choice is the best there is, whatever it looks like. The error
// is measured in CIELAB, where equal distances look about equally large. A
// color's linear light is taken to X, Y and Z, each over the white's, and
// comes out as a lightness L from 0 to 100 and two color axes a and b,
//
//   L = 116 f(Y) − 16,   a = 500 (f(X) − f(Y)),   b = 200 (f(Y) − f(Z))
//
//   t > (6/29)³:  f(t) = cbrt(t)
//   otherwise:    f(t) = t / (3 (6/29)²) + 4/29
//
// The straight piece of f near black matters to a shade table, half of which
// is dark: a cube root alone would count the steps there as far larger than
// they look. A plain distance in a and b would do the same to strong colors,
// where a step shows less than the same step between grays, so the difference
// in a and b is split into one of chroma, C = sqrt(a² + b²), and one of hue,
// and each is taken for less the stronger the color asked for (CIE94):
//
//   ΔH² = Δa² + Δb² − ΔC²
//   error² = ΔL² + (ΔC / (1 + 0.045 C))² + (ΔH / (1 + 0.015 C))²
//
// An error of about 2 can just be seen between two flat areas side by side.
//
// The palettes, and how each table is built:
//
//   picture  a picture's own palette, shaded into itself from black to its
//            own colors by RETRO_CreateShadeTable
//   vga      the palette of mode 13h, shaded into itself the same way, with
//            the picture matched into it
//   fitted   a palette fitted to the picture's colors at every shade, gathered
//            by RETRO_AddShadeTableColors and fitted by
//            RETRO_CreateHistogramPalette, with the table lit onto it by
//            RETRO_CreateShadeTable
//   phong    a texture's 6-bit palette lit by the phong model, the palette
//            from RETRO_CreatePhongShadeTablePalette and the table from
//            RETRO_CreatePhongShadeTable
//
// The pages:
//
//   table    the table itself, a column for every color and the shades up it
//            from the darkest at the bottom. The colors are sorted into grays
//            and HUE_SECTORS sectors of hue, by lightness within each
//   error    the same grid, every entry drawn as its error on a ramp from
//            black through blue, red and yellow to white at HEAT_FULL
//   picture  the picture read through the table, under a light that moves
//            across it and fades evenly to black LIGHT_REACH away
//   match    a field of every hue across and every lightness down, its
//            saturation rising and falling, matched to the palette pixel by
//            pixel: above by RETRO_NearestPaletteIndex, below through the
//            cube of RETRO_CreateColorLUT
//
// The scores, printed under every page and once to the standard output:
//
//   error    the mean error of the table, each color weighed by how many of
//            the picture's texels have it, then the worst, then the floor:
//            the mean had every entry been the palette's nearest by this error.
//            What lies between the mean and the floor is lost in the
//            matching, and the floor itself in the palette
//   light    the mean error in L alone, and in chroma and hue together
//   entries  how many different entries a color's shades land on, on average
//   reversals  how often a shade comes out darker than the one below it, by
//            more than REVERSAL_STEP
//   match    the mean error of colors every MATCH_STEP levels through the RGB
//            cube, matched directly, then through the cube, then the floor
//
// The pages turn by themselves, PAGE_SECONDS each, through every palette in
// turn. The arrow keys take over: left and right turn the page, up and down
// change the palette.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrofont.h"
#include "lib/retroshadetable.h"
#include "lib/retrovector.h"

#define SHADES 64 // of the tables shaded from black
#define TABLE_SIZE (RETRO_COLORS * SHADES) // entries, the most a table here has
#define VIEW_HEIGHT 192 // rows of a page, above the scores
#define GRID_WIDTH 256 // pixels across the table's grid
#define PAGE_SECONDS 4.0
#define HEAT_FULL 20.0f // the error at the top of the heat ramp
#define REVERSAL_STEP 0.5f // of lightness
#define GRAY_CHROMA 6.0f // a color with less is sorted among the grays
#define HUE_SECTORS 12
#define LUT_SIZE 32 // cells along a side of the color cube
#define MATCH_STEP 7 // levels between the colors the matching is scored on
#define LIGHT_REACH 260.0f // pixels from the light to black
#define LIGHT_PERIOD 12.0 // seconds for the light's path to close
#define FIELD_PERIOD 10.0 // seconds for the field's saturation to rise and fall
#define TEXT_X 4
#define TEXT_Y (VIEW_HEIGHT + 2)

enum { SUBJECT_PICTURE, SUBJECT_VGA, SUBJECT_FITTED, SUBJECT_PHONG, SUBJECTS };
enum { PAGE_TABLE, PAGE_ERROR, PAGE_PICTURE, PAGE_MATCH, PAGES };

static const char *PageNames[PAGES] = { "table", "error", "picture", "match" };

struct Subject {
	const char *name;
	const unsigned char *picture; // read through the table on the picture page
	int width, height;
	RETRO_Palette source[RETRO_COLORS]; // the colors the table lights, 8-bit
	float weight[RETRO_COLORS]; // how many of the picture's texels have each
	RETRO_Palette palette[RETRO_COLORS]; // the screen's, 8-bit
	int colors, shades;
	unsigned char table[TABLE_SIZE];
	vec3 ideal[TABLE_SIZE]; // the color every entry was asked for, unrounded
	unsigned char heat[TABLE_SIZE]; // every entry's error, on the heat ramp
	int order[RETRO_COLORS]; // the colors, sorted for the grid
	unsigned char lut[LUT_SIZE][LUT_SIZE][LUT_SIZE];
	unsigned char white, black; // the palette's nearest to each
	float error, worst, floor, light, chroma, entries;
	int reversals;
	float match, lutmatch, matchfloor;
};

Subject Subjects[SUBJECTS];
RETRO_Palette HeatPalette[RETRO_COLORS];
RETRO_ColorHistogram Histogram;
unsigned char Remapped[RETRO_WIDTH * RETRO_HEIGHT]; // the picture in the vga palette

static vec3 ColorVector(RETRO_Palette color)
{
	return { (float)color.r, (float)color.g, (float)color.b };
}

//
// The f of CIELAB: a cube root, straight near black
//
static float LabCurve(float t)
{
	float knee = 6.0f / 29;
	return t > knee * knee * knee ? cbrtf(t) : t / (3 * knee * knee) + 4.0f / 29;
}

//
// An 8-bit color in CIELAB, as (L, a, b), against the white of sRGB
//
static vec3 Lab(vec3 color)
{
	float r = RETRO_SRGBToLinear(color.x / 255);
	float g = RETRO_SRGBToLinear(color.y / 255);
	float b = RETRO_SRGBToLinear(color.z / 255);

	float x = LabCurve((0.4124564f * r + 0.3575761f * g + 0.1804375f * b) / 0.95047f);
	float y = LabCurve(0.2126729f * r + 0.7151522f * g + 0.0721750f * b);
	float z = LabCurve((0.0193339f * r + 0.1191920f * g + 0.9503041f * b) / 1.08883f);

	return { 116 * y - 16, 500 * (x - y), 200 * (y - z) };
}

//
// The error of a color against the one asked for, both in CIELAB, as its part
// in lightness and its part in chroma and hue. Its length is the error
//
static vec2 LabError(vec3 asked, vec3 got)
{
	float chroma = hypotf(asked.y, asked.z);
	float dc = hypotf(got.y, got.z) - chroma;
	float da = got.y - asked.y;
	float db = got.z - asked.z;
	float dh2 = MAX(da * da + db * db - dc * dc, 0.0f);
	float sc = 1 + 0.045f * chroma;
	float sh = 1 + 0.015f * chroma;

	return { got.x - asked.x, sqrtf(dc * dc / (sc * sc) + dh2 / (sh * sh)) };
}

//
// The error of the entry of a palette, given in CIELAB, nearest a color there
//
static float NearestError(vec3 asked, const vec3 *palettelab)
{
	float nearest = 1e30f;
	for (int i = 0; i < RETRO_COLORS; i++) {
		nearest = MIN(nearest, length(LabError(asked, palettelab[i])));
	}
	return nearest;
}

//
// A color darkened to a shade, the light of the tables shaded from black
//
static RETRO_Palette Dim(RETRO_Palette color, float shade, const float *tint)
{
	return {
		(unsigned char)(color.r * shade + 0.5f),
		(unsigned char)(color.g * shade + 0.5f),
		(unsigned char)(color.b * shade + 0.5f),
	};
}

//
// A channel of a color from its hue, lightness and saturation, each from 0 to
// 1: n is 0 for red, 8 for green and 4 for blue
//
static unsigned char HueChannel(float n, float hue, float saturation, float lightness)
{
	float k = fmodf(n + hue * 12, 12);
	float a = saturation * MIN(lightness, 1 - lightness);
	return CLAMP256((lightness - a * clamp(MIN(k - 3, 9 - k), -1.0f, 1.0f)) * 255 + 0.5f);
}

//
// Count how many texels of a picture have each color
//
static void CountTexels(Subject *subject)
{
	for (int i = 0; i < subject->width * subject->height; i++) {
		subject->weight[subject->picture[i]]++;
	}
}

//
// Ask every entry of a table shaded from black for its source color at the
// shade's level
//
static void AskDimmed(Subject *subject)
{
	for (int color = 0; color < subject->colors; color++) {
		for (int shade = 0; shade < subject->shades; shade++) {
			float level = RETRO_ShadeTableLevel(shade, subject->shades);
			subject->ideal[color * subject->shades + shade] = ColorVector(subject->source[color]) * level;
		}
	}
}

//
// Sort a subject's colors for the grid: the grays by lightness, then each
// sector of hue by lightness
//
static void SortColors(Subject *subject)
{
	float key[RETRO_COLORS];
	for (int color = 0; color < subject->colors; color++) {
		vec3 lab = Lab(ColorVector(subject->source[color]));
		float lightness = lab.x / 100;
		if (hypotf(lab.y, lab.z) < GRAY_CHROMA) {
			key[color] = lightness;
		} else {
			int sector = CLAMP((atan2f(lab.z, lab.y) + M_PI) / (2 * M_PI) * HUE_SECTORS, 0, HUE_SECTORS);
			key[color] = 1 + sector + lightness;
		}
	}

	for (int i = 0; i < subject->colors; i++) {
		int place = i;
		for (; place > 0 && key[subject->order[place - 1]] > key[i]; place--) {
			subject->order[place] = subject->order[place - 1];
		}
		subject->order[place] = i;
	}
}

//
// Score a subject's table against the colors it was asked for
//
static void ScoreTable(Subject *subject)
{
	vec3 palettelab[RETRO_COLORS];
	for (int i = 0; i < RETRO_COLORS; i++) {
		palettelab[i] = Lab(ColorVector(subject->palette[i]));
	}

	float total = 0;
	int used = 0, entries = 0;
	for (int color = 0; color < subject->colors; color++) {
		float weight = subject->weight[color];
		bool seen[RETRO_COLORS] = {};
		float below = 0;
		for (int shade = 0; shade < subject->shades; shade++) {
			int i = color * subject->shades + shade;
			vec3 asked = Lab(subject->ideal[i]);
			vec2 parts = LabError(asked, palettelab[subject->table[i]]);
			float error = length(parts);
			subject->heat[i] = CLAMP256(error / HEAT_FULL * RETRO_COLORS);
			if (weight <= 0) continue;

			subject->error += error * weight;
			subject->worst = MAX(subject->worst, error);
			subject->floor += NearestError(asked, palettelab) * weight;
			subject->light += fabsf(parts.x) * weight;
			subject->chroma += parts.y * weight;
			total += weight;

			float lightness = palettelab[subject->table[i]].x;
			if (shade > 0 && lightness < below - REVERSAL_STEP) subject->reversals++;
			below = lightness;
			if (!seen[subject->table[i]]) entries++;
			seen[subject->table[i]] = true;
		}
		if (weight > 0) used++;
	}

	subject->error /= total;
	subject->floor /= total;
	subject->light /= total;
	subject->chroma /= total;
	subject->entries = (float)entries / used;
}

//
// Score the matching into a subject's palette, directly and through its cube
//
static void ScoreMatching(Subject *subject)
{
	vec3 palettelab[RETRO_COLORS];
	for (int i = 0; i < RETRO_COLORS; i++) {
		palettelab[i] = Lab(ColorVector(subject->palette[i]));
	}

	int count = 0;
	for (int r = 0; r < RETRO_COLORS; r += MATCH_STEP) {
		for (int g = 0; g < RETRO_COLORS; g += MATCH_STEP) {
			for (int b = 0; b < RETRO_COLORS; b += MATCH_STEP) {
				RETRO_Palette target = { (unsigned char)r, (unsigned char)g, (unsigned char)b };
				vec3 asked = Lab(ColorVector(target));
				subject->match += length(LabError(asked, palettelab[RETRO_NearestPaletteIndex(target, subject->palette)]));
				subject->lutmatch += length(LabError(asked, palettelab[subject->lut[r >> 3][g >> 3][b >> 3]]));
				subject->matchfloor += NearestError(asked, palettelab);
				count++;
			}
		}
	}

	subject->match /= count;
	subject->lutmatch /= count;
	subject->matchfloor /= count;
}

//
// What every subject needs once its palette and table are built
//
static void FinishSubject(Subject *subject)
{
	RETRO_CreateColorLUT(subject->palette, LUT_SIZE, &subject->lut[0][0][0]);
	subject->white = RETRO_NearestPaletteIndex(RETRO_WHITE, subject->palette);
	subject->black = RETRO_NearestPaletteIndex(RETRO_BLACK, subject->palette);
	SortColors(subject);
	ScoreTable(subject);
	ScoreMatching(subject);

	printf("%-8s error %5.2f worst %5.2f floor %5.2f light %5.2f color %5.2f entries %4.1f of %d reversals %d match %5.2f lut %5.2f floor %5.2f\n",
		subject->name, subject->error, subject->worst, subject->floor, subject->light, subject->chroma,
		subject->entries, subject->shades, subject->reversals, subject->match, subject->lutmatch, subject->matchfloor);
}

//
// A picture's own palette, shaded into itself
//
static void BuildPicture(Subject *subject, const RETRO_Image *picture)
{
	subject->name = "picture";
	subject->picture = picture->data;
	subject->width = picture->width;
	subject->height = picture->height;
	subject->colors = RETRO_COLORS;
	subject->shades = SHADES;
	for (int i = 0; i < RETRO_COLORS; i++) {
		subject->source[i] = subject->palette[i] = picture->palette[i];
	}
	CountTexels(subject);

	RETRO_CreateShadeTable(subject->palette, subject->colors, subject->shades, subject->table);
	AskDimmed(subject);
}

//
// The vga palette, shaded into itself, with the picture matched into it
//
static void BuildVGA(Subject *subject, const RETRO_Image *picture)
{
	subject->name = "vga";
	subject->picture = Remapped;
	subject->width = picture->width;
	subject->height = picture->height;
	subject->colors = RETRO_COLORS;
	subject->shades = SHADES;
	RETRO_CreateDefault8bitPalette(subject->palette);
	for (int i = 0; i < RETRO_COLORS; i++) {
		subject->source[i] = subject->palette[i];
	}
	for (int i = 0; i < picture->width * picture->height; i++) {
		Remapped[i] = RETRO_NearestPaletteIndex(picture->palette[picture->data[i]], subject->palette);
	}
	CountTexels(subject);

	RETRO_CreateShadeTable(subject->palette, subject->colors, subject->shades, subject->table);
	AskDimmed(subject);
}

//
// A palette fitted to the picture's colors at every shade, and the picture's
// own colors lit onto it
//
static void BuildFitted(Subject *subject, const RETRO_Image *picture)
{
	subject->name = "fitted";
	subject->picture = picture->data;
	subject->width = picture->width;
	subject->height = picture->height;
	subject->colors = RETRO_COLORS;
	subject->shades = SHADES;
	for (int i = 0; i < RETRO_COLORS; i++) {
		subject->source[i] = picture->palette[i];
	}
	CountTexels(subject);

	// Every shade is as common as every other
	float lightweight[SHADES];
	for (float &weight : lightweight) {
		weight = 1;
	}
	RETRO_ShadeTable shadetable = { subject->table, subject->colors, subject->shades };
	RETRO_AddShadeTableColors(&Histogram, subject->source, subject->weight, shadetable, lightweight, Dim);
	RETRO_CreateHistogramPalette(&Histogram, subject->palette);
	RETRO_CreateShadeTable(subject->source, subject->palette, shadetable, Dim);
	AskDimmed(subject);
}

//
// A texture's 6-bit palette under the phong model, on a palette fitted to it.
// The screen shows a 6-bit level v as 4v
//
static void BuildPhong(Subject *subject, const RETRO_Image *texture)
{
	subject->name = "phong";
	subject->picture = texture->data;
	subject->width = texture->width;
	subject->height = texture->height;
	subject->colors = RETRO_SHADE_TABLE_COLORS;
	subject->shades = RETRO_SHADE_TABLE_SHADES;
	CountTexels(subject);

	RETRO_Palette palette[RETRO_COLORS];
	RETRO_CreatePhongShadeTablePalette(texture->palette, subject->colors, palette);
	RETRO_CreatePhongShadeTable(texture->palette, subject->colors, palette, subject->table);
	for (int i = 0; i < RETRO_COLORS; i++) {
		subject->palette[i] = { (unsigned char)(palette[i].r << 2), (unsigned char)(palette[i].g << 2), (unsigned char)(palette[i].b << 2) };
	}

	for (int color = 0; color < subject->colors; color++) {
		RETRO_Palette face = texture->palette[color];
		face = { (unsigned char)CLAMP64(face.r), (unsigned char)CLAMP64(face.g), (unsigned char)CLAMP64(face.b) };
		subject->source[color] = { (unsigned char)(face.r << 2), (unsigned char)(face.g << 2), (unsigned char)(face.b << 2) };
		for (int shade = 0; shade < subject->shades; shade++) {
			float theta = RETRO_IncidenceAngle(shade, subject->shades);
			subject->ideal[color * subject->shades + shade] = vec3{
				RETRO_PhongIntensity(face.r / 63.0f, RETRO_LIGHT_R, RETRO_AMBIENT_R, theta, RETRO_K_SPECULAR, RETRO_K_FALLOFF),
				RETRO_PhongIntensity(face.g / 63.0f, RETRO_LIGHT_G, RETRO_AMBIENT_G, theta, RETRO_K_SPECULAR, RETRO_K_FALLOFF),
				RETRO_PhongIntensity(face.b / 63.0f, RETRO_LIGHT_B, RETRO_AMBIENT_B, theta, RETRO_K_SPECULAR, RETRO_K_FALLOFF),
			} * (63 * 4);
		}
	}
}

//
// The table as a grid, or its errors
//
static void DrawTable(const Subject &subject, const unsigned char *entries)
{
	unsigned char *buffer = RETRO_FrameBuffer();
	int left = (RETRO_WIDTH - GRID_WIDTH) / 2;
	int cell = GRID_WIDTH / subject.colors;

	for (int y = 0; y < VIEW_HEIGHT; y++) {
		int shade = (VIEW_HEIGHT - 1 - y) * subject.shades / VIEW_HEIGHT;
		for (int x = 0; x < GRID_WIDTH; x++) {
			int color = subject.order[x / cell];
			buffer[y * RETRO_WIDTH + left + x] = entries[color * subject.shades + shade];
		}
	}
}

//
// The picture through the table, under the moving light
//
static void DrawPicture(const Subject &subject, double time)
{
	// Calculate phase. The light rides a 2:3 Lissajous figure
	double phase = fract(time / LIGHT_PERIOD) * 2 * M_PI;
	float lightx = RETRO_WIDTH / 2.0f + RETRO_WIDTH / 3.0f * sin(2 * phase + M_PI / 4);
	float lighty = VIEW_HEIGHT / 2.0f + VIEW_HEIGHT / 3.0f * sin(3 * phase);

	unsigned char *buffer = RETRO_FrameBuffer();
	int left = (RETRO_WIDTH - subject.width) / 2;
	int top = (VIEW_HEIGHT - subject.height) / 2;

	for (int y = MAX(top, 0); y < MIN(top + subject.height, VIEW_HEIGHT); y++) {
		for (int x = left; x < left + subject.width; x++) {
			float dx = x + 0.5f - lightx;
			float dy = y + 0.5f - lighty;
			float level = CLAMP01(1 - hypotf(dx, dy) / LIGHT_REACH);
			int shade = level * (subject.shades - 1) + 0.5f;
			int texel = subject.picture[(y - top) * subject.width + x - left];
			buffer[y * RETRO_WIDTH + x] = subject.table[texel * subject.shades + shade];
		}
	}
}

//
// The field of hue and lightness, matched directly above and through the
// cube below
//
static void DrawMatch(const Subject &subject, double time)
{
	// Calculate phase
	double phase = fract(time / FIELD_PERIOD) * 2 * M_PI;
	float saturation = 0.5f - 0.5f * cos(phase);

	unsigned char *buffer = RETRO_FrameBuffer();
	int half = VIEW_HEIGHT / 2;

	for (int y = 0; y < half; y++) {
		float lightness = (y + 0.5f) / half;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float hue = (x + 0.5f) / RETRO_WIDTH;
			RETRO_Palette target = {
				HueChannel(0, hue, saturation, lightness),
				HueChannel(8, hue, saturation, lightness),
				HueChannel(4, hue, saturation, lightness),
			};
			buffer[y * RETRO_WIDTH + x] = RETRO_NearestPaletteIndex(target, subject.palette);
			buffer[(half + y) * RETRO_WIDTH + x] = subject.lut[target.r >> 3][target.g >> 3][target.b >> 3];
		}
	}
}

void DEMO_Render(double time, double deltatime)
{
	static bool turning = true;
	static int current = 0;
	static int page = 0;

	// Handle keys
	int pageturn = RETRO_KeyPressed(SDL_SCANCODE_RIGHT) - RETRO_KeyPressed(SDL_SCANCODE_LEFT);
	int subjectturn = RETRO_KeyPressed(SDL_SCANCODE_DOWN) - RETRO_KeyPressed(SDL_SCANCODE_UP);
	if (pageturn != 0 || subjectturn != 0) {
		turning = false;
		page = WRAP(page + pageturn, PAGES);
		current = WRAP(current + subjectturn, SUBJECTS);
	}
	if (turning) {
		// Calculate phase
		double phase = fmod(time / PAGE_SECONDS, SUBJECTS * PAGES);
		int iphase = phase;
		current = iphase / PAGES;
		page = iphase % PAGES;
	}

	const Subject &subject = Subjects[current];
	unsigned char ink = page == PAGE_ERROR ? RETRO_COLORS - 1 : subject.white;
	RETRO_SetPalette(page == PAGE_ERROR ? HeatPalette : subject.palette);
	RETRO_Clear(page == PAGE_ERROR ? 0 : subject.black);

	switch (page) {
	case PAGE_TABLE:
		DrawTable(subject, subject.table);
		break;
	case PAGE_ERROR:
		DrawTable(subject, subject.heat);
		break;
	case PAGE_PICTURE:
		DrawPicture(subject, time);
		break;
	case PAGE_MATCH:
		DrawMatch(subject, time);
		break;
	}

	// Print the scores
	char text[256];
	snprintf(text, sizeof(text),
		"palette %d/%d %-8s page %d/%d %s\n"
		"error %5.2f  worst %5.2f  floor %5.2f\n"
		"light %5.2f  color %5.2f\n"
		"entries %4.1f of %d  reversals %d\n"
		"match %5.2f  lut %5.2f  floor %5.2f",
		current + 1, SUBJECTS, subject.name, page + 1, PAGES, PageNames[page],
		subject.error, subject.worst, subject.floor,
		subject.light, subject.chroma,
		subject.entries, subject.shades, subject.reversals,
		subject.match, subject.lutmatch, subject.matchfloor);
	RETRO_PutString(text, TEXT_X, TEXT_Y, ink);
}

void DEMO_Initialize(void)
{
	RETRO_Image *picture = RETRO_LoadImage("assets/flowers_320x240_quantizized.pcx");
	RETRO_Image *texture = RETRO_LoadImage("assets/mask_texmap_256x256.pcx");
	if (picture->width != RETRO_WIDTH || picture->height != RETRO_HEIGHT) {
		RETRO_RageQuit("The picture must be the size of the screen\n");
	}

	// Init heat ramp
	RETRO_CreateGradientPalette(0, 64, RETRO_BLACK, RETRO_BLUE, HeatPalette);
	RETRO_CreateGradientPalette(64, 128, RETRO_BLUE, RETRO_RED, HeatPalette);
	RETRO_CreateGradientPalette(128, 192, RETRO_RED, RETRO_YELLOW, HeatPalette);
	RETRO_CreateGradientPalette(192, 256, RETRO_YELLOW, RETRO_WHITE, HeatPalette);

	// Build and score the subjects
	BuildPicture(&Subjects[SUBJECT_PICTURE], picture);
	BuildVGA(&Subjects[SUBJECT_VGA], picture);
	BuildFitted(&Subjects[SUBJECT_FITTED], picture);
	BuildPhong(&Subjects[SUBJECT_PHONG], texture);
	for (Subject &subject : Subjects) {
		FinishSubject(&subject);
	}
}
