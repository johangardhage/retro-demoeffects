//
// Tunnel, free-directional
//
// A tube that the camera is inside of rather than a picture of one seen down
// its length: the camera flies along it, strays from its middle and turns all
// the way round as it goes, from looking along the tube, across it into the
// wall, back the way it came, into the wall on the other side and round to
// the front again. Nothing is looked up from a table, since there is no one
// view that a table could hold. A ray is sent out through every pixel and met
// with the wall.
//
// The tube is the cylinder x² + y² = R² about the z axis. A ray from the eye
// at o along the unit direction d is at o + t d, and is on the wall where
//
//   a t² + 2 b t + c = 0,   a = dx² + dy²
//                           b = ox dx + oy dy
//                           c = ox² + oy² - R²
//
// The eye is inside the tube, so c is negative, the roots have different
// signs, and the one ahead of the eye is
//
//   t = (-b + √(b² - a c)) / a
//
// A ray along the tube never gets to the wall: a is 0 and t is without end,
// which is the black at the far end of the tube. The wall is hit at
// p = o + t d, and the texture is laid on it by where around the tube that is
// and where along it:
//
//   u = atan2(py, px) / 2π · TEXTURE_TURNS    textures around the tube
//   v = pz / TEXTURE_LENGTH                   textures along it
//
// d is the ray through the pixel, (x - W/2, y - H/2, FOCAL), turned by the
// camera's rotation: about the camera's upright by ay, which is the turn all
// the way round, and then about the axis of the tube by az, so that the turn
// does not always sweep the same side of the wall. The flight is all in o:
// its z goes up steadily, and phase lives on TEXTURE_LENGTH, after which the
// wall is the same again, while its x and y go round the axis at TUNNEL_SWAY
// from it.
//
// The wall is darkened by how far off it was hit, down to black at
// TUNNEL_FOG. The texture's bytes are palette indices, so the darkening is a
// lookup: FogTable holds, for every color of the texture at every one of
// TUNNEL_FOG_SHADES levels, the palette entry nearest to it. The level is
// ordered-dithered between neighbors, so the steps from one to the next do
// not show as rings down the tube.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromatrix.h"
#include "lib/retroshadetable.h"

#define TEXTURE_WIDTH 256
#define TEXTURE_HEIGHT 256
#define TEXTURE_TURNS 3 // times the texture goes around the tube
#define TEXTURE_LENGTH 2.0f // length of tube a texture covers
#define TUNNEL_RADIUS 1.0f
#define TUNNEL_SWAY 0.45f // how far from the axis the camera flies
#define TUNNEL_SWAY_SPEED 0.9 // radians per second it goes round the axis at
#define TUNNEL_SPEED 1.6 // length of tube flown per second
#define TUNNEL_FOG 6.0f // how far off the wall is black
#define TUNNEL_FOG_SHADES 32 // steps from black to full color
#define FOCAL (RETRO_WIDTH / 2) // pixels from the eye to the screen
#define ROTATION_SPEED_Y 0.5 // radians per second
#define ROTATION_SPEED_Z 0.17

static unsigned char FogTable[RETRO_COLORS * TUNNEL_FOG_SHADES];

void DEMO_Render(double time, double deltatime)
{
	// Calculate rotation
	float ay = fmod(time * ROTATION_SPEED_Y, 2 * M_PI);
	float az = fmod(time * ROTATION_SPEED_Z, 2 * M_PI);

	// Calculate phase
	float phase = fmod(time * TUNNEL_SPEED, TEXTURE_LENGTH);
	float sway = fmod(time * TUNNEL_SWAY_SPEED, 2 * M_PI);

	unsigned char *image = RETRO_ImageData();

	mat3 rotation = rotateZ(az) * rotateY(ay);
	vec3 eye = { TUNNEL_SWAY * cosf(sway), TUNNEL_SWAY * sinf(sway), phase };
	float c = eye.x * eye.x + eye.y * eye.y - TUNNEL_RADIUS * TUNNEL_RADIUS;

	// Draw tunnel, a ray through every pixel
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			vec3 ray = rotation * normalize(vec3{ x + 0.5f - RETRO_WIDTH / 2, y + 0.5f - RETRO_HEIGHT / 2, FOCAL });

			// How far off the ray meets the wall, without end for a ray along the tube
			float a = ray.x * ray.x + ray.y * ray.y;
			float b = eye.x * ray.x + eye.y * ray.y;
			float distance = MIN((-b + sqrtf(b * b - a * c)) / a, TUNNEL_FOG);
			vec3 hit = eye + distance * ray;

			int tx = WRAP(atan2f(hit.y, hit.x) / (2 * M_PI) * TEXTURE_TURNS * TEXTURE_WIDTH, TEXTURE_WIDTH);
			int ty = WRAP(hit.z / TEXTURE_LENGTH * TEXTURE_HEIGHT, TEXTURE_HEIGHT);
			int shade = (1 - distance / TUNNEL_FOG) * (TUNNEL_FOG_SHADES - 1) + RETRO_DitherThreshold(x, y);
			unsigned char color = image[ty * TEXTURE_WIDTH + tx];

			RETRO_PutPixel(x, y, FogTable[color * TUNNEL_FOG_SHADES + shade]);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_Image *picture = RETRO_LoadImage("assets/flowers_256x256.pcx", true);

	RETRO_CreateShadeTable(picture->palette, RETRO_COLORS, TUNNEL_FOG_SHADES, FogTable);
}
