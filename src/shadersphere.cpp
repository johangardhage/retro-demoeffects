//
// Mirror sphere
//
// A mirror ball orbiting over a checker floor under an open sky, the room of
// shadercube.cpp. The ball is RETRO_POLY_SHADER with its vertex normals
// interpolated, and ShadeMirror reflects each pixel's view ray about its own
// normal, at its own point, and traces the room from there, so the floor under
// the ball is the floor the ball stands over, not a map baked from its centre.
// See matcapcube.cpp's RETRO_POLY_MATCAP for the canned-highlight technique
// this is not.
//
// ShadeRoom is the whole room, and draws the background as well as the
// reflections, so what the ball reflects is the room behind it. A ray is
// traced from where it leaves the eye or the mirror:
//
//   Ry > 0 (down)  floor at t = (ROOM_FLOOR - Oy) / Ry, a box-filtered checker
//                  in the ball's shadow or not
//   Ry ≤ 0 (up)    sky, from CHROME_HORIZON at the line to midnight blue at
//                  the zenith on sqrt(-Ry), so the pale blue stays a thin line,
//                  whitened by the clouds and the sun
//
// The sun stands high behind the camera, so the ball's shadow falls away
// from it, and the sun itself is seen in the ball, a highlight on the side
// turned back towards it. A sphere's shadow needs no tracing: a ray towards
// the sun is in it by how far it misses the centre, softened over a band that
// widens with the distance to the ball, as a real penumbra does. It moves
// with the ball, so the room is drawn afresh every frame.
//
// The clouds are value noise on a plane CLOUD_HEIGHT up, drifting along x.
// Near the horizon the plane is so foreshortened that a pixel spans many
// clouds, so they thin out there by the same footprint as the checker,
// instead of shimmering.
//
// The palette is two tables. The floor is a grey ramp, the dark square
// FLOOR_DARK of the way up and the shadow a darkening of either, hazed into
// the horizon a level at a time. The sky is its ramp, each shade whitened a
// level at a time; a cloud or the sun is a whiteness, so neither needs a
// ramp of its own. Both fit the palette only at a few levels each, so the
// cloud edges step, which suits the look.
//
// The checker is filtered over one screen pixel's footprint on the floor.
// Seen straight from the eye that is a pixel's angle times the distance,
// stretched by 1 / sin of the angle it meets the floor at. The ball is a
// convex mirror and fans a pixel's rays out much further, most of all near
// its rim, where a pixel covers a long strip of the surface; see ShadeMirror. Past
// half a square the average is a flat gray, so from there the floor fades
// into the horizon colour instead, and the far floor meets the sky without a
// seam.
//
// The camera looks CAMERA_PITCH down onto the ball. The library's camera
// looks straight along z, so the room is tilted up instead, by turning every
// ray into the room's frame; that is the same picture. The ball is carried
// back out of that frame into view space to be drawn.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define ROOM_FLOOR 1.8f // room units the floor lies below the ball's centre; the ball's radius is 1
#define ROOM_TILE 1.0f // room units a checker square spans
#define ROOM_PIXEL (1.0f / RETRO_PROJECTION_EYEDISTANCE) // radians one screen pixel spans at the eye
#define ROOM_EYE ((float)RETRO_PROJECTION_EYEDISTANCE / RETRO_PROJECTION_SCALE) // room units from the eye to the middle of the orbit
#define CAMERA_PITCH 0.35f // radians the view looks down onto the ball

#define SUN_DIRECTION vec3{ 0.45f, -0.8f, -0.4f } // towards the sun, in the room: right, up (-y) and behind the camera (-z)
#define SUN_RADIUS 0.045f // radians, the disc as seen
#define SUN_SOFTNESS 0.04f // radians, the light's disc as the shadow sees it; wider than the sun, for a softer shadow
#define SHADOW_DEPTH 0.6f // how much of the light the shadow takes away

#define CLOUD_HEIGHT 14.0f // room units the cloud plane lies above the ball's centre
#define CLOUD_SCALE 9.0f // room units a noise cell spans
#define CLOUD_COVER 0.5f // noise level where a cloud begins; higher is clearer sky
#define CLOUD_SPEED 0.6f // room units a second the clouds drift along x
#define CLOUD_FOOTPRINT 0.3f // noise cells a pixel spans where the clouds have thinned out; they start to at half this

#define FLOOR_DARK 0.18f // the dark square's brightness, of white's

#define CHROME_FLOOR_SHADES 16 // black through white, shadowed or not
#define CHROME_HAZE_LEVELS 6 // steps from the clear checker to the horizon colour
#define CHROME_HAZE_FOOTPRINT 0.5f // squares a pixel covers before the checker averages out and is hazed over; fully at twice this
#define CHROME_SKY_SHADES 32 // horizon to zenith
#define CHROME_WHITE_LEVELS 5 // steps from clear sky to white cloud or sun; 16 x 6 + 32 x 5 is the whole palette

#define BALL_RADIUS 1.0f // spherequads.obj's
#define ORBIT_SPEED 0.7f // radians a second
#define ORBIT_RADIUS 1.6f // room units from the middle of the orbit to the ball's centre

#define CHROME_HORIZON RETRO_Palette{ 168, 198, 224 } // pale blue at Ry = 0, where the hazed floor meets the sky

#define CHROME_FLOOR_START 0
#define CHROME_SKY_START (CHROME_FLOOR_START + CHROME_FLOOR_SHADES * CHROME_HAZE_LEVELS)

static mat3 RoomFrame; // view space to the room's, pitched so the camera looks down
static vec3 Sun; // unit, towards the sun, in the room
static vec3 Ball; // the ball's centre, in the room; set every frame
static float CloudDrift; // room units the clouds have moved along x

// abs(fract(x) - 1/2), the distance from x to the centre of its unit cell, folded
static float ChromeFold(float x)
{
	float f = x - floor(x);
	return fabs(f - 0.5f);
}

// Integral of one checker axis across a box of width w, in tile units
static float ChromeBox(float p, float w)
{
	w = MAX(w, 1.0e-4f);
	return 2.0f * (ChromeFold((p - 0.5f * w) * 0.5f) - ChromeFold((p + 0.5f * w) * 0.5f)) / w;
}

// 0 is a dark square, 1 a white one. w is the filter footprint in tile units,
// so a footprint wider than a square returns the gray in between.
static float ChromeChecker(float x, float z, float wx, float wz)
{
	return 0.5f - 0.5f * ChromeBox(x, wx) * ChromeBox(z, wz);
}

// The noise lattice's value at a grid point, in [0, 1]
static float Lattice(int x, int y)
{
	return (RETRO_Hash(x, y) & 65535u) / 65535.0f;
}

static float Noise(float x, float y)
{
	int ix = (int)floorf(x), iy = (int)floorf(y);
	float fx = smoothstep(0.0f, 1.0f, x - ix);
	float fy = smoothstep(0.0f, 1.0f, y - iy);
	float a = Lattice(ix, iy), b = Lattice(ix + 1, iy);
	float c = Lattice(ix, iy + 1), d = Lattice(ix + 1, iy + 1);
	return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy;
}

// Cloud cover at a point of the cloud plane, in noise cells: three octaves,
// cut at CLOUD_COVER with a soft edge
static float Clouds(float x, float z)
{
	float n = (4.0f * Noise(x, z) + 2.0f * Noise(2.0f * x + 17.0f, 2.0f * z + 31.0f) + Noise(4.0f * x + 53.0f, 4.0f * z + 7.0f)) / 7.0f;
	return smoothstep(CLOUD_COVER, CLOUD_COVER + 0.2f, n);
}

// The share of the sun the ball hides from a point in the room. The ray
// towards the sun passes the ball's centre closest at along, and misses it by
// miss; the light is a disc SUN_SOFTNESS wide, so the edge of the shadow is
// a band that widens with along, as a real penumbra does
static float Shadow(vec3 point)
{
	vec3 o = point - Ball;
	float along = -dot(o, Sun);
	if (along <= 0.0f) return 0.0f;
	vec3 closest = o + Sun * along;
	float miss = sqrt(dot(closest, closest));
	float penumbra = along * SUN_SOFTNESS;
	return 1.0f - smoothstep(BALL_RADIUS - penumbra, BALL_RADIUS + penumbra, miss);
}

// The colour a ray sees leaving origin along ray, both in view space. The
// ray's length is the distance the view has already travelled to origin,
// and spread the radians a pixel's rays fan over from there on.
static unsigned char ShadeRoom(vec3 origin, vec3 ray, float spread)
{
	vec3 o = RoomFrame * origin;
	vec3 d = RoomFrame * ray;
	float length = sqrt(dot(d, d));
	// Sine of the ray's dip below the horizon; +y is down
	float dip = d.y / length;

	if (dip <= 0.0f) {
		// sqrt: a linear ramp is horizon-white over most of the sky
		int shade = (int)(sqrt(-dip) * (CHROME_SKY_SHADES - 1) + 0.5f);

		// Clouds, thinned out where a pixel's footprint on their plane, along
		// the ray as on the floor, grows to a sizeable part of a noise cell
		float cloud = 0.0f;
		float t = (-CLOUD_HEIGHT - o.y) / d.y;
		float footprint = length * (ROOM_PIXEL + t * spread) / (-dip * CLOUD_SCALE);
		if (footprint < CLOUD_FOOTPRINT) {
			float x = (o.x + t * d.x + CloudDrift) / CLOUD_SCALE;
			float z = (o.z + t * d.z) / CLOUD_SCALE;
			cloud = Clouds(x, z) * (1.0f - smoothstep(0.5f * CLOUD_FOOTPRINT, CLOUD_FOOTPRINT, footprint));
		}
		// The sun: a disc, anti-aliased over its rim, in a glow
		float facing = dot(d, Sun) / length;
		float sun = 0.0f;
		if (facing > 0.0f) {
			float angle = acos(MIN(facing, 1.0f));
			sun = MAX(1.0f - smoothstep(SUN_RADIUS * 0.8f, SUN_RADIUS * 1.2f, angle), 0.6f * exp(-angle * angle / (0.04f * 0.04f)));
		}
		float white = MAX(cloud, sun);
		int level = (int)(white * (CHROME_WHITE_LEVELS - 1) + 0.5f);
		return CHROME_SKY_START + CLAMP(level, 0, CHROME_WHITE_LEVELS) * CHROME_SKY_SHADES + CLAMP(shade, 0, CHROME_SKY_SHADES);
	}

	float t = (ROOM_FLOOR - o.y) / d.y;
	vec3 floorpoint = o + d * t;
	float x = floorpoint.x / ROOM_TILE;
	float z = floorpoint.z / ROOM_TILE;

	// The footprint: a pixel wide across the ray, and that over sin of the
	// dip along it, laid onto the floor's axes by the ray's heading
	float across = length * (ROOM_PIXEL + t * spread) / ROOM_TILE;
	float along = across / dip;
	float heading = sqrt(d.x * d.x + d.z * d.z);
	float hx = heading > 1.0e-6f ? fabs(d.x) / heading : 0.0f;
	float hz = heading > 1.0e-6f ? fabs(d.z) / heading : 1.0f;
	float wx = along * hx + across * hz;
	float wz = along * hz + across * hx;
	float shown = CLAMP01(ChromeChecker(x, z, wx, wz));
	float light = (FLOOR_DARK + (1.0f - FLOOR_DARK) * shown) * (1.0f - SHADOW_DEPTH * Shadow(floorpoint));

	// Haze wherever a pixel covers so much of a square that the checker is
	// only its average
	float haze = CLAMP01((MAX(wx, wz) - CHROME_HAZE_FOOTPRINT) / CHROME_HAZE_FOOTPRINT);
	int level = (int)(haze * (CHROME_HAZE_LEVELS - 1) + 0.5f);
	int shade = (int)(light * (CHROME_FLOOR_SHADES - 1) + 0.5f);
	return CHROME_FLOOR_START + CLAMP(level, 0, CHROME_HAZE_LEVELS) * CHROME_FLOOR_SHADES + CLAMP(shade, 0, CHROME_FLOOR_SHADES);
}

// The room straight from the eye, behind the ball
static void DrawRoom(void)
{
	vec3 eye = { 0.0f, 0.0f, -ROOM_EYE };
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			vec3 ray = { (x + 0.5f - RETRO_WIDTH / 2.0f) * ROOM_PIXEL, (y + 0.5f - RETRO_HEIGHT / 2.0f) * ROOM_PIXEL, 1.0f };
			RETRO.framebuffer[y * RETRO_WIDTH + x] = ShadeRoom(eye + ray, ray, ROOM_PIXEL);
		}
	}
}

// The ball: the view ray reflected about the fragment's normal, into the
// room. A convex mirror fans a pixel's rays out further than the eye does:
// the pixel covers |view| ROOM_PIXEL / cos i of the surface, i the angle of
// incidence, the normal turns by that over the radius, and the reflection by
// twice as much
static unsigned char ShadeMirror(const Fragment &fragment)
{
	float distance = sqrt(dot(fragment.view, fragment.view));
	float incidence = MAX(fabs(dot(fragment.normal, fragment.view)) / distance, 0.02f);
	float spread = ROOM_PIXEL + 2.0f * distance * ROOM_PIXEL / (incidence * BALL_RADIUS);
	return ShadeRoom(fragment.position, reflect(fragment.view, fragment.normal), spread);
}

void DEMO_Render(double time, double deltatime)
{
	// A mirror sphere spinning about its own centre is a no-op: every rotation
	// maps the ball onto itself, so the set of normals behind any fixed screen
	// pixel never changes and the reflection sits dead still. Orbiting the
	// ball over the floor instead moves what does change the picture: where
	// it stands in the room, what it reflects from there, and its shadow.
	float phase = fmod(time * ORBIT_SPEED, 2 * M_PI);
	Ball = vec3{ ORBIT_RADIUS * cosf(phase), 0.0f, ORBIT_RADIUS * sinf(phase) };
	CloudDrift = (float)(time * CLOUD_SPEED);

	DrawRoom();

	// Draw sphere, carried from the room into view space
	vec3 centre = transpose(RoomFrame) * Ball;
	RETRO_RotateModel(0, 0, 0);
	RETRO_TranslateModel(centre.x, centre.y, centre.z);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_SHADER);
}

void DEMO_Initialize(void)
{
	// Init palette. The floor: a grey ramp, each shade mixed further into the
	// horizon a haze level at a time, the last one entirely. The sky: its
	// ramp from the horizon to the zenith, each shade mixed further into
	// white a level at a time. The sky starts from the colour the floor's
	// haze ends in, so the far floor meets it without a seam.
	RETRO_Palette palette[RETRO_COLORS];
	memset(palette, 0, sizeof(palette));
	for (int j = 0; j < CHROME_HAZE_LEVELS; j++) {
		float h = (float)j / (CHROME_HAZE_LEVELS - 1);
		for (int i = 0; i < CHROME_FLOOR_SHADES; i++) {
			float k = (float)i / (CHROME_FLOOR_SHADES - 1);
			RETRO_Palette color;
			color.r = RETRO_WHITE.r * k + (CHROME_HORIZON.r - RETRO_WHITE.r * k) * h;
			color.g = RETRO_WHITE.g * k + (CHROME_HORIZON.g - RETRO_WHITE.g * k) * h;
			color.b = RETRO_WHITE.b * k + (CHROME_HORIZON.b - RETRO_WHITE.b * k) * h;
			RETRO_SetColor(CHROME_FLOOR_START + j * CHROME_FLOOR_SHADES + i, color, palette);
		}
	}
	RETRO_Palette sky[CHROME_SKY_SHADES];
	for (int i = 0; i < CHROME_SKY_SHADES; i++) {
		float k = (float)i / (CHROME_SKY_SHADES - 1);
		sky[i].r = CHROME_HORIZON.r + (RETRO_MIDNIGHTBLUE.r - CHROME_HORIZON.r) * k;
		sky[i].g = CHROME_HORIZON.g + (RETRO_MIDNIGHTBLUE.g - CHROME_HORIZON.g) * k;
		sky[i].b = CHROME_HORIZON.b + (RETRO_MIDNIGHTBLUE.b - CHROME_HORIZON.b) * k;
	}
	for (int j = 0; j < CHROME_WHITE_LEVELS; j++) {
		float w = (float)j / (CHROME_WHITE_LEVELS - 1);
		for (int i = 0; i < CHROME_SKY_SHADES; i++) {
			RETRO_Palette color;
			color.r = sky[i].r + (RETRO_WHITE.r - sky[i].r) * w;
			color.g = sky[i].g + (RETRO_WHITE.g - sky[i].g) * w;
			color.b = sky[i].b + (RETRO_WHITE.b - sky[i].b) * w;
			RETRO_SetColor(CHROME_SKY_START + j * CHROME_SKY_SHADES + i, color, palette);
		}
	}
	RETRO_SetPalette(palette);

	// Tilting the room up by the pitch is the camera looking down by it
	RoomFrame = rotateX(-CAMERA_PITCH);
	Sun = normalize(SUN_DIRECTION);

	Model3D *model = RETRO_Load3DModel("assets/spherequads.obj");
	model->shader = ShadeMirror;
}
