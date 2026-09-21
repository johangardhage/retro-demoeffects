//
// Amiga Juggler
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
// This file's technique: real linear-light RGB, the same engine
// juggler2.cpp uses - ambient, diffuse and reflection are added into one
// colour accumulator per ray, rather than an index picked off a fixed
// palette ramp the way juggler.cpp does it, so a chrome ball genuinely
// reddens against the torso instead of that being approximated. The one
// exception is Eric Graham's real glint() highlight (see TraceScene and
// MINGLINT), which is not additive at all: found, it replaces ambient and
// diffuse outright with flat white; not found, it contributes nothing,
// ever.
//
// Four camera rays a pixel, a quarter-pixel apart, are each quantized to
// the palette on their own and the most common result wins, rather than
// averaging their four raw colours and quantizing once. This palette is a
// handful of disjoint hue-to-black ramps, not a full RGB cube, so averaging
// two different materials' colours first can land on a point that sits on
// no ramp at all - nearest-colour search then resolves that to whatever
// ramp happens to be closest, usually an unrelated grey. Quantizing each
// sample before combining them never produces a colour that was not
// already on some ramp. That antialiasing is what lets a mirror ball's
// reflected checkerboard read as fine detail instead of a blocky mosaic,
// and lets a silhouette edge blend two materials instead of hard-cutting
// between them - retraced live from continuous time, at real cost: four
// samples a pixel every displayed frame against juggler2.cpp's one, using
// the same engine, make this the slowest of the three.
//
// Eric Graham's real 1987 source (recovered and republished by Ernie Wright
// and by AlphaPixel) gives each mirror sphere its own colour, <.9,.9,.9>,
// and multiplies a bounce's traced result by it - not a lossless mirror, so
// reflectioncolor below is 0.9 rather than white, dimming a chrome ball's
// reflection by a tenth on every bounce (the only material weight left on
// that path - see TraceScene, where only a hit's own material kind, not a
// separate reflection field, decides whether it bounces at all).
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrovector.h"
#include "lib/retropalette.h"

#define NEAR_T 0.001f // t past which a hit counts, so a ray does not re-hit its own origin
#define MAX_BOUNCES 10 // matches the source recreation's own MAX_DEPTH
#define MIN_THROUGHPUT (1.0f / 256.0f) // matches its MIN_COLOR_INTENSITY - a bounce this dim can no longer move a displayed byte

#define FOCAL_DISTANCE 50.0f // world units from the eye to the virtual screen
#define SCREEN_WIDTH 100.0f // world units the virtual screen spans; with FOCAL_DISTANCE this sets the field of view
#define SUPERSAMPLE 2 // SUPERSAMPLE x SUPERSAMPLE camera rays a pixel, averaged before quantizing to a palette index

#define FLOOR_TILE 107.0f // world units per checker tile

// The source recreation measured these from photographs of the original: a
// shadowed floor tile reads as 40% of its lit colour, a shadowed sphere as
// 15%, 102 and 40 of 255. Both are stored decoded to linear light, since
// every material colour here is linear light and only encoded to sRGB right
// before it is shown - so the weight that makes a 0.4 or 0.15 photograph
// measurement come out right, after that encoding, is the measurement
// decoded by RETRO_SRGBToLinear, not the measurement itself. Both are
// AMBIENT_SHAPE's anchor value below, not the literal ambient weight - see the identical
// constants in juggler2.cpp, whose comment this mirrors
#define PLASTIC_AMBIENT RETRO_SRGBToLinear(40 / 255.0f)
#define PLASTIC_DIFFUSE 1.0f
#define PLASTIC_SPECULAR 1.0f
#define MATTE_AMBIENT RETRO_SRGBToLinear(102 / 255.0f)
#define MATTE_DIFFUSE 1.5f
#define MIRROR_SPECULAR 1.0f

// Eric Graham's real glint() test: not a Phong falloff, a hard cutoff on
// cos^2 of the angle between the camera ray's reflection and the light -
// see TraceScene
#define MINGLINT 0.95f

// The real source's ambient term is sky light, not a flat constant: a patch
// facing straight up gets the full (N.up + 1.5) * 0.4 (peaking at 1), one
// facing straight down a fifth of that. This carries that ratio but is
// rescaled by 2/3 so a patch facing level with the horizon - N.up = 0,
// roughly what the camera sees head-on - lands on exactly the material's
// own ambient weight, leaving its calibration above where it was
#define AMBIENT_SHAPE(normal) (((normal).y + 1.5f) * (2.0f / 3.0f))

#define PALETTE_MATERIAL_SHADES 32 // six materials x 32 = 192
#define PALETTE_SKY_SHADES 56 // a big, smooth, prominent area - measured at roughly half of every rendered pixel, so worth far more than a small one
#define PALETTE_WHITE_SHADES 8 // a generic black-to-white ramp: whatever a search does not already have close by - measured at under 0.1% of rendered pixels, so this is a deliberately small reserve, not a peer of the other two

// The shower: a fast low arc returning a ball to the throwing hand, a slow
// high arc lobbing it back. Both hands sit at JUGGLE_X0 and JUGGLE_X1;
// height and duration (in 1/30s units) fix the ballistic constants below via
// v_y(t) = v0 - g t and, from energy conservation at the arc's apex, g = v0^2 / 2h
static const double JUGGLE_X0 = -182;
static const double JUGGLE_X1 = -108;
static const double JUGGLE_Y0 = 88;
static const double JUGGLE_H_Y = 184; // apex height of the high arc

static const double JUGGLE_H_VX = (JUGGLE_X0 - JUGGLE_X1) / 60.0;
static const double JUGGLE_L_VX = (JUGGLE_X1 - JUGGLE_X0) / 30.0;
static const double JUGGLE_H_H = JUGGLE_H_Y - JUGGLE_Y0;
static const double JUGGLE_H_VY = 4.0 * JUGGLE_H_H / 60.0;
static const double JUGGLE_G = JUGGLE_H_VY * JUGGLE_H_VY / (2.0 * JUGGLE_H_H);
static const double JUGGLE_L_VY = 0.5 * JUGGLE_G * 30.0;
static const double JUGGLE_RATE = 30.0; // "T" units a second, so the 30-unit pattern above loops once a second

static const double HIPS_MAX_Y = 85;
static const double HIPS_MIN_Y = 81;
static const double HIPS_ANGLE_MULTIPLIER = 2.0 * M_PI / 30.0;

enum JugglerMaterial { MAT_MIRROR, MAT_TORSO, MAT_SKIN, MAT_HAIR, MAT_EYE };

struct JugglerSphere {
	vec3 center;
	float radius;
	unsigned char material;
};

// Indices 2..85 mirror the source recreation's own numbering, so the update
// code below can be checked against it line for line: balls 2 -- 4, torso
// 5 -- 12, head 13, neck 14, left leg 15 -- 31, right leg 32 -- 48, left arm
// 49 -- 65, right arm 66 -- 82, eyes 83 -- 84, hair 85. 0 and 1 are unused.
#define JUGGLER_SPHERES 86
static JugglerSphere Body[JUGGLER_SPHERES];

static vec3 CamEye, CamCenter, CamU, CamV;
static vec3 LightPos = { -564, 686, 147 };

// One material per Phong term (see the file header) - ambient, diffuse and
// specular, plus the colour each tints. specular is a weight, not a curve -
// see MINGLINT and TraceScene, where it gates a hard cutoff rather than a
// shininess exponent. Plastic (torso, skin, hair, eye) and matte (the
// floor) share their own weights across every colour they come in; only
// the mirror balls are their own one-off, a specular-only material with no
// colour of their own to speak of - everything they show is either the
// light's own white or whatever they reflect (see TraceScene: only mirrors
// bounce, and reflectioncolor is the only thing that dims that bounce)
struct PhongMaterial {
	float ambient, diffuse, specular;
	vec3 color, highlight, reflectioncolor;
};

static PhongMaterial Mats[5]; // indexed by JugglerMaterial
static PhongMaterial FloorYellow, FloorGreen;
static vec3 SkyMin, SkyMax;
static vec3 Palette[RETRO_COLORS];

// A material's own colour is stored in linear light, decoded from the
// sRGB hex a colour picker would give - see PLASTIC_AMBIENT above for why
// that matters once it is lit
static vec3 Linear(int hex, float scale = 1.0f)
{
	return { RETRO_SRGBToLinear(scale * ((hex >> 16) & 0xFF) / 255.0f),
		RETRO_SRGBToLinear(scale * ((hex >> 8) & 0xFF) / 255.0f),
		RETRO_SRGBToLinear(scale * (hex & 0xFF) / 255.0f) };
}

static PhongMaterial CreatePlastic(vec3 color)
{
	return { PLASTIC_AMBIENT, PLASTIC_DIFFUSE, PLASTIC_SPECULAR, color, { 1, 1, 1 }, { 0, 0, 0 } };
}

static PhongMaterial CreateMatte(vec3 color)
{
	return { MATTE_AMBIENT, MATTE_DIFFUSE, 0.0f, color, { 0, 0, 0 }, { 0, 0, 0 } };
}

static bool IntersectSphere(const JugglerSphere &sphere, vec3 origin, vec3 dir, float &t, bool allowinside = true)
{
	vec3 oc = origin - sphere.center;
	float b = dot(dir, oc);
	float c = dot(oc, oc) - sphere.radius * sphere.radius;
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

// Eric Graham's real glint(): is p inside the tight cone where the light's
// reflection about normal lines up with the view direction? A hard cutoff
// on cos^2 of that angle, not a Phong falloff - see MINGLINT. r here is the
// light's reflection compared to the view direction rather than the real
// source's camera ray's own reflection compared to the light - the two are
// equivalent by the symmetry of reflection, and this reuses lambert's
// already-normalized vectors instead of the real source's unnormalized ones
static bool Glint(vec3 normal, vec3 l, float lambert, vec3 dir)
{
	vec3 r = normal * (2.0f * lambert) - l;
	float rdotmd = dot(r, -dir);
	return rdotmd > 0.0f && rdotmd * rdotmd > MINGLINT;
}

// full tests every sphere; otherwise only the three balls (indices 2..4) are
// checked, trading self-shadowing between body parts - an arm shadowing the
// torso, say - for speed, since that is a subtle effect next to the floor's
// own shadow, which needs the full cast to keep its silhouette
static bool Shadowed(vec3 origin, vec3 dir, float maxdist, bool full)
{
	int end = full ? JUGGLER_SPHERES : 5;
	for (int i = 2; i < end; i++) {
		float t;
		if (IntersectSphere(Body[i], origin, dir, t, false) && t < maxdist) return true;
	}
	return false;
}

// Ray/sphere and ray/plane against the whole cast, reused for primary rays
// and, from a chrome ball's own surface, for its reflection - so a mirror
// ball shows the rest of the juggler and the floor sliding across it with
// real parallax rather than a canned environment map. Returns linear-light
// radiance; DEMO_Render encodes it to sRGB and quantizes it to a palette index.
static vec3 TraceScene(vec3 origin, vec3 dir)
{
	vec3 pixel = { 0, 0, 0 };
	vec3 throughput = { 1, 1, 1 };

	for (int bounce = 0; bounce < MAX_BOUNCES; bounce++) {
		float nearest = 1e30f;
		bool floorhit = false;
		if (dir.y < -NEAR_T) {
			float t = -origin.y / dir.y;
			if (t > NEAR_T) {
				nearest = t;
				floorhit = true;
			}
		}

		int hit = -1;
		for (int i = 2; i < JUGGLER_SPHERES; i++) {
			float t;
			if (IntersectSphere(Body[i], origin, dir, t) && t < nearest) {
				nearest = t;
				hit = i;
				floorhit = false;
			}
		}

		if (hit < 0 && !floorhit) {
			pixel += throughput * lerp(SkyMin, SkyMax, CLAMP01(dir.y));
			break;
		}

		vec3 p = origin + dir * nearest;
		vec3 normal;
		const PhongMaterial *m;

		if (floorhit) {
			normal = { 0, 1, 0 };
			int tilex = (int)floorf(p.x / FLOOR_TILE);
			int tilez = (int)floorf(p.z / FLOOR_TILE);
			m = ((tilex + tilez) & 1) == 0 ? &FloorGreen : &FloorYellow;
		} else {
			normal = normalize(p - Body[hit].center);
			if (dot(normal, dir) >= 0.0f) normal = -normal;
			m = &Mats[Body[hit].material];
		}

		vec3 pout = p + normal * 0.35f;
		bool fullshadow = floorhit || Body[hit].material == MAT_MIRROR;

		vec3 tolight = LightPos - pout;
		float lightdist = length(tolight);
		vec3 l = tolight / lightdist;
		float lambert = dot(normal, l);
		bool lit = lambert > 0.0f && !Shadowed(pout, l, lightdist, fullshadow);

		// found or not, Glint is never blended with ambient plus diffuse -
		// see the file header
		bool glinted = lit && m->specular > 0.0f && Glint(normal, l, lambert, dir);

		if (glinted) {
			pixel += throughput * m->highlight; // brite[k] = 1 outright in the real source, not scaled by a weight
		} else {
			if (m->ambient > 0.0f) pixel += throughput * m->color * (m->ambient * AMBIENT_SHAPE(normal));
			if (lit && m->diffuse > 0.0f) pixel += throughput * m->color * (m->diffuse * lambert);
		}

		// only the mirror balls bounce; reflectioncolor is what actually dims
		// the throughput on the way out, not a separate reflection weight
		if (floorhit || Body[hit].material != MAT_MIRROR) break;
		throughput = throughput * m->reflectioncolor;
		if (throughput.x < MIN_THROUGHPUT && throughput.y < MIN_THROUGHPUT && throughput.z < MIN_THROUGHPUT) break;
		dir = normalize(reflect(dir, normal));
		origin = pout;
	}

	return pixel;
}

// Places j where circles of radius A (about p) and B (about q) meet in the
// plane normal to w (see the file header for the two-triangle solve). Every
// limb tapers over 8 spheres each side, so that count is not a parameter
static void UpdateAppendage(int sceneindex, vec3 p, vec3 q, vec3 w, float A, float B)
{
	vec3 V = normalize(q - p);
	float D = length(q - p);
	vec3 W = normalize(w);
	vec3 U = cross(V, W);

	float A2 = A * A;
	float y = (A2 - B * B + D * D) / (2.0f * D);
	float x = sqrtf(MAX(A2 - y * y, 0.0f));

	vec3 j = p + U * x + V * y;

	vec3 d = (j - p) * (1.0f / 8.0f);
	for (int i = 0; i <= 8; i++) {
		Body[sceneindex + i].center = p + d * (float)i;
	}

	d = (j - q) * (1.0f / 8.0f);
	for (int i = 0; i < 8; i++) {
		Body[9 + sceneindex + i].center = q + d * (float)i;
	}
}

// Radii and materials, set once - the source recreation's own split between
// createScene (sizes and materials, called once) and updateScene (centres,
// called every frame - see UpdateScene below) kept here rather than folded
// into DEMO_Initialize alongside the camera and the palette, which have
// nothing to do with the body
static void CreateScene(void)
{
	for (int i = 2; i <= 4; i++) {
		Body[i] = { { 110, 0, 0 }, 14, MAT_MIRROR };
	}
	for (int i = 5; i <= 12; i++) {
		float percent = (i - 5) / 7.0f;
		Body[i] = { {}, 16.0f + 4.0f * percent, MAT_TORSO };
	}
	Body[13] = { {}, 14, MAT_SKIN }; // head
	Body[14] = { {}, 5, MAT_SKIN }; // neck

	for (int limb = 0; limb < 4; limb++) {
		int base = 15 + 17 * limb;
		for (int i = 0; i <= 7; i++) {
			Body[base + i] = { {}, 2.5f + 2.5f * i / 7.0f, MAT_SKIN };
		}
		for (int i = 8; i <= 16; i++) {
			Body[base + i] = { {}, 5, MAT_SKIN };
		}
	}

	Body[83] = { {}, 4, MAT_EYE };
	Body[84] = { {}, 4, MAT_EYE };
	Body[85] = { {}, 14, MAT_HAIR };
}

static void UpdateScene(double T)
{
	double Tlow = fmod(T, 30.0);
	double Thigh0 = fmod(T + 30.0, 60.0);
	double Thigh1 = fmod(T, 60.0);

	Body[3].center.y = (float)(JUGGLE_Y0 + (JUGGLE_H_VY - 0.5 * JUGGLE_G * Thigh0) * Thigh0);
	Body[3].center.z = (float)(JUGGLE_X1 + JUGGLE_H_VX * Thigh0);

	Body[4].center.y = (float)(JUGGLE_Y0 + (JUGGLE_H_VY - 0.5 * JUGGLE_G * Thigh1) * Thigh1);
	Body[4].center.z = (float)(JUGGLE_X1 + JUGGLE_H_VX * Thigh1);

	Body[2].center.y = (float)(JUGGLE_Y0 + (JUGGLE_L_VY - 0.5 * JUGGLE_G * Tlow) * Tlow);
	Body[2].center.z = (float)(JUGGLE_X0 + JUGGLE_L_VX * Tlow);

	// Hips ride a cosine; the same phase drives the shoulders and knees below
	double angle = HIPS_ANGLE_MULTIPLIER * T;
	double oscillation = 0.5 * (1.0 + cos(angle));

	vec3 o = { 151, (float)(HIPS_MIN_Y + (HIPS_MAX_Y - HIPS_MIN_Y) * oscillation), -151 };
	vec3 v = normalize(vec3{ 0, 70, (float)((HIPS_MIN_Y - HIPS_MAX_Y) * sin(angle)) });
	vec3 u = { 0, v.z, -v.y };

	for (int i = 5; i <= 12; i++) {
		float percent = (i - 5) / 7.0f;
		Body[i].center = o + v * (32.0f * percent);
	}
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

	Body[83].center = o + v * 69.0f + u * -7.0f;
	Body[83].center.x = 142;
	Body[84].center = o + v * 69.0f + u * 7.0f;
	Body[84].center.x = 142;

	Body[85].center = o + v * 71.0f;
	Body[85].center.x = 152;
}

#define QUANTIZE_BITS 6 // 64 levels a channel
#define QUANTIZE_LEVELS (1 << QUANTIZE_BITS)
static unsigned char ColorLookup[QUANTIZE_LEVELS * QUANTIZE_LEVELS * QUANTIZE_LEVELS];

// The nearest of the 256 built colours by squared distance, for every colour
// a 6-bit-a-channel cube can hold - built once, since a search this size
// once a pixel, over four samples and up to ten bounces each, is not free
static void BuildColorLookup(void)
{
	for (int r = 0; r < QUANTIZE_LEVELS; r++) {
		for (int g = 0; g < QUANTIZE_LEVELS; g++) {
			for (int b = 0; b < QUANTIZE_LEVELS; b++) {
				vec3 color = { r / (float)(QUANTIZE_LEVELS - 1), g / (float)(QUANTIZE_LEVELS - 1), b / (float)(QUANTIZE_LEVELS - 1) };
				float best = 1e30f;
				int index = 0;
				for (int i = 0; i < RETRO_COLORS; i++) {
					vec3 d = color - Palette[i];
					float err = dot(d, d);
					if (err < best) {
						best = err;
						index = i;
					}
				}
				ColorLookup[(r * QUANTIZE_LEVELS + g) * QUANTIZE_LEVELS + b] = (unsigned char)index;
			}
		}
	}
}

static unsigned char QuantizeToPalette(vec3 color)
{
	int r = (int)(CLAMP01(color.x) * (QUANTIZE_LEVELS - 1) + 0.5f);
	int g = (int)(CLAMP01(color.y) * (QUANTIZE_LEVELS - 1) + 0.5f);
	int b = (int)(CLAMP01(color.z) * (QUANTIZE_LEVELS - 1) + 0.5f);
	return ColorLookup[(r * QUANTIZE_LEVELS + g) * QUANTIZE_LEVELS + b];
}

#define SAMPLES (SUPERSAMPLE * SUPERSAMPLE)

// The palette is a handful of disjoint straight ramps (each material's hue
// faded to black, plus the sky's own gradient), not a full RGB cube, so
// averaging two different materials' linear colours before quantizing lands
// off every ramp - nearest-colour search then snaps that point to whichever
// ramp is geometrically closest, usually an unrelated grey. Quantizing each
// sample first and taking the most common result never averages across
// ramps, so a supersampled edge only ever resolves to colours that were
// actually seen there
static unsigned char MajorityColor(const unsigned char *samples)
{
	unsigned char best = samples[0];
	int bestcount = 0;
	for (int i = 0; i < SAMPLES; i++) {
		int count = 0;
		for (int j = 0; j < SAMPLES; j++) {
			if (samples[j] == samples[i]) count++;
		}
		if (count > bestcount) {
			bestcount = count;
			best = samples[i];
		}
	}
	return best;
}

void DEMO_Render(double time, double deltatime)
{
	UpdateScene(time * JUGGLE_RATE);

	float ratio = SCREEN_WIDTH / RETRO_WIDTH;
	float substep = 1.0f / SUPERSAMPLE;
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			unsigned char samples[SAMPLES];
			int n = 0;
			for (int sy = 0; sy < SUPERSAMPLE; sy++) {
				for (int sx = 0; sx < SUPERSAMPLE; sx++) {
					float px = x + (sx + 0.5f) * substep;
					float py = y + (sy + 0.5f) * substep;
					float a = ratio * (px - RETRO_WIDTH / 2.0f);
					float b = ratio * (RETRO_HEIGHT / 2.0f - py);
					vec3 p = CamCenter + CamU * a + CamV * b;
					vec3 dir = normalize(p - CamEye);
					vec3 color = TraceScene(CamEye, dir);
					color = { RETRO_LinearToSRGB(color.x), RETRO_LinearToSRGB(color.y), RETRO_LinearToSRGB(color.z) };
					samples[n++] = QuantizeToPalette(color);
				}
			}
			RETRO_PutPixel(x, y, MajorityColor(samples));
		}
	}
}

// A colour a byte at a time, straight off the sRGB scale with no decode -
// unlike Linear, this builds the palette in the same already-encoded space
// DEMO_Render's search compares against
static vec3 Byte(int hex)
{
	return { ((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f, (hex & 0xFF) / 255.0f };
}

void DEMO_Initialize(void)
{
	Mats[MAT_MIRROR] = { 0.0f, 0.0f, MIRROR_SPECULAR, { 1, 1, 1 }, { 1, 1, 1 }, { 0.9f, 0.9f, 0.9f } };
	Mats[MAT_TORSO] = CreatePlastic(Linear(0xE51715, 1.05f));
	Mats[MAT_SKIN] = CreatePlastic(Linear(0xF2ADAB, 1.05f));
	Mats[MAT_EYE] = CreatePlastic(Linear(0x1E1B94, 1.4f));
	Mats[MAT_HAIR] = CreatePlastic(Linear(0x261117, 1.4f));
	FloorYellow = CreateMatte({ 1, 1, 0 });
	FloorGreen = CreateMatte({ 0, 1, 0 });
	SkyMin = Linear(0xBDBDFF);
	SkyMax = Linear(0x2223F6);

	// Six materials, black to their own colour, then the sky's own gradient,
	// then a plain black-to-white ramp for whatever else a search turns up
	vec3 materialcolor[6] = { Byte(0xF2ADAB), Byte(0xE51715), Byte(0x1E1B94), Byte(0x261117), { 1, 1, 0 }, { 0, 1, 0 } };
	int index = 0;
	for (int m = 0; m < 6; m++) {
		for (int shade = 0; shade < PALETTE_MATERIAL_SHADES; shade++) {
			Palette[index++] = materialcolor[m] * (shade / (float)(PALETTE_MATERIAL_SHADES - 1));
		}
	}
	for (int shade = 0; shade < PALETTE_SKY_SHADES; shade++) {
		Palette[index++] = lerp(Byte(0xBDBDFF), Byte(0x2223F6), shade / (float)(PALETTE_SKY_SHADES - 1));
	}
	for (int shade = 0; shade < PALETTE_WHITE_SHADES; shade++) {
		Palette[index++] = vec3{ 1, 1, 1 } * (shade / (float)(PALETTE_WHITE_SHADES - 1));
	}
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, (unsigned char)(Palette[i].x * 255), (unsigned char)(Palette[i].y * 255), (unsigned char)(Palette[i].z * 255));
	}
	BuildColorLookup();

	// A camera obscura: w points from the look-at point back to the eye, u
	// and v complete a right-handed frame around it, and the virtual screen
	// sits FOCAL_DISTANCE in front of the eye along -w
	CamEye = { 2, 100, -2 };
	vec3 look = { 1000, 77, -1000 };
	vec3 w = normalize(CamEye - look);
	CamCenter = CamEye - w * FOCAL_DISTANCE;
	CamU = normalize(vec3{ w.z, 0, -w.x });
	CamV = cross(w, CamU);

	CreateScene();
}
