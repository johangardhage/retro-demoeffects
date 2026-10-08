//
// Plasma ball
//
// ARCS filaments run from a central electrode to the inside of a glass
// sphere. Each filament is a midpoint-displacement path between its two
// ends: with the ends a and b of a span fixed, its midpoint is
//
//   m = (a + b) / 2 + ARC_ROUGHNESS · o · (b − a)⊥
//
// and the two halves are split the same way, ARC_LEVELS times, so the
// kink at every scale is in proportion to the span it breaks. (b − a)⊥
// is the span turned a quarter turn, as long as the span. The offsets o
// are kept per point and walk from step to step as
//
//   o' = ARC_KEEP o + √(1 − ARC_KEEP²) n,   n of unit variance
//
// which keeps their variance at one while the filament writhes, and a
// fresh ARC_JITTER n on top of o is what makes it crackle. The far end
// sits on the glass. Arc i points along angle θ_i, which drifts under
// random kicks and a pairwise repulsion
//
//   θ_i'' = ARC_REPEL Σ_j sin(θ_i − θ_j) exp(ARC_REPEL_REACH cos(θ_i − θ_j))
//           + ARC_DRIFT n
//
// damped by ARC_SPIN_KEEP, so the filaments wander but stay spread round
// the ball. The exp term (minus the slope of a von Mises bump) makes a
// neighbor push hardest when close. An arc also leans toward or away
// from the viewer by tilt, and its end lands at BALL_RADIUS cos(tilt)
// from the center. Each arc forks BRANCHES short side branches off points
// in its middle half, each bent off the arc's heading and redrawn from
// scratch after a life of a few steps, so they flicker while the main
// filament only writhes.
//
// The paths are drawn as thin lines of intensity, brighter toward the
// electrode, into a float buffer C. The glow is two blurs of it, each two
// box passes (a tent), one tight and one wide:
//
//   I = glass + electrode + CORE_GAIN C + TIGHT_GAIN tent_1(C)
//     + WIDE_GAIN tent_6(C)
//
// and the palette index is 1 − exp(−I), so a white core rolls off into
// pink and violet instead of clipping.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retrovector.h"

#define BALL_RADIUS 104.0f // pixels to the inside of the glass
#define ELECTRODE_RADIUS 10.0f // pixels of the central electrode
#define ARCS 7 // main filaments
#define ARC_LEVELS 6 // midpoint subdivisions of a filament
#define ARC_POINTS ((1 << ARC_LEVELS) + 1) // points along a filament
#define ARC_ROUGHNESS 0.18f // midpoint offset per unit of span
#define ARC_KEEP 0.985f // kept of each offset per step, so how slowly a filament writhes
#define ARC_JITTER 0.15f // fresh offset added every step, the crackle
#define ARC_REPEL 3.0f // how hard the filaments push apart
#define ARC_REPEL_REACH 2.0f // how much harder a close neighbor pushes than a far one
#define ARC_DRIFT 8.0f // random angular kick, radians per second squared
#define ARC_SPIN_KEEP 0.97f // kept of the angular velocity per step
#define TILT_MAX 0.9f // radians a filament leans toward or away from the viewer
#define TILT_DRIFT 0.03f // random walk of the tilt per step
#define BRANCHES 3 // side branches per filament
#define BRANCH_LEVELS 4 // midpoint subdivisions of a branch
#define BRANCH_POINTS ((1 << BRANCH_LEVELS) + 1) // points along a branch
#define BRANCH_LIFE 6 // steps a branch lives, at most
#define ARC_BRIGHTNESS 0.5f // intensity at the electrode end of a filament
#define BRANCH_BRIGHTNESS 0.25f // intensity at the root of a branch
#define SPOT_BRIGHTNESS 4.0f // intensity where a filament meets the glass
#define CORE_GAIN 1.0f // weight of the sharp lines
#define TIGHT_GAIN 1.5f // weight of the tight glow
#define WIDE_GAIN 4.0f // weight of the wide glow
#define GLASS_RIM 0.35f // intensity of the glass edge
#define GLASS_RIM_WIDTH 3.0f // pixels of the glass edge
#define GLASS_HAZE 0.08f // intensity the glass adds at its rim, falling to 0 at the center
#define GLASS_HIGHLIGHT 0.3f // intensity of the window reflection
#define ELECTRODE_GLOW 0.6f // intensity of the halo round the electrode

static const vec2 Center = { RETRO_WIDTH / 2.0f, RETRO_HEIGHT / 2.0f };
static const vec2 Highlight = { -0.45f * BALL_RADIUS, -0.5f * BALL_RADIUS }; // from the center

struct Branch {
	int root;
	int life;
	float bend;
	float reach;
	float offset[BRANCH_POINTS];
	vec2 point[BRANCH_POINTS];
};

struct Arc {
	float angle;
	float spin;
	float tilt;
	float offset[ARC_POINTS];
	vec2 point[ARC_POINTS];
	Branch branch[BRANCHES];
};

static Arc Arcs[ARCS];

static float Glass[RETRO_WIDTH * RETRO_HEIGHT];
static float Core[RETRO_WIDTH * RETRO_HEIGHT];
static float Tight[RETRO_WIDTH * RETRO_HEIGHT];
static float Wide[RETRO_WIDTH * RETRO_HEIGHT];

//
// A sum of three uniforms, zero mean and unit variance
//
static float Noise(void)
{
	return RANDOMF(2) + RANDOMF(2) + RANDOMF(2) - 3.0f;
}

//
// Fill point[1 .. count-2] between the two fixed ends by midpoint displacement
//
static void Displace(vec2 *point, const float *offset, int count, float jitter)
{
	for (int step = count - 1; step > 1; step /= 2) {
		int half = step / 2;
		for (int i = half; i < count; i += step) {
			vec2 a = point[i - half];
			vec2 b = point[i + half];
			vec2 normal = { a.y - b.y, b.x - a.x };
			point[i] = (a + b) * 0.5f + normal * (ARC_ROUGHNESS * (offset[i] + jitter * Noise()));
		}
	}
}

//
// Pick a new root, bend, reach and shape for a branch
//
static void Respawn(Branch *branch)
{
	branch->root = ARC_POINTS / 4 + RANDOM(ARC_POINTS / 2);
	branch->life = 2 + RANDOM(BRANCH_LIFE - 1);
	branch->bend = (float)mix(0.3, 0.9, RAND()) * (RANDOM(2) ? 1.0f : -1.0f);
	branch->reach = mix(0.25, 0.5, RAND());
	for (float &offset : branch->offset) {
		offset = Noise();
	}
}

//
// Add v at a fractional pixel, shared bilinearly between its four neighbors
//
static void Splat(vec2 p, float v)
{
	int x = (int)floorf(p.x);
	int y = (int)floorf(p.y);
	if (x < 0 || y < 0 || x >= RETRO_WIDTH - 1 || y >= RETRO_HEIGHT - 1) {
		return;
	}
	float fx = p.x - x;
	float fy = p.y - y;
	float *core = Core + y * RETRO_WIDTH + x;
	core[0] += v * (1 - fx) * (1 - fy);
	core[1] += v * fx * (1 - fy);
	core[RETRO_WIDTH] += v * (1 - fx) * fy;
	core[RETRO_WIDTH + 1] += v * fx * fy;
}

//
// Draw a polyline whose intensity runs linearly from start to end
//
static void DrawPath(const vec2 *point, int count, float start, float end)
{
	for (int i = 0; i < count - 1; i++) {
		vec2 a = point[i];
		vec2 b = point[i + 1];
		int steps = MAX((int)ceilf(distance(a, b)), 1);
		for (int k = 0; k < steps; k++) {
			float t = (k + 0.5f) / steps;
			float u = (i + t) / (count - 1);
			Splat(mix(a, b, t), mix(start, end, u));
		}
	}
}

//
// Box blur of the given radius, rows then columns, reading zero past the edges.
// Source and dest may be the same buffer
//
static void BoxBlur(const float *source, float *dest, int radius)
{
	static float scratch[RETRO_WIDTH * RETRO_HEIGHT]; // the rows, between the two passes

	float scale = 1.0f / (2 * radius + 1);
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		const float *row = source + y * RETRO_WIDTH;
		float sum = 0;
		for (int x = 0; x < radius; x++) {
			sum += row[x];
		}
		for (int x = 0; x < RETRO_WIDTH; x++) {
			if (x + radius < RETRO_WIDTH) {
				sum += row[x + radius];
			}
			scratch[y * RETRO_WIDTH + x] = sum * scale;
			if (x - radius >= 0) {
				sum -= row[x - radius];
			}
		}
	}
	for (int x = 0; x < RETRO_WIDTH; x++) {
		float sum = 0;
		for (int y = 0; y < radius; y++) {
			sum += scratch[y * RETRO_WIDTH + x];
		}
		for (int y = 0; y < RETRO_HEIGHT; y++) {
			if (y + radius < RETRO_HEIGHT) {
				sum += scratch[(y + radius) * RETRO_WIDTH + x];
			}
			dest[y * RETRO_WIDTH + x] = sum * scale;
			if (y - radius >= 0) {
				sum -= scratch[(y - radius) * RETRO_WIDTH + x];
			}
		}
	}
}

void DEMO_FixedUpdate(RETRO_Time time)
{
	float dt = (float)time.delta;
	float kick = sqrtf(1.0f - ARC_KEEP * ARC_KEEP);

	for (Arc &arc : Arcs) {
		// Wander, pushed apart from the other filaments
		float push = 0;
		for (const Arc &other : Arcs) {
			float d = arc.angle - other.angle;
			push += sinf(d) * expf(ARC_REPEL_REACH * cosf(d));
		}
		arc.spin = (arc.spin + (ARC_REPEL * push + ARC_DRIFT * Noise()) * dt) * ARC_SPIN_KEEP;
		arc.angle = fmodf(arc.angle + arc.spin * dt, 2 * (float)M_PI);
		arc.tilt = clamp(arc.tilt + TILT_DRIFT * Noise(), 0.0f, TILT_MAX);

		// Writhe and redraw the filament
		for (float &offset : arc.offset) {
			offset = ARC_KEEP * offset + kick * Noise();
		}
		vec2 heading = { cosf(arc.angle), sinf(arc.angle) };
		arc.point[0] = Center + heading * ELECTRODE_RADIUS;
		arc.point[ARC_POINTS - 1] = Center + heading * (BALL_RADIUS * cosf(arc.tilt));
		Displace(arc.point, arc.offset, ARC_POINTS, ARC_JITTER);

		// Fork the branches off it
		for (Branch &branch : arc.branch) {
			if (--branch.life <= 0) {
				Respawn(&branch);
			}
			vec2 root = arc.point[branch.root];
			vec2 span = arc.point[ARC_POINTS - 1] - root;
			branch.point[0] = root;
			branch.point[BRANCH_POINTS - 1] = root + rotate(span, branch.bend) * branch.reach;
			Displace(branch.point, branch.offset, BRANCH_POINTS, ARC_JITTER);
		}
	}
}

void DEMO_Render(RETRO_Time time)
{
	// Draw filaments, branches and the spots where they touch the glass
	memset(Core, 0, sizeof(Core));
	for (const Arc &arc : Arcs) {
		DrawPath(arc.point, ARC_POINTS, ARC_BRIGHTNESS, ARC_BRIGHTNESS * 0.4f);
		for (const Branch &branch : arc.branch) {
			DrawPath(branch.point, BRANCH_POINTS, BRANCH_BRIGHTNESS, 0);
		}
		Splat(arc.point[ARC_POINTS - 1], SPOT_BRIGHTNESS);
	}

	// Glow: a tight and a wide tent
	BoxBlur(Core, Wide, 1);
	BoxBlur(Wide, Tight, 1);
	BoxBlur(Core, Wide, 6);
	BoxBlur(Wide, Wide, 6);

	unsigned char *buffer = RETRO_FrameBuffer();
	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		float intensity = Glass[i] + CORE_GAIN * Core[i] + TIGHT_GAIN * Tight[i] + WIDE_GAIN * Wide[i];
		buffer[i] = CLAMP256((1.0f - expf(-intensity)) * RETRO_COLORS);
	}
}

void DEMO_Initialize(void)
{
	// Init filaments, spread evenly round the ball
	for (int i = 0; i < ARCS; i++) {
		Arcs[i].angle = 2 * (float)M_PI * i / ARCS;
		Arcs[i].tilt = RANDOMF(TILT_MAX);
		for (float &offset : Arcs[i].offset) {
			offset = Noise();
		}
	}

	// Init the still part of the picture: the glass edge, a haze that thickens
	// toward it, a window reflection, and the electrode with its halo
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			vec2 p = vec2{ x + 0.5f, y + 0.5f } - Center;
			float r = length(p);
			float edge = (r - BALL_RADIUS) / GLASS_RIM_WIDTH;
			float intensity = GLASS_RIM * expf(-edge * edge);
			if (r < BALL_RADIUS) {
				intensity += GLASS_HAZE * (r / BALL_RADIUS) * (r / BALL_RADIUS);
				vec2 h = (p - Highlight) / (0.07f * BALL_RADIUS);
				intensity += GLASS_HIGHLIGHT * expf(-0.5f * dot(h, h));
			}
			float e = r / ELECTRODE_RADIUS;
			intensity += ELECTRODE_GLOW * expf(-0.125f * e * e);
			if (e < 1) {
				intensity += 3.0f * sqrtf(1 - e * e);
			}
			Glass[y * RETRO_WIDTH + x] = intensity;
		}
	}

	// Init palette. Index is 1 − exp(−I): violet haze, pink filaments, white cores.
	RETRO_CreateGradientPalette(0, 40, RETRO_BLACK, RETRO_INDIGOBLACK);
	RETRO_CreateGradientPalette(40, 100, RETRO_INDIGOBLACK, RETRO_PURPLE);
	RETRO_CreateGradientPalette(100, 170, RETRO_PURPLE, RETRO_HOTPINK);
	RETRO_CreateGradientPalette(170, 230, RETRO_HOTPINK, RETRO_PINKLACE);
	RETRO_CreateGradientPalette(230, RETRO_COLORS, RETRO_PINKLACE, RETRO_WHITE);
}
