//
// God rays
//
// A rotating ball with six glowing holes and volumetric beams, rendered
// through polygon coverage and fragment shaders. godrays3.cpp traces the
// same scene directly; this version shades only pixels covered by a model.
//
// The ball shader locates holes in its local frame and softens their rims.
// Each beam shader intersects the view ray with a truncated cone, stops at
// the ball's saved depth and samples haze along the visible interval.
// Haze scatters evenly, without a directional brightness boost.
//
// Beams crossing the eye use a full-screen quad to retain complete coverage.
// Their light accumulates in floating point before exponential blending into
// the grey, red and blue palette ramps. The noisy spiral wall is static.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"

#define HOLE_ANGLE 0.41f // radians from an axis to the rim of its hole
#define BALL_SHINE 0.38f // brightness of the ball where it faces the eye
#define BEAM_SPREAD 0.15f // ball radii a beam widens by for every radius it travels
#define BEAM_APEX (cosf(HOLE_ANGLE) - sinf(HOLE_ANGLE) / BEAM_SPREAD) // ball radii from the centre to the beam's apex, behind it
#define BEAM_LENGTH 9.0f // ball radii from the lamp to the end of a beam
#define BEAM_FADE 0.5f // share of the beam's length before its end starts to fade
#define BEAM_DENSITY 0.9f // light a ray gathers crossing a beam through its axis
#define BEAM_SOFTNESS 0.15f // share of a beam's width its edge fades over, either side
#define BEAM_SAMPLES 12 // minimum steps; narrow beam edges need closer spacing
#define BEAM_SEGMENTS 16 // sides of the beam model; even, for its end caps' fans of quads

#define PATH_Z 0.5f // ball radii the middle of the path lies behind the origin
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

static Model3D *Ball;
static Model3D *Beam;
static Model3D *Screen; // stands in for a beam that reaches past the eye
static mat3 HoleFrame[6]; // turns the beam model's +z onto each hole's axis
static unsigned char Wall[RETRO_WIDTH * RETRO_HEIGHT]; // RED_START or BLUE_START, per pixel

static mat3 BallMatrix; // R, this frame
static mat3 BallInverse; // R^T, shared by all ball fragments
static float Light[RETRO_WIDTH * RETRO_HEIGHT];
static vec3 BallCentre; // C, in view space, this frame
static vec3 BeamAxis; // a, unit, in view space, of the beam being drawn
static float BallDepth[RETRO_WIDTH * RETRO_HEIGHT]; // the depth buffer as the ball left it, q per pixel, 0 off the ball

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

static unsigned char ShadeBall(const Fragment &fragment)
{
	vec3 normal = normalize(fragment.position - BallCentre);
	vec3 q = BallInverse * normal;
	float facing = -dot(normal, normalize(fragment.view));
	float shade = BALL_SHINE * facing * facing * facing * facing;

	// The rim of a hole, blended over a pixel. A pixel spans 1 / (focal q)
	// of the surface across the view, and 1 / facing times that along the
	// slope, and max |q| climbs by sin HOLE_ANGLE a radian there
	float pixel = 1.0f / (RETRO_PROJECTION_SCALE * RETRO_PROJECTION_EYEDISTANCE * fragment.q * MAX(facing, 0.1f));
	float rim = 0.5f * sinf(HOLE_ANGLE) * pixel;
	float hole = smoothstep(cosf(HOLE_ANGLE) - rim, cosf(HOLE_ANGLE) + rim, MAX(fabs(q.x), MAX(fabs(q.y), fabs(q.z))));
	shade += (1.0f - shade) * hole;
	return GREY_START + (int)(shade * (LIGHT_LEVELS - 1) + 0.5f);
}

static double PreciseDot(vec3 a, vec3 b)
{
	return (double)a.x * b.x + (double)a.y * b.y + (double)a.z * b.z;
}

// Clip a view ray to the beam's axial slab, outer cone and opaque ball.
static bool IntersectBeam(vec3 eye, vec3 direction, float balldepth, float &near, float &far)
{
	near = 0.0f;
	far = balldepth > 0.0f ? 1.0f / (balldepth * RETRO_PROJECTION_SCALE * direction.z) : 1.0e9f;
	vec3 fromapex = eye - (BallCentre + BeamAxis * BEAM_APEX);
	float axialorigin = dot(fromapex, BeamAxis);
	float axialdirection = dot(direction, BeamAxis);
	float start = -BEAM_APEX;
	float end = BEAM_LENGTH - BEAM_APEX;
	if (fabs(axialdirection) < 1.0e-6f) {
		if (axialorigin < start || axialorigin > end) return false;
	} else {
		float t0 = (start - axialorigin) / axialdirection;
		float t1 = (end - axialorigin) / axialdirection;
		near = MAX(near, MIN(t0, t1));
		far = MIN(far, MAX(t0, t1));
	}
	if (far <= near) return false;

	// Double precision and paired roots avoid cancellation near parallel rays.
	double spread = BEAM_SPREAD * (1.0f + BEAM_SOFTNESS);
	double k = 1.0 + spread * spread;
	double directionsquared = PreciseDot(direction, direction);
	double ua = PreciseDot(fromapex, BeamAxis);
	double da = PreciseDot(direction, BeamAxis);
	double a = directionsquared - k * da * da;
	double b = 2.0 * (PreciseDot(fromapex, direction) - k * ua * da);
	double c = PreciseDot(fromapex, fromapex) - k * ua * ua;
	if (fabs(a) <= 1.0e-7 * directionsquared) {
		// Along a cone generator the quadratic becomes b*t + c <= 0.
		if (b == 0.0) return c <= 0.0;
		float root = -c / b;
		if (b > 0.0) far = MIN(far, root);
		else near = MAX(near, root);
	} else {
		double discriminant = b * b - 4.0 * a * c;
		if (discriminant <= 0.0) return a < 0.0;
		double rootterm = -0.5 * (b + copysign(sqrt(discriminant), b));
		double root0 = rootterm / a;
		double root1 = c / rootterm;
		float entry = MIN(root0, root1);
		float exit = MAX(root0, root1);
		if (a > 0.0) {
			near = MAX(near, entry);
			far = MIN(far, exit);
		} else if (da > 0.0) {
			// The axial slab selects the positive half of the double cone.
			near = MAX(near, exit);
		} else {
			far = MIN(far, entry);
		}
	}
	return far > near;
}

static float IntegrateBeam(vec3 eye, vec3 direction, float near, float far)
{
	float raylength = length(direction);
	vec3 fromcentre = eye - BallCentre;
	float axialorigin = dot(eye - (BallCentre + BeamAxis * BEAM_APEX), BeamAxis);
	float axialdirection = dot(direction, BeamAxis);
	// Resolve the soft edge even on long rays looking towards a hole.
	// Width is linear along the ray, so its minimum is at an endpoint.
	float nearwidth = BEAM_SPREAD * (axialorigin + near * axialdirection);
	float farwidth = BEAM_SPREAD * (axialorigin + far * axialdirection);
	float samplespacing = BEAM_SOFTNESS * MIN(nearwidth, farwidth);
	int samples = MAX(BEAM_SAMPLES, (int)ceil((far - near) * raylength / samplespacing));
	float step = (far - near) / samples;
	float light = 0.0f;
	for (int i = 0; i < samples; i++) {
		float t = near + (i + 0.5f) * step;
		vec3 point = fromcentre + direction * t;
		float distancesquared = dot(point, point);
		float distance = sqrt(distancesquared);
		float along = dot(point, BeamAxis);
		float radial = sqrt(MAX(distancesquared - along * along, 0.0f));
		float width = BEAM_SPREAD * (axialorigin + t * axialdirection);
		float haze = 1.0f - smoothstep(1.0f - BEAM_SOFTNESS, 1.0f + BEAM_SOFTNESS, radial / width);
		float fade = 1.0f - smoothstep(BEAM_FADE * BEAM_LENGTH, BEAM_LENGTH, distance);
		light += haze * fade / (2.0f * width);
	}
	return BEAM_DENSITY * light * step * raylength;
}

static unsigned char ShadeBeam(const Fragment &fragment)
{
	int pixel = fragment.y * RETRO_WIDTH + fragment.x;
	vec3 eye = fragment.position - fragment.view;
	float near, far;
	if (IntersectBeam(eye, fragment.view, BallDepth[pixel], near, far)) {
		Light[pixel] += IntegrateBeam(eye, fragment.view, near, far);
	}
	return RETRO.framebuffer[pixel]; // Palette conversion happens after all beams.
}

// A cone truncated at the centre and at BEAM_LENGTH, along +z, closed at
// both ends, as ShadeBeam cuts it. Its sides are drawn out to the far side of the soft edge, and a little
// more, 1 / cos(pi / BEAM_SEGMENTS), so the polygons cover the whole of the
// round beam ShadeBeam finds in them
static void BuildBeam(Model3D *model)
{
	float z0 = 0.0f;
	float z1 = BEAM_LENGTH;
	float grow = (1.0f + BEAM_SOFTNESS) / cosf(M_PI / BEAM_SEGMENTS);
	int hole[BEAM_SEGMENTS], end[BEAM_SEGMENTS];
	for (int i = 0; i < BEAM_SEGMENTS; i++) {
		float angle = 2.0f * M_PI * i / BEAM_SEGMENTS;
		float r0 = grow * BEAM_SPREAD * (z0 - BEAM_APEX);
		float r1 = grow * BEAM_SPREAD * (z1 - BEAM_APEX);
		hole[i] = RETRO_AddModelVertex(model, r0 * cosf(angle), r0 * sinf(angle), z0);
		end[i] = RETRO_AddModelVertex(model, r1 * cosf(angle), r1 * sinf(angle), z1);
	}

	// The sides, and each cap as a fan of quads from its first corner, all
	// wound so that (v1 - v0) x (v2 - v0) points out
	for (int i = 0; i < BEAM_SEGMENTS; i++) {
		int j = (i + 1) % BEAM_SEGMENTS;
		RETRO_AddModelQuad(model, hole[i], hole[j], end[j], end[i]);
	}
	for (int i = 1; i + 2 < BEAM_SEGMENTS; i += 2) {
		RETRO_AddModelQuad(model, end[0], end[i], end[i + 1], end[i + 2]);
		RETRO_AddModelQuad(model, hole[0], hole[i + 2], hole[i + 1], hole[i]);
	}
	RETRO_InitializeFaceNormals(model);
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
	memcpy(RETRO.framebuffer, Wall, sizeof(Wall));

	float ax = fmod(time * SPIN_SPEED_X, 2 * M_PI);
	float ay = fmod(time * SPIN_SPEED_Y, 2 * M_PI);
	float az = fmod(time * SPIN_SPEED_Z, 2 * M_PI);
	BallMatrix = rotate(ax, ay, az);
	BallInverse = transpose(BallMatrix);

	// The ball's path is a Lissajous figure; whole multiples of one phase
	// keep it closed, so the wrap is seamless. Where it comes nearest the
	// eye, at phase 3/2 pi, it passes the middle of the screen
	float phase = fmod(time * PATH_SPEED, 2 * M_PI);
	BallCentre = { PATH_WIDTH * sinf(2 * phase), PATH_HEIGHT * cosf(3 * phase), PATH_Z + PATH_DEPTH * sinf(phase) };

	// Draw ball
	RETRO_RotateModel(BallMatrix, Ball);
	RETRO_TranslateModel(BallCentre.x, BallCentre.y, BallCentre.z, Ball);
	RETRO_ProjectModel(RETRO_PROJECTION_SCALE, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, Ball);
	RETRO_RenderModel(RETRO_POLY_SHADER, RETRO_SHADE_NONE, Ball);
	memcpy(BallDepth, RETRO_DepthBuffer, sizeof(BallDepth));

	memset(Light, 0, sizeof(Light));
	// Draw beams. Each is a depth range of its own, cleared as it is drawn:
	// ShadeBeam hides what lies behind the ball itself
	for (const mat3 &frame : HoleFrame) {
		mat3 matrix = BallMatrix * frame;
		BeamAxis = matrix * vec3{ 0.0f, 0.0f, 1.0f };
		RETRO_RotateModel(matrix, Beam);
		RETRO_TranslateModel(BallCentre.x, BallCentre.y, BallCentre.z, Beam);
		RETRO_ProjectModel(RETRO_PROJECTION_SCALE, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, Beam);
		bool pasteye = false;
		for (int i = 0; i < Beam->vertices; i++) {
			if (Beam->vertex[i].q <= 0.0f) pasteye = true;
		}
		RETRO_RenderModel(RETRO_POLY_SHADER, RETRO_SHADE_FLAT, pasteye ? Screen : Beam);
	}

	// Quantize only once, so faint and overlapping beams retain their light.
	for (int pixel = 0; pixel < RETRO_WIDTH * RETRO_HEIGHT; pixel++) {
		int color = RETRO.framebuffer[pixel];
		int ramp = color / LIGHT_LEVELS * LIGHT_LEVELS;
		float lit = (float)(color - ramp) / (LIGHT_LEVELS - 1);
		lit = 1.0f - (1.0f - lit) * expf(-Light[pixel]);
		int level = (int)(lit * (LIGHT_LEVELS - 1) + 0.5f);
		RETRO.framebuffer[pixel] = ramp + CLAMP(level, 0, LIGHT_LEVELS);
	}
}

void DEMO_Initialize(void)
{
	BuildPalette();

	BuildWall();

	// +z, -z, +x, -x, +y, -y; rotateY turns +z towards +x, rotateX towards -y
	HoleFrame[0] = identity();
	HoleFrame[1] = rotateX(M_PI);
	HoleFrame[2] = rotateY(M_PI / 2);
	HoleFrame[3] = rotateY(-M_PI / 2);
	HoleFrame[4] = rotateX(-M_PI / 2);
	HoleFrame[5] = rotateX(M_PI / 2);

	Ball = RETRO_Load3DModel("assets/spherequads.obj");
	Ball->shader = ShadeBall;

	Beam = RETRO_Allocate3DModel();
	BuildBeam(Beam);
	Beam->shader = ShadeBeam;

	// A quad one ball radius in front of the eye, facing it, a little wider
	// than the screen it covers there; it never moves, so it is placed once
	float depth = 1.0f;
	float x = 1.1f * (RETRO_WIDTH / 2.0f) * depth / RETRO_PROJECTION_EYEDISTANCE;
	float y = 1.1f * (RETRO_HEIGHT / 2.0f) * depth / RETRO_PROJECTION_EYEDISTANCE;
	float z = depth - (float)RETRO_PROJECTION_EYEDISTANCE / RETRO_PROJECTION_SCALE;
	Screen = RETRO_Allocate3DModel();
	int topleft = RETRO_AddModelVertex(Screen, -x, -y, z);
	int bottomleft = RETRO_AddModelVertex(Screen, -x, y, z);
	int bottomright = RETRO_AddModelVertex(Screen, x, y, z);
	int topright = RETRO_AddModelVertex(Screen, x, -y, z);
	RETRO_AddModelQuad(Screen, topleft, bottomleft, bottomright, topright);
	RETRO_InitializeFaceNormals(Screen);
	RETRO_RotateModel(identity(), Screen);
	RETRO_ProjectModel(RETRO_PROJECTION_SCALE, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, Screen);
	Screen->shader = ShadeBeam;
}
