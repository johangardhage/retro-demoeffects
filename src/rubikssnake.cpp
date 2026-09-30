//
// Rubik's Snake
//
// A chain of PRISMS wedges, triangular prisms that are each half of a cube cut
// edge to edge, twisted from one shape into the next. A wedge has two square
// faces at right angles to each other and is joined to the wedge before it by
// one of them and to the wedge after it by the other. A joint turns about the
// middle of the square it joins, in quarter turns, and a shape is the number of
// quarter turns at every joint. With none at any joint the wedges point up and
// down by turns and the snake is a straight bar.
//
// A wedge is placed by where the one before it is. In the frame of a wedge,
// with the square it was joined by at x = 0 and the wedge on the far side of
// it, the square the next one is joined by is at y = -½, centered on (½, -½,
// 0). The next wedge has its own x axis along the way out of that square, its y
// axis back along this one's x, and is then turned about its x axis by the
// angle of the joint:
//
//   origin' = origin + M (½, -½, 0)
//   M'      = M J Rx(angle)
//
// with M the wedge's axes and J the three axes above. Going down the chain from
// the first wedge places them all.
//
// There are two ways of getting from one shape to the next, named in the corner
// of the screen, and Tab changes between them. The change is taken up the next
// time a shape is held, so a twist under way is finished the way it was begun.
//
// The careful way turns the joints one at a time, in an order laid down for
// each pair of shapes so that no wedge passes through another on the way. A
// joint can only turn while what hangs on either side of it swings clear of the
// rest, so where a joint that has to turn is shut in, others are opened first
// to let it out and closed again after. That is why some joints turn more than
// once, and why the snake passes through shapes that are neither the one it
// left nor the one it is making. A quarter turn takes TWIST_TIME and a half
// turn twice that.
//
// The quick way turns every joint the shorter way from the one number of
// quarter turns to the other, each over WAVE_TIME and each starting
// WAVE_STAGGER after the one before it, so the change runs down the snake from
// one end. Nothing keeps the wedges out of each other's way, and they pass
// through one another.
//
// The first wedge is the one that stays put, so everything after a turning
// joint swings around with it. The snake is therefore moved to stand with the
// mean of its corners on the middle of the screen, and scaled so the corner
// farthest from it is SCREEN_REACH pixels out, which lets the straight bar and
// the ball both fill the screen.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"
#include "lib/retrofont.h"

#define PRISMS 24
#define JOINTS (PRISMS - 1)
#define PRISM_VERTICES 6
#define PRISM_FACES 5
#define PRISM_SHADES ((RETRO_COLORS - 1) / 2) // palette entries a color's ramp covers
#define TEXT_COLOR (RETRO_COLORS - 1) // the entry past the two ramps
#define SCREEN_REACH 100.0f // pixels from the middle of the screen to the farthest corner
#define SHAPE_HOLD 2.5 // seconds a shape is held
#define TWIST_TIME 0.5 // seconds a joint takes to turn a quarter turn, the careful way
#define WAVE_TIME 2.0 // seconds a joint takes to turn, the quick way
#define WAVE_STAGGER 0.2 // seconds a joint waits for the one before it
#define ROTATION_SPEED_X 0.5 // radians a second
#define ROTATION_SPEED_Y 0.8
#define ROTATION_SPEED_Z 0.3

// Quarter turns at each joint, from the first to the last
static const char *Shapes[] = {
	"00000000000000000000000", // bar
	"13113133131131331311313", // ball
	"01011201011201011201011", // ring
	"01233201233201233201233", // flower
	"01311301311301311301311", // star
	"02012002012002012002012", // cross
	"12121212121212121212121", // stairs
	"11132311132311132311132", // wreath
};

#define NUM_SHAPES (int)(sizeof(Shapes) / sizeof(Shapes[0]))

// The twists that take each shape to the one after it, and the last back to
// the first, in the order they are made: the joint, a for the first, then + or
// - for a quarter turn one way or the other, twice over for a half turn
static const char *Twists[NUM_SHAPES] = {
	"w-v+u-t+r-q+p-o-n+m-k+l+j-i+g-h-f+e-c+a+d+b+b++u++s+u++",
	"u--s-u--b--a-e++c-h++g+i-j--m+o+p--r-u+w--l+f+",
	"v++w++u++j--d++r++q--r--p++l++k++l++o++f++i++e++f--c++",
	"u+w++r+v++o+q++p++l+i+k++f+j++c+e++d++",
	"i+c+f+l+o+r+u+w+t+n+h+b+e+k-q+k--",
	"b++c+a+b--h++i+g+h--n++o+m+n--t--u+s+t--e-q-p+k-w-v+r++l++j+f--d+",
	"b-h-n-t-v+w+r+l+d+f--e-f-e--p--q-p-q--j--k-j-k--",
	"k++a-b-c-e++d+f+g-h-i-j+l+m-n-o-q++p+r+s-t-u-v+w++",
};

static double TwistTime[NUM_SHAPES]; // seconds the twists out of each shape take
static bool Careful = true; // the way the next shape is to be made

// The corners of a wedge in its own frame: the triangle at z = -½, then at ½
static const vec3 PrismCorners[PRISM_VERTICES] = {
	{ 0, -0.5f, -0.5f }, { 0, 0.5f, -0.5f }, { 1, -0.5f, -0.5f },
	{ 0, -0.5f, 0.5f }, { 0, 0.5f, 0.5f }, { 1, -0.5f, 0.5f },
};

// Its faces, wound to look out of it: the two triangles, the square it is
// joined by, the square the next is joined by, and the slope
static const int PrismFaces[PRISM_FACES][4] = {
	{ 0, 1, 2, -1 }, { 3, 5, 4, -1 }, { 0, 3, 4, 1 }, { 0, 2, 5, 3 }, { 1, 4, 5, 2 },
};

// The axes of a wedge in the frame of the one before it, with the joint at rest
static const mat3 JointRest = { { 0, -1, 0 }, { -1, 0, 0 }, { 0, 0, -1 } };

void DEMO_Render(double time, double deltatime)
{
	// Calculate rotation
	float ax = fmod(time * ROTATION_SPEED_X, 2 * M_PI);
	float ay = fmod(time * ROTATION_SPEED_Y, 2 * M_PI);
	float az = fmod(time * ROTATION_SPEED_Z, 2 * M_PI);

	if (RETRO_KeyPressed(SDL_SCANCODE_TAB)) {
		Careful = !Careful;
	}

	// Calculate phase: the shape the snake is leaving, and how long ago it made it
	static double phase = 0;
	static int iphase = 0;
	static bool careful = true;
	phase += deltatime;
	double twisttime = careful ? TwistTime[iphase] : WAVE_TIME + (JOINTS - 1) * WAVE_STAGGER;
	if (phase >= SHAPE_HOLD + twisttime) {
		phase -= SHAPE_HOLD + twisttime;
		iphase = (iphase + 1) % NUM_SHAPES;
	}
	if (phase < SHAPE_HOLD) {
		careful = Careful;
	}

	float quarters[JOINTS];
	for (int i = 0; i < JOINTS; i++) {
		quarters[i] = Shapes[iphase][i] - '0';
	}

	if (careful) {
		// Turn the joints one after the other, as the twists are written
		double start = SHAPE_HOLD;
		for (const char *twist = Twists[iphase]; *twist;) {
			int joint = *twist++ - 'a';
			int turns = 0;
			while (*twist == '+' || *twist == '-') {
				turns += *twist++ == '+' ? 1 : -1;
			}
			double duration = abs(turns) * TWIST_TIME;

			quarters[joint] += turns * smoothstep(start, start + duration, phase);
			start += duration;
		}
	} else {
		// Turn every joint the shorter way, -1, 0, 1 or 2 quarter turns, each
		// starting a little after the one before it
		const char *from = Shapes[iphase];
		const char *to = Shapes[(iphase + 1) % NUM_SHAPES];
		for (int i = 0; i < JOINTS; i++) {
			int turns = (to[i] - from[i] + 5) % 4 - 1;
			double start = SHAPE_HOLD + i * WAVE_STAGGER;

			quarters[i] += turns * smoothstep(start, start + WAVE_TIME, phase);
		}
	}

	// Place the wedges, each from the one before it
	Model3D *model = RETRO_Get3DModel();
	mat3 matrix = identity();
	vec3 origin = { 0, 0, 0 };
	for (int i = 0; i < PRISMS; i++) {
		if (i > 0) {
			origin += matrix * vec3{ 0.5f, -0.5f, 0 };
			matrix = matrix * JointRest * rotateX(quarters[i - 1] * M_PI / 2);
		}
		for (int j = 0; j < PRISM_VERTICES; j++) {
			model->vertex[i * PRISM_VERTICES + j].pos = origin + matrix * PrismCorners[j];
		}
	}

	// Center the snake on the mean of its corners, and find how far it reaches
	vec3 center = { 0, 0, 0 };
	for (int i = 0; i < model->vertices; i++) {
		center += model->vertex[i].pos;
	}
	center /= model->vertices;

	float reach = 0;
	for (int i = 0; i < model->vertices; i++) {
		model->vertex[i].pos -= center;
		reach = MAX(reach, length(model->vertex[i].pos));
	}

	// The faces have moved, so their normals have to be found again
	RETRO_InitializeFaceNormals();

	// Draw snake
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel(SCREEN_REACH / reach);
	RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_FLAT);

	// Draw state: the way in use, and the way waiting to be taken up if Tab has
	// changed it since
	RETRO_PutString(careful ? "CAREFUL" : "QUICK", 10, 10, TEXT_COLOR);
	if (Careful != careful) {
		RETRO_PutString(Careful ? "NEXT CAREFUL" : "NEXT QUICK", 10, 20, TEXT_COLOR);
	}
	RETRO_PutString("TAB MODE", 10, 222, TEXT_COLOR);
}

void DEMO_Initialize(void)
{
	// Init palette, one ramp per color of the wedges
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreateGradientPalette(1, 1 + PRISM_SHADES, RETRO_BLUEBLACK, RETRO_AZURE);
	RETRO_CreateGradientPalette(1 + PRISM_SHADES, 1 + 2 * PRISM_SHADES, RETRO_BLUEBLACK, RETRO_WHITE);
	RETRO_SetColor(TEXT_COLOR, RETRO_WHITE);

	Model3D *model = RETRO_Allocate3DModel();
	model->c = 1;
	model->shades = PRISM_SHADES;
	model->vertices = PRISMS * PRISM_VERTICES;
	model->faces = PRISMS * PRISM_FACES;

	// The wedges are the two colors by turns
	for (int i = 0; i < PRISMS; i++) {
		for (int j = 0; j < PRISM_FACES; j++) {
			Face *face = &model->face[i * PRISM_FACES + j];
			face->vertices = PrismFaces[j][3] < 0 ? 3 : 4;
			for (int k = 0; k < face->vertices; k++) {
				face->vertex[k] = i * PRISM_VERTICES + PrismFaces[j][k];
			}
			face->c = i % 2 * PRISM_SHADES;
		}
	}

	// A + or a - is a quarter turn
	for (int i = 0; i < NUM_SHAPES; i++) {
		for (const char *twist = Twists[i]; *twist; twist++) {
			if (*twist == '+' || *twist == '-') {
				TwistTime[i] += TWIST_TIME;
			}
		}
	}

	RETRO_InitializeLightSource(-0.4, -0.5, -0.77);
}
