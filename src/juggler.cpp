//
// Amiga Juggler — basic variant
//
// In 1986 Eric Graham found his Amiga 1000 could ray trace: three chrome
// balls tossed by a humanoid built entirely out of spheres, one second of
// looping animation that talked Commodore into buying the rights to it.
// Every part of that scene is a sphere - torso, head, limbs, juggling balls,
// even the sky - so the whole demo is one ray/sphere intersection and one
// ray/plane intersection, reused for primary rays, shadow rays and the
// chrome balls' reflections alike.
//
// Each arm and leg is two line segments (p to a joint j, and q to j) lying
// in a plane, packed with a fixed count of spheres tapering in radius along
// the way. j is found the way two circles of radius A and B, centred at p
// and q, intersect: drop a perpendicular of length y from the p-q axis,
//
//   y = (A^2 - B^2 + D^2) / 2D,   x = sqrt(A^2 - y^2),   D = |q - p|
//
// two applications of Pythagoras on the right triangles the height splits
// the p-q-j triangle into. (U, V, W) is an orthonormal frame for the limb's
// plane - V along p to q, W its given normal, U completing it - so
// j = p + xU + yV places the elbow or knee on the correct side without ever
// naming an angle. The hips sway on a cosine and the shoulders and knees
// answer to the same phase, which is what reads as a single juggling motion
// instead of independent limbs.
//
// The balls fly a shower pattern: a fast low arc back to the throwing hand,
// a slow high arc over to the other one, both plain projectile motion,
// v_y(t) = v0 - g t, solved from the height and duration of each arc via
// energy conservation, 1/2 v0^2 = g h. Two balls share the high arc a half
// period apart. Because the low arc's period (30) is exactly half the high
// arc's (60), every 30 units the ball finishing the high arc and the one
// finishing the low arc meet at the same point at the same instant - so
// swapping which formula drives which sphere there is invisible, and a pair
// of one-way parabolas becomes a seamless loop with no motion blending.
//
// This is the plain one of the three, and its technique is now the simplest
// of them too: no colour arithmetic at all. Every hit resolves to a kind
// (sky, a floor tile, or one of the body's materials) plus a brightness and
// whether it is inside Eric Graham's real glint() cone, and RampIndex turns
// that straight into an index already sitting in that kind's own two-part
// palette ramp - black up to the kind's own hue, then that hue up to white
// (see CreateMaterialRamp) - no colour accumulator, no palette search. The
// highlight test itself is the exact same one juggler2.cpp and juggler3.cpp
// use (see Glint): a hard cos² cutoff, not a blend, since any blend of a
// ramp's hue and its white end shows as a visible band of some third
// colour against a saturated background, however narrow. What stays plain
// here, unlike those two files, is ambient (flat, not their sky-shaped
// AMBIENT_SHAPE) and the mirror, which never dims what it reflects.
//
// One camera ray a pixel, retraced live from continuous time every
// displayed frame, the same as juggler2.cpp.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retrovector.h"

#define NEAR_T 0.001f // t past which a hit counts, so a ray does not re-hit its own origin
#define MAX_BOUNCES 10
#define FOCAL_DISTANCE 50.0f // world units from the eye to the virtual screen
#define SCREEN_WIDTH 100.0f // world units the virtual screen spans; with FOCAL_DISTANCE this sets the field of view
#define FLOOR_TILE 107.0f // world units per checker tile
#define PLASTIC_AMBIENT 0.05f
#define PLASTIC_DIFFUSE 1.3f // >1, unlike juggler2.cpp/juggler3.cpp's 1.0 - flat ambient here has no AMBIENT_SHAPE boost to lean on, so diffuse alone makes up the difference
#define MATTE_AMBIENT 0.15f
#define MATTE_DIFFUSE 1.5f
// Eric Graham's real glint() test: not a Phong falloff, a hard cutoff on
// cos² of the angle between the camera ray's reflection and the light -
// see Glint. The same constant and test juggler2.cpp and juggler3.cpp use
#define MINGLINT 0.95f
#define JUGGLE_RATE 30.0 // "T" units a second, so the 30-unit pattern in UpdateScene loops once a second
#define SPHERE_COUNT 86

// Every ramp below is two RETRO_CreateGradientPalette halves back to back -
// see CreateMaterialRamp - so each *_SHADES stays even
#define SKY_START 0
#define SKY_SHADES 64
#define FLOOR_YELLOW_START (SKY_START + SKY_SHADES)
#define FLOOR_SHADES 32
#define FLOOR_GREEN_START (FLOOR_YELLOW_START + FLOOR_SHADES)
#define TORSO_START (FLOOR_GREEN_START + FLOOR_SHADES)
#define TORSO_SHADES 32
#define SKIN_START (TORSO_START + TORSO_SHADES)
#define SKIN_SHADES 64
#define HAIR_START (SKIN_START + SKIN_SHADES)
#define HAIR_SHADES 16
#define EYE_START (HAIR_START + HAIR_SHADES)
#define EYE_SHADES 16

enum MatId { Mirror, Torso, Skin, Hair, Eye };

struct Sphere {
	vec3 center;
	float radius;
	unsigned char mat;
};
static Sphere Body[SPHERE_COUNT];

static vec3 CamEye, CamCenter, CamU, CamV;
static vec3 LightPos = { -564, 686, 147 };

static bool IntersectSphere(const Sphere &s, vec3 origin, vec3 dir, float &t, bool allowinside = true)
{
	vec3 oc = origin - s.center;
	float b = dot(dir, oc);
	float c = dot(oc, oc) - s.radius * s.radius;
	float disc = b * b - c;
	if (disc < 0.0f) return false;
	float root = sqrtf(disc);
	float tt = -b - root;
	if (tt <= NEAR_T) {
		if (!allowinside) return false;
		tt = -b + root;
	}
	if (tt <= NEAR_T) return false;
	t = tt;
	return true;
}

static bool Shadowed(vec3 origin, vec3 dir, float maxdist)
{
	for (int i = 2; i < SPHERE_COUNT; i++) {
		float t;
		if (IntersectSphere(Body[i], origin, dir, t, false) && t < maxdist) return true;
	}
	return false;
}

// Eric Graham's real glint(): is p inside the tight cone where the light's
// reflection about normal lines up with the view direction? A hard cutoff
// on cos² of that angle, not a Phong falloff - see MINGLINT. Identical to
// juggler2.cpp/juggler3.cpp's own Glint
static bool Glint(vec3 normal, vec3 l, float lambert, vec3 dir)
{
	vec3 r = normal * (2.0f * lambert) - l;
	float rdotmd = dot(r, -dir);
	return rdotmd > 0.0f && rdotmd * rdotmd > MINGLINT;
}

// Ambient plus diffuse in x, whether p is inside Glint's cone in y (1 or 0) -
// ambient here is flat, unlike juggler2.cpp/juggler3.cpp's sky-shaped
// AMBIENT_SHAPE, but the glint test itself is exactly theirs
static vec2 ShadeLight(vec3 p, vec3 normal, vec3 dir, float ambient, float diffuse)
{
	vec3 tolight = LightPos - p;
	float lightdist = length(tolight);
	vec3 l = tolight / lightdist;
	float lambert = MAX(dot(normal, l), 0.0f);
	if (lambert <= 0.0f || Shadowed(p, l, lightdist)) return { ambient, 0.0f };

	float glint = Glint(normal, l, lambert, dir) ? 1.0f : 0.0f;
	return { ambient + diffuse * lambert, glint };
}

static void MaterialRange(unsigned char mat, int &start, int &shades)
{
	switch (mat) {
	case Torso: start = TORSO_START; shades = TORSO_SHADES; break;
	case Skin: start = SKIN_START; shades = SKIN_SHADES; break;
	case Hair: start = HAIR_START; shades = HAIR_SHADES; break;
	default: start = EYE_START; shades = EYE_SHADES; break;
	}
}

static int RampIndex(int start, int shades, float brightness, float glint)
{
	if (glint > 0.0f) return start + shades - 1;
	int half = shades / 2;
	// start + half is the pure hue itself (the first entry of the second
	// RETRO_CreateGradientPalette call, since its "from" is the first call's
	// "to" - see CreateMaterialRamp), reachable at brightness 1 only by
	// scaling against half, not half - 1: the first call's own last entry
	// falls (half - 1) / half of the way there, one step short of true peak
	return start + (int)(CLAMP01(brightness) * half);
}

static unsigned char TraceScene(vec3 origin, vec3 dir)
{
	float glint = 0.0f; // brightest glint seen on any mirror bounce along the way, carried to whatever is finally resolved
	int start = SKY_START, shades = SKY_SHADES; // sky by default, if the bounce budget runs out mid-mirror-chain
	float brightness = CLAMP01(dir.y);

	for (int bounce = 0; bounce < MAX_BOUNCES; bounce++) {
		float nearest = 1e30f;
		bool floorhit = false;
		if (dir.y < -NEAR_T) {
			float t = -origin.y / dir.y;
			if (t > NEAR_T) { nearest = t; floorhit = true; }
		}
		int hit = -1;
		for (int i = 2; i < SPHERE_COUNT; i++) {
			float t;
			if (IntersectSphere(Body[i], origin, dir, t) && t < nearest) { nearest = t; hit = i; floorhit = false; }
		}
		if (hit < 0 && !floorhit) { brightness = CLAMP01(dir.y); break; }

		vec3 p = origin + dir * nearest;
		vec3 normal = floorhit ? vec3{ 0, 1, 0 } : normalize(p - Body[hit].center);
		if (!floorhit && dot(normal, dir) >= 0.0f) normal = -normal;
		vec3 pOut = p + normal * 0.35f;

		if (floorhit) {
			int tilex = (int)floorf(p.x / FLOOR_TILE), tilez = (int)floorf(p.z / FLOOR_TILE);
			start = ((tilex + tilez) & 1) == 0 ? FLOOR_GREEN_START : FLOOR_YELLOW_START;
			shades = FLOOR_SHADES;
			brightness = ShadeLight(pOut, normal, dir, MATTE_AMBIENT, MATTE_DIFFUSE).x;
			break;
		}

		if (Body[hit].mat == Mirror) {
			glint = MAX(glint, ShadeLight(pOut, normal, dir, 0.0f, 0.0f).y);
			dir = normalize(dir - normal * (2.0f * dot(dir, normal)));
			origin = pOut;
			continue;
		}

		MaterialRange(Body[hit].mat, start, shades);
		vec2 shade = ShadeLight(pOut, normal, dir, PLASTIC_AMBIENT, PLASTIC_DIFFUSE);
		brightness = shade.x;
		glint = MAX(glint, shade.y);
		break;
	}

	return (unsigned char)RampIndex(start, shades, brightness, glint);
}

// Places j where circles of radius A (about p) and B (about q) meet in the
// plane normal to w - see juggler2.cpp for the two-triangle solve. Every
// limb tapers over 8 spheres each side, so that count is not a parameter
static void UpdateAppendage(int sceneindex, vec3 p, vec3 q, vec3 w, float A, float B)
{
	vec3 V = normalize(q - p);
	float D = length(q - p);
	vec3 U = cross(V, normalize(w));
	float y = (A * A - B * B + D * D) / (2.0f * D);
	float x = sqrtf(MAX(A * A - y * y, 0.0f));
	vec3 j = p + U * x + V * y;
	vec3 d = (j - p) * (1.0f / 8.0f);
	for (int i = 0; i <= 8; i++) Body[sceneindex + i].center = p + d * (float)i;
	d = (j - q) * (1.0f / 8.0f);
	for (int i = 0; i < 8; i++) Body[9 + sceneindex + i].center = q + d * (float)i;
}

static void CreateScene(void)
{
	for (int i = 2; i <= 4; i++) Body[i] = { { 110, 0, 0 }, 14, Mirror };
	for (int i = 5; i <= 12; i++) {
		float percent = (i - 5) / 7.0f;
		Body[i] = { {}, 16.0f + 4.0f * percent, Torso };
	}
	Body[13] = { {}, 14, Skin }; // head
	Body[14] = { {}, 5, Skin }; // neck
	for (int limb = 0; limb < 4; limb++) {
		int base = 15 + 17 * limb;
		for (int i = 0; i <= 7; i++) Body[base + i] = { {}, 2.5f + 2.5f * i / 7.0f, Skin };
		for (int i = 8; i <= 16; i++) Body[base + i] = { {}, 5, Skin };
	}
	Body[83] = { {}, 4, Eye };
	Body[84] = { {}, 4, Eye };
	Body[85] = { {}, 14, Hair };
}

static void UpdateScene(double T)
{
	const double X0 = -182, X1 = -108, Y0 = 88, HY = 184;
	const double HVX = (X0 - X1) / 60.0, LVX = (X1 - X0) / 30.0;
	const double HH = HY - Y0, HVY = 4.0 * HH / 60.0;
	const double G = HVY * HVY / (2.0 * HH), LVY = 0.5 * G * 30.0;

	// T grows without bound over the session's runtime, so it is wrapped
	// into each ball's own period before use - see the file header for why
	// swapping Thigh0/Thigh1 at the 30-unit mark is invisible
	double Tlow = fmod(T, 30.0);
	double Thigh0 = fmod(T + 30.0, 60.0);
	double Thigh1 = fmod(T, 60.0);
	Body[2].center.y = (float)(Y0 + (LVY - 0.5 * G * Tlow) * Tlow);
	Body[2].center.z = (float)(X0 + LVX * Tlow);
	Body[3].center.y = (float)(Y0 + (HVY - 0.5 * G * Thigh0) * Thigh0);
	Body[3].center.z = (float)(X1 + HVX * Thigh0);
	Body[4].center.y = (float)(Y0 + (HVY - 0.5 * G * Thigh1) * Thigh1);
	Body[4].center.z = (float)(X1 + HVX * Thigh1);

	// Hips ride a cosine; the same phase drives the shoulders and knees below
	double angle = 2.0 * M_PI / 30.0 * T;
	double oscillation = 0.5 * (1.0 + cos(angle));
	vec3 o = { 151, (float)(81.0 + 4.0 * oscillation), -151 };
	vec3 v = normalize(vec3{ 0, 70, (float)(-4.0 * sin(angle)) });
	vec3 u = { 0, v.z, -v.y };

	for (int i = 5; i <= 12; i++) Body[i].center = o + v * (32.0f * (i - 5) / 7.0f);
	Body[13].center = o + v * 70.0f;
	Body[14].center = o + v * 55.0f;

	vec3 p = { 159, 2.5f, -133 };
	vec3 q = o + v * -9.0f + u * -16.0f;
	UpdateAppendage(15, p, q, u, 42.58f, 34.07f);
	p = { 139, 2.5f, -164 };
	q = o + v * -9.0f + u * 16.0f;
	UpdateAppendage(32, p, q, u, 42.58f, 34.07f);

	double armangle = -0.35 * oscillation;
	p = { (float)(69.0 + 41.0 * cos(armangle)), (float)(60.0 - 41.0 * sin(armangle)), -108 };
	q = o + v * 45.0f + u * -19.0f;
	vec3 n = (o + v * 45.41217f + u * -19.91111f) - q;
	UpdateAppendage(49, p, q, n, 44.294f, 46.098f);
	p.z = -182;
	q = o + v * 45.0f + u * 19.0f;
	n = q - (o + v * 45.41217f + u * 19.91111f);
	UpdateAppendage(66, p, q, n, 44.294f, 46.098f);

	Body[83].center = o + v * 69.0f + u * -7.0f; Body[83].center.x = 142;
	Body[84].center = o + v * 69.0f + u * 7.0f; Body[84].center.x = 142;
	Body[85].center = o + v * 71.0f; Body[85].center.x = 152;
}

void DEMO_Render(double time, double deltatime)
{
	UpdateScene(time * JUGGLE_RATE);
	float ratio = SCREEN_WIDTH / RETRO_WIDTH;
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float a = ratio * (x + 0.5f - RETRO_WIDTH / 2.0f), b = ratio * (RETRO_HEIGHT / 2.0f - (y + 0.5f));
			vec3 dir = normalize(CamCenter + CamU * a + CamV * b - CamEye);
			RETRO_PutPixel(x, y, TraceScene(CamEye, dir));
		}
	}
}

// Half the ramp is RampIndex's black-to-hue side, the other half its
// hue-to-white side - see RampIndex and the file header
static void CreateMaterialRamp(int start, int shades, RETRO_Palette from, RETRO_Palette to)
{
	int half = shades / 2;
	RETRO_CreateGradientPalette(start, start + half, from, to);
	RETRO_CreateGradientPalette(start + half, start + shades, to, RETRO_WHITE);
}

void DEMO_Initialize(void)
{
	CreateMaterialRamp(SKY_START, SKY_SHADES, RETRO_RGB(0xBDBDFF), RETRO_RGB(0x2223F6));
	CreateMaterialRamp(FLOOR_YELLOW_START, FLOOR_SHADES, RETRO_BLACK, RETRO_YELLOW);
	CreateMaterialRamp(FLOOR_GREEN_START, FLOOR_SHADES, RETRO_BLACK, RETRO_GREEN);
	CreateMaterialRamp(TORSO_START, TORSO_SHADES, RETRO_BLACK, RETRO_RGB(0xE51715));
	CreateMaterialRamp(SKIN_START, SKIN_SHADES, RETRO_BLACK, RETRO_RGB(0xF2ADAB));
	CreateMaterialRamp(HAIR_START, HAIR_SHADES, RETRO_BLACK, RETRO_RGB(0x261117));
	CreateMaterialRamp(EYE_START, EYE_SHADES, RETRO_BLACK, RETRO_RGB(0x1E1B94));

	// A camera obscura: w points from the look-at point back to the eye, u
	// and v complete a right-handed frame around it
	CamEye = { 2, 100, -2 };
	vec3 look = { 1000, 77, -1000 };
	vec3 w = normalize(CamEye - look);
	CamCenter = CamEye - w * FOCAL_DISTANCE;
	CamU = normalize(vec3{ w.z, 0, -w.x });
	CamV = cross(w, CamU);

	CreateScene();
}
