//
// Lamps
//
// A matte torus lit by nothing but three point lights, each circling it on a
// path of its own, at its own height, reach and speed. A point light shines
// from where it stands and dims with distance d as
//
//   strength / (1 + linear d)
//
// so the side of the torus a lamp passes lights up while the far side stays
// dark, and where two lamps reach the same surface their light adds. The
// material is matte, its ramp running from black to the face color with no
// highlight: a highlight laid out by angle reads a sum of lamps as one light
// shining straight on, and would flare wherever two of them meet. Each lamp
// is drawn where it stands as a small glowing disc, depth tested against the
// torus, so it slips behind the ring and comes out the other side. Keys 1 to
// 3 shade the torus flat, Gouraud or Phong: one sum of the lamps a face, at
// each corner, or at every pixel.
//
// C turns the lamps red, green and blue, and switches to a palette of its own,
// since a ramp of one color at many brightnesses cannot show three lights of
// different colors mixing. The torus is then given a color light table, with
// three tints of red, green and blue light, each entry the palette color
// nearest that light on its material, and flat, Gouraud and Phong all light it
// in the lamps' colors and look the result up there. The palette is fitted to
// what the tables ask for: the lamps' light is surveyed on the torus's near
// side as they and it turn, and the marble's grays and the plain torus's white
// under each light weigh in as often as it falls. Black, white and each lamp's
// glow are held in it exactly.
//
// T lays pale marble tiles over the torus, gray only, so the lamps' light is
// all the color there is. Under the white lamps each gray is lit along the
// torus's own ramp, through a shade table with a row for each; under the
// colored ones the color light table has a row for each gray instead.
//
// B makes the marble its own bump map, dark as low: the joints sink and the
// veins pit, and each edge catches the lamps it faces, in their colors, even
// where the smooth torus would be dark. Without the marble the plain torus
// takes the same relief, in every shading. Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"
#include "lib/retrofont.h"

#define ROTATION_SPEED 0.4 // radians a second, about each axis
#define PROJECTION_SCALE 38 // pixels per unit of the torus, which is 4.2 across
#define LAMP_STRENGTH 1.6f // where a lamp stands
#define LAMP_LINEAR 0.5f // how fast it dims, per unit of distance
#define LAMP_SIZE 0.12f // radius of a lamp's disc, in the torus's units
#define LAMP_SHADES 8 // palette entries at the top for the lamps' glow
#define TORUS_SHADES (RETRO_COLORS - RETRO_PHONG_OFFSET - LAMP_SHADES)
#define TORUS_COLOR RETRO_Palette{ 230, 200, 150 }
#define LAMP_EDGE RETRO_Palette{ 255, 170, 60 }
#define TORUS_WHITE 230 // the material under colored lamps, gray on every channel
#define TABLE_LEVELS 16 // of each of red, green and blue light in the color light table
#define PALETTE_SAMPLES 64 // moments the colored lamps' light is surveyed at
#define PALETTE_SPAN 60.0 // seconds they are spread over
#define PLAIN_WEIGHT (1 / 4.0f) // the plain torus's say in the colored palette, beside the marble's

enum { MODE_FLAT, MODE_GOURAUD, MODE_PHONG, MODES };

// How the torus is shaded, plain and with the marble, and what the mode is
// called on screen
struct Mode {
	RETRO_POLY_TYPE rendertype;
	RETRO_POLY_SHADE shadertype;
	RETRO_POLY_SHADE textureshade;
	const char *name;
};

static const Mode Modes[MODES] = {
	{ RETRO_POLY_FLAT, RETRO_SHADE_FLAT, RETRO_SHADE_FLAT, "Flat" },
	{ RETRO_POLY_GOURAUD, RETRO_SHADE_NONE, RETRO_SHADE_GOURAUD, "Gouraud" },
	{ RETRO_POLY_PHONG, RETRO_SHADE_NONE, RETRO_SHADE_PHONG, "Phong" },
};

// A lamp's circle about the view's vertical axis through the torus's center
struct Lamp {
	float radius; // from the axis
	float height; // above (negative) or below the center
	float speed; // radians a second, and which way round
	float phase; // where it starts
	vec3 color; // when the lamps are colored
};

static const Lamp Lamps[] = {
	{ 2.3f, -1.3f, 0.9f, 0.0f, { 1.0f, 0.15f, 0.1f } },
	{ 2.6f, 0.3f, -1.3f, 2.1f, { 0.1f, 1.0f, 0.15f } },
	{ 2.2f, 1.4f, 0.6f, 4.2f, { 0.2f, 0.35f, 1.0f } },
};
#define LAMPS (int)(sizeof(Lamps) / sizeof(Lamps[0]))
static_assert(LAMPS <= RETRO_MAX_LIGHTS, "Each lamp is a light of the torus's rig");

// The colored palette's held entries: black, white, and each lamp's glow from
// its own color toward white, white being its last step
enum { COLOR_BLACK, COLOR_WHITE, COLOR_GLOW, HELD = COLOR_GLOW + LAMPS * (LAMP_SHADES - 1) };

static RETRO_Lighting Lighting;
static RETRO_Palette WhitePalette[RETRO_COLORS];
static RETRO_Palette ColorPalette[RETRO_COLORS];

// The torus's color light table, for the colored lamps
static unsigned char ColorTableData[TABLE_LEVELS * TABLE_LEVELS * TABLE_LEVELS];
static const RETRO_ShadeTable ColorTable = { ColorTableData, 1, 1, { TABLE_LEVELS, TABLE_LEVELS, TABLE_LEVELS } };

// The marble's tables, a row for each of its grays: under the white lamps,
// and under the colored ones
static unsigned char MarbleShadeTable[RETRO_SHADE_TABLE_SIZE];
static unsigned char MarbleColorTableData[RETRO_SHADE_TABLE_COLORS * TABLE_LEVELS * TABLE_LEVELS * TABLE_LEVELS];
static const RETRO_ShadeTable MarbleColorTable = { MarbleColorTableData, RETRO_SHADE_TABLE_COLORS, 1, { TABLE_LEVELS, TABLE_LEVELS, TABLE_LEVELS } };

// A marble gray under the white lamps, at a shade from 0 to 1: the torus's
// ramp at that shade, darkened by the gray
static RETRO_Palette MarbleLight(RETRO_Palette gray, float shade, const float *tint)
{
	RETRO_Palette lit = WhitePalette[RETRO_PHONG_OFFSET + (int)(shade * (TORUS_SHADES - 1) + 0.5f)];
	return RETRO_ShadeColor(lit, gray.r / 255.0f);
}

// Step k of lamp i's glow in the colored palette, of LAMP_SHADES from its
// color to white
static unsigned char LampGlow(int i, int k)
{
	return k == LAMP_SHADES - 1 ? COLOR_WHITE : COLOR_GLOW + i * (LAMP_SHADES - 1) + k;
}

// The lamps on their circles at a time
static void PlaceLamps(double time)
{
	for (int i = 0; i < LAMPS; i++) {
		float angle = fmod(time * Lamps[i].speed + Lamps[i].phase, 2 * M_PI);
		Lighting.light[i].position = { Lamps[i].radius * cosf(angle), Lamps[i].height, Lamps[i].radius * sinf(angle) };
	}
}

//
// The colored palette, fitted to the colors the colored lamps light the torus
// in. Each light they cast on its near side, the corners facing the viewer,
// is counted at moments spread over their circles and its turning, rounded to
// the color light tables' levels, and the marble's grays, as often as its
// texels have them, and the plain torus's white, with less say, are gathered
// under each as often as it falls
//
static void FitColorPalette(const RETRO_Image *marble, const RETRO_Palette *materials)
{
	static float lightweight[TABLE_LEVELS * TABLE_LEVELS * TABLE_LEVELS];
	Model3D *model = RETRO_Get3DModel();
	for (int sample = 0; sample < PALETTE_SAMPLES; sample++) {
		double time = sample * PALETTE_SPAN / PALETTE_SAMPLES;
		PlaceLamps(time);
		RETRO_Lighting colored = Lighting;
		for (int i = 0; i < LAMPS; i++) {
			colored.light[i].color = Lamps[i].color;
		}
		float angle = fmod(time * ROTATION_SPEED, 2 * M_PI);
		RETRO_RotateModel(angle, angle, angle);
		for (int i = 0; i < model->faces; i++) {
			const Face *face = &model->face[i];
			for (int j = 0; j < face->vertices; j++) {
				vec3 n = model->normal[face->vertexnormal[j]].rdir;
				if (n.z >= 0) continue;
				vec3 light = RETRO_ColorLambert(colored, model->vertex[face->vertex[j]].rpos, n);
				lightweight[RETRO_ColorEntry(ColorTable, light)]++;
			}
		}
	}

	static RETRO_ColorHistogram histogram;
	float texels[RETRO_SHADE_TABLE_COLORS] = {};
	for (int i = 0; i < marble->width * marble->height; i++) {
		texels[CLAMP(marble->data[i], 0, RETRO_SHADE_TABLE_COLORS)]++;
	}
	RETRO_AddShadeTableColors(&histogram, materials, texels, MarbleColorTable, lightweight, RETRO_LightColor);
	RETRO_Palette white = { TORUS_WHITE, TORUS_WHITE, TORUS_WHITE };
	float plaintexels = marble->width * marble->height; // as many as the marble's
	RETRO_AddShadeTableColors(&histogram, &white, &plaintexels, ColorTable, lightweight, RETRO_LightColor, PLAIN_WEIGHT);

	RETRO_Palette held[HELD];
	held[COLOR_BLACK] = RETRO_BLACK;
	held[COLOR_WHITE] = RETRO_WHITE;
	for (int i = 0; i < LAMPS; i++) {
		for (int k = 0; k < LAMP_SHADES - 1; k++) {
			vec3 glow = mix(Lamps[i].color, vec3{ 1, 1, 1 }, k / (float)(LAMP_SHADES - 1)) * 255.0f + 0.5f;
			held[LampGlow(i, k)] = { (unsigned char)glow.x, (unsigned char)glow.y, (unsigned char)glow.z };
		}
	}
	RETRO_CreateHistogramPalette(&histogram, ColorPalette, held, HELD);
}

//
// Lamp i, in view space, as a disc that glows from its edge to white at its
// middle, in its own color when the lamps are colored. Its pixels are depth
// tested at the lamp's own depth, so the torus covers the part of it that is
// behind
//
static void DrawLamp(int i, bool colored)
{
	vec3 position = Lighting.light[i].position;
	Vertex lamp = RETRO_ProjectPoint(position, PROJECTION_SCALE);
	if (lamp.q == 0.0f) return;

	float radius = LAMP_SIZE * PROJECTION_SCALE * RETRO_PROJECTION_EYEDISTANCE * lamp.q;
	int reach = ceil(radius);
	for (int y = -reach; y <= reach; y++) {
		for (int x = -reach; x <= reach; x++) {
			float distance = length(vec2{ (float)x, (float)y }) / radius;
			if (distance > 1) continue;

			int px = lamp.spos.x + x;
			int py = lamp.spos.y + y;
			if (RETRO_OnScreen(px, py) && RETRO_DepthTest(py * RETRO_WIDTH + px, lamp.q)) {
				int step = (LAMP_SHADES - 1) * (1 - distance);
				unsigned char color = colored ? LampGlow(i, step) : RETRO_COLORS - LAMP_SHADES + step;
				RETRO_PutPixel(px, py, color);
			}
		}
	}
}

void DEMO_Render(RETRO_Time time)
{
	static int mode = MODE_GOURAUD;
	static bool colored = false;
	static bool textured = true;
	static bool bumped = true;

	// Handle keys
	if (RETRO_KeyPressed(SDL_SCANCODE_1)) {
		mode = MODE_FLAT;
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_2)) {
		mode = MODE_GOURAUD;
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_3)) {
		mode = MODE_PHONG;
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_C)) {
		colored = !colored;
		RETRO_SetPalette(colored ? ColorPalette : WhitePalette);
		for (int i = 0; i < LAMPS; i++) {
			Lighting.light[i].color = colored ? Lamps[i].color : vec3{ 1, 1, 1 };
		}
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_T)) {
		textured = !textured;
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_B)) {
		bumped = !bumped;
	}

	// The marble is shaded in its table's levels, the plain torus in its ramp
	Model3D *model = RETRO_Get3DModel();
	model->c = textured ? 0 : RETRO_PHONG_OFFSET;
	model->shades = textured ? RETRO_SHADE_TABLE_SHADES : TORUS_SHADES;
	model->colortable = !colored ? RETRO_ShadeTable{} : textured ? MarbleColorTable : ColorTable;
	model->bumpmap = bumped ? model->texmap : NULL; // the marble's relief, with it or without

	// Calculate rotation
	float ax = fmod(time.total * ROTATION_SPEED, 2 * M_PI);
	float ay = fmod(time.total * ROTATION_SPEED, 2 * M_PI);
	float az = fmod(time.total * ROTATION_SPEED, 2 * M_PI);

	// Move the lamps
	PlaceLamps(time.total);

	// Draw torus, then the lamps in front of it or behind
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel(PROJECTION_SCALE);
	RETRO_RenderModel(textured ? RETRO_POLY_TEXTURE : Modes[mode].rendertype, textured ? Modes[mode].textureshade : Modes[mode].shadertype);
	for (int i = 0; i < LAMPS; i++) {
		DrawLamp(i, colored);
	}

	// Draw mode
	char text[128];
	snprintf(text, sizeof(text), "%s, %s lamps%s\n1 flat, 2 gouraud, 3 phong\nc color, t marble, b bump", Modes[mode].name, colored ? "colored" : "white", textured ? (bumped ? ", bumped marble" : ", marble") : (bumped ? ", bumped" : ""));
	RETRO_PutString(text, 0, RETRO_HEIGHT - 26, colored ? COLOR_WHITE : RETRO_COLORS - 1);
}

void DEMO_Initialize(void)
{
	// Init the white palette: the matte ramp, and the lamps' glow above it
	RETRO_SetColor(0, RETRO_BLACK, WhitePalette);
	RETRO_CreatePhongPalette(RETRO_PHONG_OFFSET, RETRO_PHONG_OFFSET + TORUS_SHADES, TORUS_COLOR, 0.0f, RETRO_K_FALLOFF, WhitePalette);
	RETRO_CreateGradientPalette(RETRO_COLORS - LAMP_SHADES, RETRO_COLORS, LAMP_EDGE, RETRO_WHITE, WhitePalette);

	// Init marble: its grays along the torus's ramp
	RETRO_Image *marble = RETRO_LoadImage("assets/marble_256x256.pcx");
	RETRO_CreateShadeTable(marble->palette, WhitePalette, { MarbleShadeTable, RETRO_SHADE_TABLE_COLORS, RETRO_SHADE_TABLE_SHADES }, MarbleLight);

	// Init lamps
	Lighting.lights = LAMPS;
	for (int i = 0; i < LAMPS; i++) {
		Lighting.light[i].point = true;
		Lighting.light[i].strength = LAMP_STRENGTH;
		Lighting.light[i].constant = 1;
		Lighting.light[i].linear = LAMP_LINEAR;
	}

	Model3D *model = RETRO_Load3DModel("assets/torusquads.obj");
	model->c = RETRO_PHONG_OFFSET;
	model->shades = TORUS_SHADES;
	model->lighting = &Lighting;
	model->texmap = marble->data;
	model->shadetable = MarbleShadeTable;

	// Init the colored palette, and the color light tables in it: the plain
	// torus's white, and the marble's grays as materials as light as it at
	// their lightest
	RETRO_Palette materials[RETRO_SHADE_TABLE_COLORS];
	for (int i = 0; i < RETRO_SHADE_TABLE_COLORS; i++) {
		materials[i] = RETRO_ShadeColor(marble->palette[i], TORUS_WHITE / 255.0f);
	}
	FitColorPalette(marble, materials);
	RETRO_Palette white = { TORUS_WHITE, TORUS_WHITE, TORUS_WHITE };
	RETRO_CreateColorLightTable(&white, ColorPalette, ColorTable);
	RETRO_CreateColorLightTable(materials, ColorPalette, MarbleColorTable);

	RETRO_SetPalette(WhitePalette);
	RETRO_SetFont(RETRO_FONT_VGA_8X8);
}
