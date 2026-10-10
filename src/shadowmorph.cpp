//
// Shadow morph
//
// A bronze cat inside two crossed hoops, with a camera flying around them.
// Both are RETRO_POLY_MATCAP meshes lit by 128x128 lighting maps. The cat's
// map is rebuilt each frame from the bronze with one of thirty-two shadow
// masks laid over it, picked by the camera's angles: black in the mask moves
// the bronze 64 entries down the palette, onto its dark copy. The cat also has
// a bump whose strength comes and goes, and a palette morph turns the bronze
// into steel and back.
//
// A keyframe table gives the camera's position and pitch, the bump strength
// and the morph, and each frame mixes the two keyframes it falls between. The
// last keyframe is the first again, so the loop closes. The yaw turns
// steadily, YAW_TURNS times a loop, and the cat CAT_TURNS more on top of it.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromodel.h"
#include "lib/retromatrix.h"
#include "lib/retrorender.h"

#define YAW_TURNS 8                        // the camera's turns in a loop
#define CAT_TURNS -10                      // the cat's own turns in a loop, on top of the camera's
#define FOCAL 437.5f                       // the eye's distance from the screen, in pixels
#define MAP_SIZE 128                       // the lighting maps' width and height
#define MAP_RADIUS 54                      // the lighting maps' reach from the middle, in texels
#define SHADOW_OFFSET 64                   // the dark bronze is 64 entries past the lit
#define BUMP_GRAZING 293.0f                // the bump's grazing height at full strength

// Where the camera is, its pitch in degrees, the bump strength from 0 for
// none to 1 for full, and the morph from 0 for bronze to 1 for steel, at a
// time. Between two keyframes everything moves in a straight line.
struct Keyframe {
	float time;
	vec3 cam;
	float pitch;
	float bump;
	float morph;
};

static const Keyframe Keyframes[] = {
	{ 0.0f, { -5.875f, 0, 9.277f }, 0.0f, 1.0f, 0.0f },
	{ 4.0f, { -1.603f, 0, 4.684f }, -6.2f, 1.0f, 0.0f },
	{ 6.0f, { -0.267f, 0, 4.684f }, -9.2f, 1.0f, 0.0f },
	{ 10.0f, { -0.267f, 0, 7.889f }, -15.4f, 1.0f, 0.0f },
	{ 13.0f, { -0.267f, 0, 7.889f }, -20.0f, 1.0f, 0.0f },
	{ 20.0f, { -0.267f, 0, 3.216f }, -57.7f, 1.0f, 0.0f },
	{ 24.0f, { -0.267f, 0, 3.216f }, -36.1f, 1.0f, 0.0f },
	{ 30.0f, { -0.267f, 0, 8.022f }, -45.4f, 1.0f, 0.0f },
	{ 33.0f, { -0.267f, 0, 6.821f }, -26.9f, 1.0f, 0.0f },
	{ 33.9f, { -0.267f, 0, 6.340f }, -29.7f, 0.0f, 0.246f },
	{ 36.7f, { -0.267f, 0, 4.845f }, -38.3f, 0.0f, 1.0f },
	{ 39.0f, { -0.267f, 0, 3.616f }, -45.4f, 0.0f, 1.0f },
	{ 39.6f, { -0.267f, 0, 3.648f }, -45.4f, 0.984f, 0.754f },
	{ 41.4f, { -0.267f, 0, 3.745f }, -45.4f, 0.984f, 0.0f },
	{ 43.0f, { -0.267f, 0, 3.830f }, -45.4f, 0.984f, 0.0f },
	{ 43.6f, { -0.267f, 0, 3.830f }, -45.4f, 0.0f, 0.244f },
	{ 45.4f, { -0.267f, 0, 3.830f }, -45.4f, 0.0f, 1.0f },
	{ 47.0f, { -0.267f, 0, 3.830f }, -45.4f, 0.0f, 1.0f },
	{ 49.0f, { -0.267f, 0, 3.830f }, -48.4f, 0.0f, 1.0f },
	{ 58.5f, { -5.875f, 0, 9.277f }, 0.0f, 1.0f, 0.0f },
};

#define KEYFRAMES (int)(sizeof(Keyframes) / sizeof(Keyframes[0]))

static Model3D *Cat;
static Model3D *Hoops;
static RETRO_Image *Background;
static RETRO_Image *BronzeMap;
static RETRO_Image *HoopMap;
static RETRO_Image *Masks;
static RETRO_Image *HeightField;
static unsigned char CatMap[MAP_SIZE * MAP_SIZE];

// The model given a quarter turn about z, x taking y's place, then turned by
// the yaw, the pitch and a quarter turn of roll, carried across by the
// camera's x and y, projected with the eye FOCAL pixels from the screen, and
// drawn. The first model drawn in a frame clears the depth buffer.
static void DrawObject(Model3D *model, float yaw, float pitch, vec3 cam, bool cleardepth)
{
	RETRO_RotateModel(rotate(yaw, pitch, (float)M_PI / 2) * rotateZ(0, -1), model);
	RETRO_TranslateModel(-cam.x, -cam.y, 0, model);
	RETRO_ProjectModel(FOCAL / cam.z, RETRO_WIDTH / 2.0f, RETRO_HEIGHT / 2.0f, model, FOCAL);
	RETRO_RenderModel(RETRO_POLY_MATCAP, RETRO_SHADE_NONE, model, cleardepth);
}

// The lit bronze, with the dark bronze where the mask is black. The mask is
// picked by the camera's own yaw and pitch, not the cat's, whose turn goes only
// into its mesh. The masks run with the yaw backwards over a quarter turn, and
// once the pitch has passed a quarter turn, and not yet three, the yaw runs
// forwards instead, so the bank covers the camera either way up. This track's
// pitch stays below that, so the yaw always runs backwards here.
static void ShadeCatMap(float yaw, float pitch)
{
	const float quarter = (float)M_PI / 2;
	float angle = -yaw;
	if (mod(pitch - quarter, 4 * quarter) < 2 * quarter) {
		angle = yaw;
	}
	const int size = Masks->width;
	const int frames = Masks->height / size;
	int frame = MIN((int)(mod(angle, quarter) / quarter * frames), frames - 1);
	const unsigned char *mask = Masks->data + frame * size * size;
	const int scale = MAP_SIZE / size;
	for (int v = 0; v < MAP_SIZE; v++) {
		for (int u = 0; u < MAP_SIZE; u++) {
			int texel = v * MAP_SIZE + u;
			int shadow = mask[v / scale * size + u / scale] == 0;
			CatMap[texel] = BronzeMap->data[texel] + (shadow ? SHADOW_OFFSET : 0);
		}
	}
}

void DEMO_Render(RETRO_Time time)
{
	// The two keyframes the time falls between, and how far it is between them
	const double length = Keyframes[KEYFRAMES - 1].time;
	double seconds = mod(time.total, length);
	int key = 0;
	while (key < KEYFRAMES - 2 && seconds >= Keyframes[key + 1].time) {
		key++;
	}
	const Keyframe &from = Keyframes[key];
	const Keyframe &to = Keyframes[key + 1];
	float t = CLAMP01((float)((seconds - from.time) / (to.time - from.time)));

	// The bronze palette mixed toward the steel one, which the hoop map carries
	float morph = mix(from.morph, to.morph, t);
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, mix(BronzeMap->palette[i], HoopMap->palette[i], morph));
	}

	RETRO_Blit(Background->data);

	// The camera and its pitch come from the keyframes. The yaw turns steadily,
	// YAW_TURNS times a loop, and the cat's yaw turns CAT_TURNS more on top of
	// it, so both come round whole at the end of the loop.
	double loop = seconds / length;
	vec3 cam = mix(from.cam, to.cam, t);
	float yaw = (float)(2 * M_PI * mod(loop * YAW_TURNS, 1.0));
	float pitch = (float)radians(mix(from.pitch, to.pitch, t));
	float catturn = (float)(2 * M_PI * mod(loop * (YAW_TURNS + CAT_TURNS), 1.0));

	ShadeCatMap(yaw, pitch);

	// The bump strength scales the height field's tilt, which the library sets
	// by the grazing height, so the grazing height goes as one over the
	// strength. A strength under 1/256 is no bump at all.
	float bump = mix(from.bump, to.bump, t);
	Cat->bumpmap = NULL;
	if (bump >= 1.0f / 256) {
		Cat->bumpmap = HeightField->data;
		Cat->bumpgrazing = (int)(BUMP_GRAZING / bump + 0.5f);
	}

	DrawObject(Cat, catturn, pitch, cam, true);
	DrawObject(Hoops, yaw, pitch, cam, false);
}

void DEMO_Initialize(void)
{
	Background = RETRO_LoadImage("assets/blueswirl_320x240.pcx");
	BronzeMap = RETRO_LoadImage("assets/shadowmorph_bronzemap_128x128.pcx");
	HoopMap = RETRO_LoadImage("assets/shadowmorph_hoopmap_128x128.pcx");
	Masks = RETRO_LoadImage("assets/shadowmorph_masks_64x2048.pcx");
	HeightField = RETRO_LoadImage("assets/bumpheight_256x256.pcx");

	Cat = RETRO_Load3DModel("assets/cat.obj");
	Cat->envmap = CatMap;
	Cat->envmapwidth = MAP_SIZE;
	Cat->envmapheight = MAP_SIZE;
	Cat->envmapradius = MAP_RADIUS;

	Hoops = RETRO_Load3DModel("assets/hoops.obj");
	Hoops->envmap = HoopMap->data;
	Hoops->envmapwidth = MAP_SIZE;
	Hoops->envmapheight = MAP_SIZE;
	Hoops->envmapradius = MAP_RADIUS;
	Hoops->twosided = true;
}
