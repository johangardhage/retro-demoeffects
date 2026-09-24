//
// God rays
//
// A rotating ball with six glowing holes and light beams, traced per pixel.
// Camera rays are rotated into the ball's frame, where its holes and beams
// lie on the coordinate axes. Sphere and beam bounds skip empty space;
// a midpoint march integrates haze up to the ball or palette saturation.
//
// Closely spaced samples resolve the soft beam edges without dithering.
// Light scatters evenly and blends the surface colour exponentially towards white.
// Grey, red and blue palette ramps shade the ball and the static spiral wall.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrovector.h"
#include "lib/retromatrix.h"

#define FOCAL 250.0f // pixels; matches the polygon demos

#define HOLE_ANGLE 0.41f // radians from an axis to the rim of its hole
#define BALL_SHINE 0.38f // brightness of the ball where it faces the eye
#define BEAM_SPREAD 0.15f // ball radii a beam widens by for every radius it travels
#define BEAM_SOFTNESS 0.15f // share of a beam's width its edge fades over, either side
#define BEAM_LENGTH 9.0f // ball radii from the lamp to the end of a beam
#define BEAM_FADE 0.5f // share of the beam's length before its end starts to fade
#define BEAM_DENSITY 0.9f // light a ray gathers crossing a beam through its axis
#define BEAM_STEP 0.08f // maximum sample spacing in ball radii

#define PATH_DISTANCE 5.5f // ball radii from the eye to the middle of the ball's path
#define PATH_SPEED 0.45 // radians a second
#define PATH_WIDTH 1.5f // ball radii the path reaches either side
#define PATH_HEIGHT 1.0f // ball radii the path reaches above and below
#define PATH_DEPTH 1.6f // ball radii the path reaches towards and away from the eye

#define SPIN_SPEED_X 0.9 // radians a second
#define SPIN_SPEED_Y 1.3
#define SPIN_SPEED_Z 0.5

#define WALL_ARMS 5 // arms of blue haze
#define WALL_TWIST 0.008f // radians the arms curl through per pixel out from the middle
#define WALL_ROUGHNESS 0.5f // how far the noise pushes the arms' edges about
#define WALL_THRESHOLD 0.72f // how much of the spiral is cut away; higher is thinner arms
#define WALL_CELL 10.0f // pixels a noise cell spans
#define WALL_PIXEL 2 // pixels a block of the wall spans
#define WALL_CORE 60.0f // pixels from the middle the arms thin out within

#define WALL_RED RETRO_Palette{ 35, 2, 0 }
#define WALL_BLUE RETRO_Palette{ 0, 0, 31 }

#define LIGHT_LEVELS 85 // no light through white, per ramp; 3 x 85 fills the palette but for one colour
#define GREY_START 0
#define RED_START (GREY_START + LIGHT_LEVELS)
#define BLUE_START (RED_START + LIGHT_LEVELS)

static vec3 CameraRay[RETRO_WIDTH * RETRO_HEIGHT]; // fixed pinhole rays, before ball rotation
static unsigned char Wall[RETRO_WIDTH * RETRO_HEIGHT]; // RED_START or BLUE_START, per pixel

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

static float BeamDensity(vec3 q)
{
	float r2 = dot(q, q);
	float along = MAX(fabs(q.x), MAX(fabs(q.y), fabs(q.z)));
	float radial = sqrt(MAX(r2 - along * along, 0.0f));
	float width = sinf(HOLE_ANGLE) + BEAM_SPREAD * (along - cosf(HOLE_ANGLE));
	float inside = 1.0f - smoothstep(1.0f - BEAM_SOFTNESS, 1.0f + BEAM_SOFTNESS, radial / width);
	float distance = sqrt(r2);
	float end = 1.0f - smoothstep(BEAM_FADE * BEAM_LENGTH, BEAM_LENGTH, distance);
	if (inside <= 0.0f || end <= 0.0f) return 0.0f;
	return inside * end / (2.0f * width);
}

static float ShadeBall(vec3 normal, vec3 direction, float pixel)
{
	float holeaxis = MAX(fabs(normal.x), MAX(fabs(normal.y), fabs(normal.z)));
	float facing = -dot(normal, direction);
	float shade = BALL_SHINE * facing * facing * facing * facing;
	// Blend the hole rim across a pixel, including the surface foreshortening.
	float rim = 0.5f * sinf(HOLE_ANGLE) * pixel / MAX(facing, 0.1f);
	float hole = smoothstep(cosf(HOLE_ANGLE) - rim, cosf(HOLE_ANGLE) + rim, holeaxis);
	return shade + (1.0f - shade) * hole;
}

// Narrow [entry, exit] to where margin + rate * t stays non-negative.
static bool ClipToPlane(float margin, float rate, float &entry, float &exit)
{
	if (rate == 0.0f) return margin >= 0.0f;
	float crossing = -margin / rate;
	if (rate > 0.0f) entry = MAX(entry, crossing);
	else exit = MIN(exit, crossing);
	return exit > entry;
}

// Clip to a square-sided bound around one beam. Its four side planes
// enclose the round soft edge; the sphere intersection supplies the ends.
static bool BeamBounds(vec3 origin, vec3 direction, int axis, float sign, float &entry, float &exit)
{
	float position[] = { origin.x, origin.y, origin.z };
	float velocity[] = { direction.x, direction.y, direction.z };
	float along = sign * position[axis];
	float forward = sign * velocity[axis];
	float slope = BEAM_SPREAD * (1.0f + BEAM_SOFTNESS);
	float radius = (sinf(HOLE_ANGLE) - BEAM_SPREAD * cosf(HOLE_ANGLE)) * (1.0f + BEAM_SOFTNESS);
	if (!ClipToPlane(along, forward, entry, exit)) return false;
	for (int component = 0; component < 3; component++) {
		if (component == axis) continue;
		if (!ClipToPlane(radius + slope * along - position[component], slope * forward - velocity[component], entry, exit)) return false;
		if (!ClipToPlane(radius + slope * along + position[component], slope * forward + velocity[component], entry, exit)) return false;
	}
	return true;
}

static float IntegrateHaze(vec3 origin, vec3 direction, float entry, float exit, float lightlimit)
{
	if (exit <= entry || lightlimit <= 0.0f) return 0.0f;
	int samples = (int)ceil((exit - entry) / BEAM_STEP);
	float step = (exit - entry) / samples;
	float densitylimit = lightlimit / (BEAM_DENSITY * step);
	struct Span { int first, end; } spans[6];
	int count = 0;
	for (int beam = 0; beam < 6; beam++) {
		float near = entry, far = exit;
		if (!BeamBounds(origin, direction, beam / 2, beam % 2 ? -1.0f : 1.0f, near, far)) continue;
		// Round outwards to keep boundary samples despite floating-point error.
		Span span = { MAX(0, (int)floor((near - entry) / step - 0.5f)),
			MIN(samples, (int)ceil((far - entry) / step + 0.5f)) };
		int insert = count++;
		while (insert > 0 && spans[insert - 1].first > span.first) {
			spans[insert] = spans[insert - 1];
			insert--;
		}
		spans[insert] = span;
	}
	float light = 0.0f;
	// Sorted spans can overlap; visit each midpoint only once.
	int nextsample = 0;
	for (int span = 0; span < count; span++) {
		int first = MAX(nextsample, spans[span].first);
		for (int i = first; i < spans[span].end; i++) {
			vec3 point = origin + direction * (entry + (i + 0.5f) * step);
			light += BeamDensity(point);
			if (light >= densitylimit) return lightlimit;
		}
		nextsample = MAX(nextsample, spans[span].end);
	}
	return light * (BEAM_DENSITY * step);
}

static void BuildPalette(void)
{
	// Init palette. Each ramp adds white light to its colour, a level at a
	// time, and saturates into white
	RETRO_Palette palette[RETRO_COLORS] = {};
	const RETRO_Palette rampcolors[] = { { 0, 0, 0 }, WALL_RED, WALL_BLUE };
	int rampstart = GREY_START;
	for (const RETRO_Palette &color : rampcolors) {
		for (int level = 0; level < LIGHT_LEVELS; level++) {
			int light = level * 255 / (LIGHT_LEVELS - 1);
			RETRO_SetColor(rampstart + level, RETRO_Palette{
				(unsigned char)MIN(color.r + light, 255),
				(unsigned char)MIN(color.g + light, 255),
				(unsigned char)MIN(color.b + light, 255)
			}, palette);
		}
		rampstart += LIGHT_LEVELS;
	}
	RETRO_SetPalette(palette);
}

static void BuildWall(void)
{
	// Draw the wall: the spiral, roughened by two octaves of noise, and
	// thinning out into the middle of the screen
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float px = (x / WALL_PIXEL + 0.5f) * WALL_PIXEL;
			float py = (y / WALL_PIXEL + 0.5f) * WALL_PIXEL;
			float dx = px - RETRO_WIDTH / 2.0f;
			float dy = py - RETRO_HEIGHT / 2.0f;
			float r = sqrt(dx * dx + dy * dy);
			float a = atan2(dy, dx);
			float spiral = 0.5f + 0.5f * cosf(WALL_ARMS * (a + WALL_TWIST * r));
			float noise = (2.0f * Noise(px / WALL_CELL, py / WALL_CELL) + Noise(2.0f * px / WALL_CELL + 17.0f, 2.0f * py / WALL_CELL + 31.0f)) / 3.0f;
			float arms = spiral + WALL_ROUGHNESS * (noise - 0.5f) - 0.3f * (1.0f - smoothstep(0.0f, WALL_CORE, r));
			Wall[y * RETRO_WIDTH + x] = arms > WALL_THRESHOLD ? BLUE_START : RED_START;
		}
	}
}

void DEMO_Render(double time, double deltatime)
{
	float ax = fmod(time * SPIN_SPEED_X, 2 * M_PI);
	float ay = fmod(time * SPIN_SPEED_Y, 2 * M_PI);
	float az = fmod(time * SPIN_SPEED_Z, 2 * M_PI);
	mat3 inverse = transpose(rotate(ax, ay, az));

	// The ball's path is a Lissajous figure; whole multiples of one phase
	// keep it closed, so the wrap is seamless. Where it comes nearest the
	// eye, at phase 3/2 pi, it passes the middle of the screen
	float phase = fmod(time * PATH_SPEED, 2 * M_PI);
	vec3 centre = { PATH_WIDTH * sinf(2 * phase), PATH_HEIGHT * cosf(3 * phase), PATH_DISTANCE + PATH_DEPTH * sinf(phase) };

	vec3 q0 = inverse * -centre;
	float c = dot(q0, q0) - 1.0f;
	float reach = dot(q0, q0) - BEAM_LENGTH * BEAM_LENGTH;

	for (int sy = 0; sy < RETRO_HEIGHT; sy++) {
		for (int sx = 0; sx < RETRO_WIDTH; sx++) {
			vec3 d = inverse * CameraRay[sy * RETRO_WIDTH + sx];
			float b = dot(d, q0);

			// The ball, or the wall behind it
			int base = Wall[sy * RETRO_WIDTH + sx];
			float shade = 0.0f;
			float far = 1.0e9f;
			float disc = b * b - c;
			if (disc > 0.0f) {
				far = -b - sqrt(disc);
				vec3 normal = q0 + d * far;
				base = GREY_START;
				float pixel = far * CameraRay[sy * RETRO_WIDTH + sx].z / FOCAL;
				shade = ShadeBall(normal, d, pixel);
			}

			// The haze, where the ray is within BEAM_LENGTH of the lamp
			float light = 0.0f;
			float span = b * b - reach;
			if (span > 0.0f) {
				float t0 = MAX(-b - sqrt(span), 0.0f);
				float t1 = MIN(-b + sqrt(span), far);
				// Stop when the remaining light cannot change the rounded palette level.
				float remaining = (1.0f - shade) * (LIGHT_LEVELS - 1);
				float lightlimit = remaining > 0.5f ? logf(2.0f * remaining) : 0.0f;
				light = IntegrateHaze(q0, d, t0, t1, lightlimit);
			}

			float lit = 1.0f - (1.0f - shade) * expf(-light);
			int level = (int)(lit * (LIGHT_LEVELS - 1) + 0.5f);
			RETRO.framebuffer[sy * RETRO_WIDTH + sx] = base + CLAMP(level, 0, LIGHT_LEVELS);
		}
	}
}

void DEMO_Initialize(void)
{
	BuildPalette();

	BuildWall();

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			CameraRay[y * RETRO_WIDTH + x] = normalize(vec3{
				x + 0.5f - RETRO_WIDTH / 2.0f, y + 0.5f - RETRO_HEIGHT / 2.0f, FOCAL
			});
		}
	}
}
