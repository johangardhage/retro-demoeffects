//
// Classic Amiga rubber vector
//
// The cube itself never bends.  One rigid cube is rendered per simulation step
// and each of its scanlines is packed into a few colored spans.  Old scanlines
// are retained in a ring buffer, then the displayed picture is assembled one
// line at a time: a travelling sine chooses how old the source line is.
// Straight polygon edges therefore appear curved even though every source
// image contains an ordinary six-face cube.
//
// The ring is a number of steps deep rather than a number of seconds, so the
// cubes are produced in DEMO_Update at the fixed simulation rate.  Producing
// one per displayed frame instead would hand the amount of bend to the refresh
// rate: the same 24 copies span 0.4s at 60Hz and 0.17s at 144Hz, so the cube
// shreds into disconnected slabs on a slow display and flattens toward a rigid
// cube on a fast one.
//
// A face takes the ramp of the axis it faces.  Opposite faces share a ramp and
// a convex cube never shows both of a pair, so the three faces on screen are
// always three different colors and the multiplexing bends three distinct
// bands rather than one silhouette.  Every ramp is matte and starts at black,
// which disposes of the flat renderer's lower clamp at model->c: a face turned
// away from the light goes dark rather than picking up the first ramp's hue.
//
// This reproduces the characteristic scanline-multiplexed Amiga effect rather
// than deforming a subdivided mesh as rubbervector2.cpp does.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

#define RUBBER_COPIES 24     // retained cube images, one per simulation step: 0.4s of rotation
#define ROTATION_SPEED 1.1f  // cube rotation in radians per second
#define RUBBER_SPEED 2.2f    // vertical selection wave speed
#define RUBBER_WAVES 1.75f   // waves over the height of the screen
#define MAX_LINE_SPANS 6     // a projected convex cube normally needs at most three
#define RUBBER_RAMP 1        // where the first ramp starts, past the background
#define RUBBER_SHADES ((RETRO_COLORS - RUBBER_RAMP) / 3) // palette entries a face color's ramp
                             // covers, the three sharing everything past the background

static Model3D *Cube;

struct RubberSpan {
	unsigned short left;
	unsigned short right;
	unsigned char color;
};

struct RubberLine {
	unsigned char spans;
	RubberSpan span[MAX_LINE_SPANS];
};

static RubberLine LineHistory[RUBBER_COPIES][RETRO_HEIGHT];
static int HistoryHead;

static void PackCubeImage(RubberLine *image, const unsigned char *source)
{
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		RubberLine &line = image[y];
		line.spans = 0;
		int x = 0;

		while (x < RETRO_WIDTH) {
			unsigned char color = source[y * RETRO_WIDTH + x];
			int left = x++;
			while (x < RETRO_WIDTH && source[y * RETRO_WIDTH + x] == color) x++;

			// Background is implicit.  A convex cube produces only a handful of
			// nonzero runs even where several differently shaded faces meet.
			if (color != 0 && line.spans < MAX_LINE_SPANS) {
				RubberSpan &span = line.span[line.spans++];
				span.left = left;
				span.right = x;
				span.color = color;
			}
		}
	}
}

//
// Render the cube at one pose and retain its compact scanline spans rather than
// the complete chunky image
//
// The framebuffer is scratch space for this: a step hands its cube to the ring
// and nothing else, and DEMO_Render is given a cleared framebuffer, so none of
// what is drawn here is ever displayed.
//
static void RetainCubeImage(float ax, float ay, float az, RubberLine *image)
{
	RETRO_Clear();
	RETRO_RotateModel(ax, ay, az, Cube);
	RETRO_ProjectModel(RETRO_PROJECTION_SCALE, RETRO_WIDTH / 2, RETRO_HEIGHT / 2, Cube);
	RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_FLAT, Cube);
	PackCubeImage(image, RETRO_FrameBuffer());
}

//
// Retain one rigid cube image per fixed step, so the ring holds the same
// stretch of the cube's rotation whatever the display is doing
//
void DEMO_Update(double deltatime)
{
	static float ax, ay, az;
	ax = fmod(ax + deltatime * ROTATION_SPEED, 2 * M_PI);
	ay = fmod(ay + deltatime * ROTATION_SPEED * 1.17f, 2 * M_PI);
	az = fmod(az + deltatime * ROTATION_SPEED * 0.61f, 2 * M_PI);

	// The head is the newest image rather than the next slot to fill, so it
	// stands still between steps and DEMO_Render can read the ring on a frame
	// that earned no step at all
	HistoryHead = (HistoryHead + 1) % RUBBER_COPIES;

	RetainCubeImage(ax, ay, az, LineHistory[HistoryHead]);
}

void DEMO_Render(double deltatime)
{
	// The selection wave runs on displayed time, so it stays smooth on a fast
	// display even though the images it selects between arrive at the step rate
	static float phase;
	phase = fmod(phase + deltatime * RUBBER_SPEED, 2 * M_PI);

	unsigned char *buffer = RETRO_FrameBuffer();

	// Multiplex the retained copies by scanline.  Quantizing the sine to an
	// image age is intentional: every line comes wholly from one retained cube
	// image, with age zero selecting the newest.
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float wave = phase + y * RUBBER_WAVES * 2.0f * M_PI / RETRO_HEIGHT;
		float agechoice = (sin(wave) + 1.0f) * 0.5f;
		int age = CLAMP(agechoice * RUBBER_COPIES, 0, RUBBER_COPIES);
		int source = WRAP(HistoryHead - age, RUBBER_COPIES);
		const RubberLine &line = LineHistory[source][y];
		for (int i = 0; i < line.spans; i++) {
			const RubberSpan &span = line.span[i];
			memset(buffer + y * RETRO_WIDTH + span.left, span.color, span.right - span.left);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette.  One ramp per axis of the cube, each out of the black it sits
	// against.  Matte, for the reason RETRO_CreateMattePalette gives: a flat lit
	// face has one normal for all of it, so a specular highlight would flash the
	// whole face at once
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreatePhongPalette(RUBBER_RAMP, RUBBER_RAMP + RUBBER_SHADES, RETRO_DEEPPINK, 0.0);
	RETRO_CreatePhongPalette(RUBBER_RAMP + RUBBER_SHADES, RUBBER_RAMP + 2 * RUBBER_SHADES, RETRO_AZURE, 0.0);
	RETRO_CreatePhongPalette(RUBBER_RAMP + 2 * RUBBER_SHADES, RUBBER_RAMP + 3 * RUBBER_SHADES, RETRO_GOLD, 0.0);

	Cube = RETRO_Load3DModel("assets/cubequads.obj");
	Cube->c = RUBBER_RAMP;
	Cube->cintensity = RUBBER_SHADES - 1;

	// The ramp a face is shaded in is the one of the axis it faces
	for (int i = 0; i < Cube->faces; i++) {
		float x = fabs(Cube->face[i].facenormal.nx);
		float y = fabs(Cube->face[i].facenormal.ny);
		float z = fabs(Cube->face[i].facenormal.nz);
		Cube->face[i].c = (x > y && x > z ? 0 : (y > z ? 1 : 2)) * RUBBER_SHADES;
	}

	// Head on, as the other flat shaded cubes have it.  The three faces on screen
	// are already told apart by their ramps, so the light is left to shade them
	// rather than to separate them
	RETRO_InitializeLightSource(0, 0, -1);

	// Fill the ring with the cube at rest, so the first displayed frame has a
	// full history to multiplex rather than a black trail.  From the first step
	// onward each slot is replaced naturally as the ring advances.
	RetainCubeImage(0, 0, 0, LineHistory[0]);
	for (int i = 1; i < RUBBER_COPIES; i++) memcpy(LineHistory[i], LineHistory[0], sizeof(LineHistory[i]));
}
