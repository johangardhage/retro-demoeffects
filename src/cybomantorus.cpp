//
// Cyboman torus
//
// The eye sits on the centre line of a hollow torus.  Rather than drawing a
// mesh from the outside, one ray is marched through every pixel until it meets
// the torus skin.  The camera and its rays stay fixed in world space.  Both are
// transformed by the inverse object rotation before intersection, so only the
// torus moves, including the characteristic pinched far wall.
//
// The decoration is procedural in the torus' two natural angles: broad bands
// run around the tube and alternating triangles run around the ring.  Four
// 64-colour palette ramps give those materials their cold green/blue lighting.
// A light at the eye supplies the travelling white flare.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define MAJOR_RADIUS 2.25f
#define TUBE_RADIUS 1.0f
#define CAMERA_FOV 1.22f
#define ROTATION_SPEED 0.42f
#define MAX_STEPS 72
#define MAX_DISTANCE 12.0f
#define HIT_EPSILON 0.003f

struct Vec3 {
	float x, y, z;
};

static inline Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static inline Vec3 Scale(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static inline float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline Vec3 Normalise(Vec3 a)
{
	float inverse = 1.0f / sqrtf(Dot(a, a));
	return Scale(a, inverse);
}

static Vec3 Rotate(Vec3 p, float ax, float ay, float az)
{
	float sx = sinf(ax), cx = cosf(ax);
	float sy = sinf(ay), cy = cosf(ay);
	float sz = sinf(az), cz = cosf(az);

	float y = p.y * cx - p.z * sx;
	float z = p.y * sx + p.z * cx;
	p.y = y; p.z = z;
	float x = p.x * cy + p.z * sy;
	z = -p.x * sy + p.z * cy;
	p.x = x; p.z = z;
	x = p.x * cz - p.y * sz;
	y = p.x * sz + p.y * cz;
	return {x, y, p.z};
}

static float TorusDistance(Vec3 p)
{
	float radial = sqrtf(p.x * p.x + p.z * p.z) - MAJOR_RADIUS;
	return sqrtf(radial * radial + p.y * p.y) - TUBE_RADIUS;
}

static Vec3 TorusNormal(Vec3 p)
{
	float radial = sqrtf(p.x * p.x + p.z * p.z);
	float centrex = MAJOR_RADIUS * p.x / radial;
	float centrez = MAJOR_RADIUS * p.z / radial;
	return Normalise({p.x - centrex, p.y, p.z - centrez});
}

static int Material(Vec3 p)
{
	float alpha = atan2f(p.z, p.x) / (2.0f * M_PI) + 0.5f;
	float radial = sqrtf(p.x * p.x + p.z * p.z) - MAJOR_RADIUS;
	float beta = atan2f(p.y, radial) / (2.0f * M_PI) + 0.5f;

	int band = (int)floorf(beta * 12.0f);
	if ((band & 3) == 0 || (band & 3) == 3) return 1;

	// Two saw teeth per broad band.  XORing ring and tube cells alternates
	// their direction, producing the reference's long zig-zag borders.
	float ringcell = alpha * 28.0f;
	float tubecell = beta * 12.0f;
	float tooth = ringcell - floorf(ringcell);
	float across = tubecell - floorf(tubecell);
	bool triangle = ((int)floorf(ringcell) ^ (int)floorf(tubecell)) & 1
		? across < tooth : across < 1.0f - tooth;
	return triangle ? 2 : 0;
}

void DEMO_Render(double deltatime)
{
	static float phase = 0.0f;
	phase = fmodf(phase + deltatime * ROTATION_SPEED, 2.0f * M_PI);

	// The world-space camera never moves.  It sits on the tube centreline;
	// looking inward sees across the hole and
	// catches the opposite tube as the pinched central shape; looking outward
	// still meets the broad near wall around it.
	Vec3 worldeye = {MAJOR_RADIUS, 0.0f, 0.0f};
	// Fixed look-at basis.  The camera is on +X, so forward points directly at
	// the torus centre; right and down complete the screen frame.
	Vec3 forward = {-1.0f, 0.0f, 0.0f};
	Vec3 right = {0.0f, 0.0f, -1.0f};
	Vec3 down = {0.0f, 1.0f, 0.0f};
	float aspect = (float)RETRO_WIDTH / RETRO_HEIGHT;
	float ax = phase;
	float ay = phase * 0.61f;

	// Inverse object rotation.  The X turn visibly tumbles the torus; its axis
	// passes through the camera.  The Y turn spins the patterned skin around the
	// torus' own symmetry axis.  This combination keeps the fixed eye inside.
	Vec3 eye = Rotate(worldeye, -ax, -ay, 0.0f);

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float sy = (2.0f * (y + 0.5f) / RETRO_HEIGHT - 1.0f) * CAMERA_FOV;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float sx = (2.0f * (x + 0.5f) / RETRO_WIDTH - 1.0f) * CAMERA_FOV * aspect;
			Vec3 worldray = Normalise(Add(forward, Add(Scale(right, sx), Scale(down, sy))));
			Vec3 ray = Rotate(worldray, -ax, -ay, 0.0f);

			float distance = 0.0f;
			Vec3 point = eye;
			bool hit = false;
			for (int step = 0; step < MAX_STEPS && distance < MAX_DISTANCE; step++) {
				point = Add(eye, Scale(ray, distance));
				float surface = fabsf(TorusDistance(point));
				if (surface < HIT_EPSILON) { hit = true; break; }
				distance += MAX(surface * 0.78f, HIT_EPSILON);
			}

			if (!hit) continue;
			Vec3 normal = TorusNormal(point);
			// We see the inside face, so its useful normal points into the cavity.
			float diffuse = MAX(0.0f, Dot(Scale(normal, -1.0f), Scale(ray, -1.0f)));
			float flare = powf(diffuse, 18.0f);
			float fog = 1.0f / (1.0f + distance * 0.10f);
			int shade = CLAMP((0.16f + 0.48f * diffuse + 0.32f * flare) * fog * 63.0f, 0, 64);
			RETRO_PutPixel(x, y, Material(point) * 64 + shade);
		}
	}
}

void DEMO_Initialize(void)
{
	// Four ramps: green cloth, deep cyan bands, pale teeth, and spare white.
	for (int material = 0; material < 4; material++) {
		for (int shade = 0; shade < 64; shade++) {
			float s = (float)shade / 63.0f;
			float glow = powf(s, 5.0f);
			int r, g, b;
			if (material == 0) { r = 10 + 52*s; g = 28 + 130*s; b = 24 + 75*s; }
			else if (material == 1) { r = 4 + 35*s; g = 25 + 110*s; b = 35 + 155*s; }
			else if (material == 2) { r = 20 + 130*s; g = 42 + 145*s; b = 38 + 115*s; }
			else { r = g = b = 40 + 180*s; }
			r += 110 * glow; g += 100 * glow; b += 115 * glow;
			RETRO_SetColor(material * 64 + shade, CLAMP256(r), CLAMP256(g), CLAMP256(b));
		}
	}
}
