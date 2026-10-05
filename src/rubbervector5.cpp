//
// Vector slime
//
// The multiplexed rubber cube of rubbervector4, cut finer. Rigid cube images
// are retained in a ring, one per simulation step, but the displayed picture is
// assembled from narrow vertical strips rather than whole scanlines. Two sines
// traveling down the screen choose how old each row's source image is, and a
// third traveling across it adds a little more age strip by strip, so the
// cube wobbles sideways as well as bending from top to bottom.
//
// The images are kept whole rather than packed into spans as rubbervector4
// packs them, since a strip needs every pixel of its source line.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"

#define SLIME_HISTORY 0.57      // seconds of rotation retained, one cube image per simulation step
#define SLIME_STRIP 4            // pixels across a strip, each copied from one image
#define ROTATION_SPEED 0.43f     // pitch in radians per second; roll turns twice and yaw four times as fast
#define ROW_REACH 0.51f          // seconds back the two row waves reach together
#define ROW_PHASE_A 5.22f        // radians each wave has reached at the middle of the screen when the demo starts
#define ROW_PHASE_B 2.42f
#define STRIP_PHASE 2.60f
#define ROW_SPEED_A 1.29f        // first vertical selection wave speed
#define ROW_SPEED_B -0.43f       // second vertical selection wave speed, running the other way
#define ROW_WAVES_A 2.04f        // first wave's waves over the height of the screen
#define ROW_WAVES_B 1.56f        // second wave's waves over the height of the screen
#define STRIP_REACH 0.047f       // seconds back the horizontal wave reaches
#define STRIP_SPEED 2.15f        // horizontal selection wave speed
#define STRIP_WAVES 2.8f         // waves over the width of the screen
#define SLIME_AMBIENT 16         // shade every face starts from, however it faces the light
#define SLIME_SHADES 63          // entries in the cube's ramp, the top of the DAC's six-bit scale

#define SLIME_COPIES (int)(SLIME_HISTORY / RETRO_SIMULATION_STEP + 1)

static unsigned char History[SLIME_COPIES][RETRO_HEIGHT][RETRO_WIDTH];
static int HistoryHead;

//
// Render the cube at one pose and retain the whole image
//
// The framebuffer is scratch space for this, as in rubbervector4: DEMO_Render
// is given a cleared framebuffer, so none of what is drawn here is displayed.
//
static void RetainCubeImage(float angle, unsigned char *image)
{
	RETRO_Clear();
	// Yaw outermost, as in the original tumble. RETRO_RotateModel(ax, ay, az)
	// turns the axes in the other order and gives a different motion
	RETRO_RotateModel(rotateY(4 * angle) * rotateX(angle) * rotateZ(2 * angle));
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_FLAT);
	memcpy(image, RETRO_FrameBuffer(), RETRO_WIDTH * RETRO_HEIGHT);
}

void DEMO_FixedUpdate(RETRO_Time time)
{
	static float angle;
	angle = fmod(angle + time.delta * ROTATION_SPEED, 2 * M_PI);

	// The head is the newest image, as in rubbervector4
	HistoryHead = (HistoryHead + 1) % SLIME_COPIES;

	RetainCubeImage(angle, History[HistoryHead][0]);
}

void DEMO_Render(RETRO_Time time)
{
	// The selection waves run on displayed time, so they stay smooth on a fast
	// display even though the images they select between arrive at the step rate
	float phasea = fmod(ROW_PHASE_A + time.total * ROW_SPEED_A, 2 * M_PI);
	float phaseb = fmod(ROW_PHASE_B + time.total * ROW_SPEED_B, 2 * M_PI);
	float phasestrip = fmod(STRIP_PHASE + time.total * STRIP_SPEED, 2 * M_PI);

	unsigned char *buffer = RETRO_FrameBuffer();

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float wavea = sin(phasea + (y - RETRO_HEIGHT / 2.0) * ROW_WAVES_A * 2 * M_PI / RETRO_HEIGHT);
		float waveb = sin(phaseb + (y - RETRO_HEIGHT / 2.0) * ROW_WAVES_B * 2 * M_PI / RETRO_HEIGHT);
		// A crest selects the newest image, a trough the oldest
		float rowage = (2 - wavea - waveb) / 4 * ROW_REACH;
		for (int x = 0; x < RETRO_WIDTH; x += SLIME_STRIP) {
			float wavestrip = sin(phasestrip + (x - RETRO_WIDTH / 2.0) * STRIP_WAVES * 2 * M_PI / RETRO_WIDTH);
			float stripage = (1 - wavestrip) / 2 * STRIP_REACH;
			int age = CLAMP((rowage + stripage) / RETRO_SIMULATION_STEP, 0, SLIME_COPIES);
			int source = WRAP(HistoryHead - age, SLIME_COPIES);
			memcpy(buffer + y * RETRO_WIDTH + x, History[source][y] + x, SLIME_STRIP);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette, on the DAC's six-bit scale. The cube's ramp turns from
	// blue in the darks to pink in the lights, since red grows with the square
	// of the shade while blue grows with the shade itself
	for (int i = 0; i < SLIME_SHADES + 1; i++) {
		RETRO_Set6bitColor(i, i * i / 63, i * 31 / 45, i);
	}

	Model3D *model = RETRO_Load3DModel("assets/cubequads.obj");
	model->c = SLIME_AMBIENT;
	model->shades = SLIME_SHADES - SLIME_AMBIENT;

	// From above left and in front, so the three faces on screen are told
	// apart and the bend can be followed across them
	RETRO_InitializeLightSource(-1, -1, -2);

	// Fill the ring with the cube at rest, so the first displayed frame has a
	// full history to multiplex rather than a black trail
	RetainCubeImage(0, History[0][0]);
	for (int i = 1; i < SLIME_COPIES; i++) memcpy(History[i], History[0], sizeof(History[i]));
}
