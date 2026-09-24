//
// God rays
//
// A rotating black ball with six glowing holes and polygonal light beams.
// The ball uses a cos^4 highlight; raised spherical caps form its holes.
// A static, noisy spiral supplies the red wall and blue haze behind it.
//
// Beams are adjoining closed frusta, with nested shells for a soft radial
// profile. Density interpolates between section ends, so joins stay smooth.
// Each surface subtracts entry light or adds exit light,
// clipped at the ball's depth. The result is the visible thickness of haze.
// Near-plane clipping keeps this valid even when the eye enters a beam.
//
// Light accumulates in floating point and is mapped to red, blue and grey
// palette ramps using 1 - exp(-light), approaching white as it brightens.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"

#define HOLE_ANGLE 0.41f // radians from an axis to the rim of its hole
#define HOLE_SEGMENTS 24 // corners around a cap
#define HOLE_RINGS 3 // rings from a cap's middle out to its rim
#define BALL_SHINE 0.38f // brightness of the ball where it faces the eye
#define BALL_LIFT 1.01f // the caps' radius; the ball's facets lie inside radius 1

#define BEAM_SPREAD 0.15f // ball radii a beam widens by for every radius it travels
#define BEAM_LENGTH 9.0f // ball radii from the lamp to the end of a beam
#define BEAM_FADE 0.5f // share of the beam's length before its end starts to fade
#define BEAM_SECTIONS 12 // adjoining sections along each beam
#define BEAM_SHELLS 24 // nested volumes approximating the radial profile
#define BEAM_SEGMENTS 16 // corners around each beam section
#define BEAM_CORE 0.6f // share of a beam's radius its bright core spans
#define BEAM_EDGE 0.85f // brightness at the core's rim, of the middle's
#define BEAM_DENSITY 0.62f // calibrated for this radial profile to match the other demos side-on
#define BEAM_NEAR_DEPTH 1.0f // matches RETRO_ProjectVertex's near plane

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

#define LIGHT_LEVELS 78 // no light through white, per ramp
#define BALL_SHADES 20 // the ball's own shades
#define GREY_START 0
#define RED_START (GREY_START + LIGHT_LEVELS)
#define BLUE_START (RED_START + LIGHT_LEVELS)
#define BALL_START (BLUE_START + LIGHT_LEVELS) // 3 x 78 + 20 is the palette but for two colours

struct BeamFrustum {
	float start, end; // distances along the beam axis
	float startwidth, endwidth;
	float startdensity, enddensity;
};

// A linear density field: value at the eye and change per model-space unit.
struct BeamFog {
	float ateye;
	vec3 gradient;
};

static BeamFrustum Beam[BEAM_SECTIONS * BEAM_SHELLS];
static vec3 BeamRadial[BEAM_SEGMENTS]; // unit circle shared by all beams

static Model3D *Ball;
static Model3D *Caps;
static mat3 HoleFrame[6]; // turns +z onto each hole's axis
static unsigned char Wall[RETRO_WIDTH * RETRO_HEIGHT]; // RED_START or BLUE_START, per pixel

static float RayLength[RETRO_WIDTH * RETRO_HEIGHT]; // view-axis distance to distance along the pixel ray
static float Light[RETRO_WIDTH * RETRO_HEIGHT]; // light each pixel has taken this frame
static unsigned char Base[RETRO_COLORS]; // the ramp a palette entry is lit on
static unsigned char Level[RETRO_COLORS]; // and how far up it already is

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

// The six caps, each a disc of the sphere about its axis: a middle, then
// HOLE_RINGS rings out to HOLE_ANGLE, the middle closed by triangles, quads
// whose last corner repeats. All are wound so that (v1 - v0) x (v2 - v0)
// points out of the ball
static void BuildCaps(Model3D *model)
{
	for (const mat3 &frame : HoleFrame) {
		int middle = RETRO_AddModelVertex(model, 0.0f, 0.0f, 0.0f);
		model->vertex[middle].pos = frame * vec3{ 0.0f, 0.0f, BALL_LIFT };
		int ring[HOLE_RINGS + 1][HOLE_SEGMENTS];
		for (int k = 1; k <= HOLE_RINGS; k++) {
			float angle = HOLE_ANGLE * k / HOLE_RINGS;
			for (int i = 0; i < HOLE_SEGMENTS; i++) {
				float around = 2.0f * M_PI * i / HOLE_SEGMENTS;
				vec3 p = vec3{ sinf(angle) * cosf(around), sinf(angle) * sinf(around), cosf(angle) } * BALL_LIFT;
				ring[k][i] = RETRO_AddModelVertex(model, 0.0f, 0.0f, 0.0f);
				model->vertex[ring[k][i]].pos = frame * p;
			}
		}
		for (int i = 0; i < HOLE_SEGMENTS; i++) {
			int j = (i + 1) % HOLE_SEGMENTS;
			RETRO_AddModelQuad(model, middle, ring[1][i], ring[1][j], ring[1][j]);
			for (int k = 1; k < HOLE_RINGS; k++) {
				RETRO_AddModelQuad(model, ring[k][i], ring[k + 1][i], ring[k + 1][j], ring[k][j]);
			}
		}
	}
	RETRO_InitializeFaceNormals(model);
}

// A closed volume contributes integrated light at exit minus that at entry.
// Stop both at opaque depth so only haze in front of the ball contributes.
static void AddVolumeTriangle(const PolygonPoint *p0, const PolygonPoint *p1, const PolygonPoint *p2, const BeamFog &fog)
{
	TriangleSpan span[RETRO_HEIGHT];
	int ystart, yend;
	float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend);
	if (determinant == 0.0f) return;

	float dqdx = ((p1->q - p0->q) * (p2->pos.y - p0->pos.y) - (p2->q - p0->q) * (p1->pos.y - p0->pos.y)) / determinant;
	float dqdy = ((p1->pos.x - p0->pos.x) * (p2->q - p0->q) - (p2->pos.x - p0->pos.x) * (p1->q - p0->q)) / determinant;
	// Outward winding gives positive screen area on exits, negative on entries.
	float weight = copysignf(1.0f / RETRO_PROJECTION_SCALE, determinant);
	float slopestep = fog.gradient.x / RETRO_PROJECTION_EYEDISTANCE;
	for (int y = ystart; y < yend; y++) {
		if (span[y].left > span[y].right) continue;
		int xstart = MAX((int)ceil(span[y].left - 0.5f), 0);
		int xend = MIN((int)ceil(span[y].right - 0.5f), RETRO_WIDTH);
		float q = p0->q + dqdx * (xstart + 0.5f - p0->pos.x) + dqdy * (y + 0.5f - p0->pos.y);
		float slope = fog.gradient.z
			+ fog.gradient.x * (xstart + 0.5f - RETRO_WIDTH / 2.0f) / RETRO_PROJECTION_EYEDISTANCE
			+ fog.gradient.y * (y + 0.5f - RETRO_HEIGHT / 2.0f) / RETRO_PROJECTION_EYEDISTANCE;
		for (int x = xstart; x < xend; x++, q += dqdx, slope += slopestep) {
			int offset = y * RETRO_WIDTH + x;
			float visibleq = MAX(q, RETRO_DepthBuffer[offset]);
			if (visibleq <= 0.0f) continue;
			float depth = 1.0f / visibleq;
			float distance = MAX(depth - BEAM_NEAR_DEPTH, 0.0f);
			// The integral of linear density is distance times its midpoint value.
			float averagedensity = fog.ateye + slope * (depth + BEAM_NEAR_DEPTH) / (2.0f * RETRO_PROJECTION_SCALE);
			Light[offset] += weight * RayLength[offset] * distance * averagedensity;
		}
	}
}

// Project one clipped corner onto the end of the output. Returns the new count.
static int AppendBeamPoint(PolygonPoint *output, int count, vec3 pos, float depth)
{
	const float focal = RETRO_PROJECTION_SCALE * RETRO_PROJECTION_EYEDISTANCE;
	PolygonPoint &point = output[count];
	point.q = 1.0f / depth;
	point.pos = { RETRO_WIDTH / 2.0f + focal * pos.x * point.q,
		RETRO_HEIGHT / 2.0f + focal * pos.y * point.q };
	return count + 1;
}

// Clip before projection; a triangle can become a quad. Distances are
// measured from this plane, so its missing cap would contribute zero even
// when the camera is inside a beam.
static int ProjectBeamTriangle(const vec3 *input, PolygonPoint *output)
{
	int count = 0;
	vec3 previous = input[2];
	float previousdepth = RETRO_PROJECTION_SCALE * previous.z + RETRO_PROJECTION_EYEDISTANCE;
	for (int i = 0; i < 3; i++) {
		vec3 current = input[i];
		float depth = RETRO_PROJECTION_SCALE * current.z + RETRO_PROJECTION_EYEDISTANCE;
		if ((previousdepth >= BEAM_NEAR_DEPTH) != (depth >= BEAM_NEAR_DEPTH)) {
			float t = (BEAM_NEAR_DEPTH - previousdepth) / (depth - previousdepth);
			count = AppendBeamPoint(output, count, previous + (current - previous) * t, BEAM_NEAR_DEPTH);
		}
		if (depth >= BEAM_NEAR_DEPTH) count = AppendBeamPoint(output, count, current, depth);
		previous = current;
		previousdepth = depth;
	}
	return count;
}

static void DrawVolumeTriangle(vec3 a, vec3 b, vec3 c, const BeamFog &fog)
{
	vec3 input[] = { a, b, c };
	PolygonPoint point[4];
	int count = ProjectBeamTriangle(input, point);
	for (int i = 1; i + 1 < count; i++) {
		AddVolumeTriangle(&point[0], &point[i], &point[i + 1], fog);
	}
}

static float BeamProfile(float radius)
{
	if (radius <= BEAM_CORE) return 1.0f - (1.0f - BEAM_EDGE) * radius / BEAM_CORE;
	return BEAM_EDGE * MAX(1.0f - radius, 0.0f) / (1.0f - BEAM_CORE);
}

// All six beams share these dimensions and densities; only their transforms
// change during animation. Store shell contributions as closed frusta.
static void BuildBeam(void)
{
	for (int i = 0; i < BEAM_SEGMENTS; i++) {
		float angle = 2.0f * M_PI * i / BEAM_SEGMENTS;
		BeamRadial[i] = { cosf(angle), sinf(angle), 0.0f };
	}
	float start = cosf(HOLE_ANGLE);
	float step = (BEAM_LENGTH - start) / BEAM_SECTIONS;
	for (int section = 0; section < BEAM_SECTIONS; section++) {
		float sectionstart = start + section * step;
		float sectionend = start + (section + 1) * step;
		float startwidth = sinf(HOLE_ANGLE) + BEAM_SPREAD * (sectionstart - start);
		float endwidth = sinf(HOLE_ANGLE) + BEAM_SPREAD * (sectionend - start);
		float startfade = 1.0f - smoothstep(BEAM_FADE * BEAM_LENGTH, BEAM_LENGTH, sectionstart);
		float endfade = 1.0f - smoothstep(BEAM_FADE * BEAM_LENGTH, BEAM_LENGTH, sectionend);
		for (int shell = 1; shell <= BEAM_SHELLS; shell++) {
			float radius = (float)shell / BEAM_SHELLS;
			float outerdensity = shell == BEAM_SHELLS ? 0.0f : BeamProfile((shell + 0.5f) / BEAM_SHELLS);
			float shelldensity = BeamProfile((shell - 0.5f) / BEAM_SHELLS) - outerdensity;
			Beam[section * BEAM_SHELLS + shell - 1] = {
				sectionstart, sectionend, radius * startwidth, radius * endwidth,
				BEAM_DENSITY * startfade / startwidth * shelldensity,
				BEAM_DENSITY * endfade / endwidth * shelldensity
			};
		}
	}
}

// Both caps and the sides are wound outward for signed volume accumulation.
static void DrawFrustum(const BeamFrustum &frustum, const vec3 *radial, vec3 startcentre, vec3 endcentre, vec3 axis)
{
	vec3 gradient = axis * ((frustum.enddensity - frustum.startdensity) / (frustum.end - frustum.start));
	vec3 eye = { 0.0f, 0.0f, -(float)RETRO_PROJECTION_EYEDISTANCE / RETRO_PROJECTION_SCALE };
	BeamFog fog = { frustum.startdensity + dot(gradient, eye - startcentre), gradient };
	vec3 front[BEAM_SEGMENTS], back[BEAM_SEGMENTS];
	for (int i = 0; i < BEAM_SEGMENTS; i++) {
		front[i] = startcentre + radial[i] * frustum.startwidth;
		back[i] = endcentre + radial[i] * frustum.endwidth;
	}
	for (int i = 0; i < BEAM_SEGMENTS; i++) {
		int next = (i + 1) % BEAM_SEGMENTS;
		DrawVolumeTriangle(startcentre, front[next], front[i], fog);
		DrawVolumeTriangle(endcentre, back[i], back[next], fog);
		DrawVolumeTriangle(front[i], front[next], back[next], fog);
		DrawVolumeTriangle(front[i], back[next], back[i], fog);
	}
}

static void DrawBeam(const mat3 &frame, vec3 centre)
{
	vec3 radial[BEAM_SEGMENTS];
	for (int i = 0; i < BEAM_SEGMENTS; i++) radial[i] = frame * BeamRadial[i];
	for (const BeamFrustum &frustum : Beam) {
		vec3 startcentre = centre + frame.col2 * frustum.start;
		vec3 endcentre = centre + frame.col2 * frustum.end;
		DrawFrustum(frustum, radial, startcentre, endcentre, frame.col2);
	}
}

static void BuildPalette(void)
{
	// Init palette. Each ramp adds white light to its colour, a level at a
	// time, and saturates into white. The ball's shades are BALL_SHINE cos^4
	// of the angle each stands for, lit on the grey ramp
	RETRO_Palette palette[RETRO_COLORS] = {};
	const RETRO_Palette rampcolors[] = { { 0, 0, 0 }, WALL_RED, WALL_BLUE };
	int rampstart = GREY_START;
	for (const RETRO_Palette &color : rampcolors) {
		for (int level = 0; level < LIGHT_LEVELS; level++) {
			int light = level * 255 / (LIGHT_LEVELS - 1);
			int index = rampstart + level;
			RETRO_SetColor(index, RETRO_Palette{
				(unsigned char)MIN(color.r + light, 255),
				(unsigned char)MIN(color.g + light, 255),
				(unsigned char)MIN(color.b + light, 255)
			}, palette);
			Base[index] = rampstart;
			Level[index] = level;
		}
		rampstart += LIGHT_LEVELS;
	}
	for (int i = 0; i < BALL_SHADES; i++) {
		float angle = (1.0f - (float)i / (BALL_SHADES - 1)) * M_PI / 2;
		int level = (int)(BALL_SHINE * powf(cosf(angle), 4.0f) * (LIGHT_LEVELS - 1) + 0.5f);
		RETRO_SetColor(BALL_START + i, palette[GREY_START + level], palette);
		Base[BALL_START + i] = GREY_START;
		Level[BALL_START + i] = level;
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
	mat3 matrix = rotate(ax, ay, az);

	// The ball's path is a Lissajous figure; whole multiples of one phase
	// keep it closed, so the wrap is seamless. Where it comes nearest the
	// eye, at phase 3/2 pi, it passes the middle of the screen
	float phase = fmod(time * PATH_SPEED, 2 * M_PI);
	vec3 centre = { PATH_WIDTH * sinf(2 * phase), PATH_HEIGHT * cosf(3 * phase), PATH_Z + PATH_DEPTH * sinf(phase) };

	// Draw ball, and its holes over it
	RETRO_RotateModel(matrix, Ball);
	RETRO_TranslateModel(centre.x, centre.y, centre.z, Ball);
	RETRO_ProjectModel(RETRO_PROJECTION_SCALE, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, Ball);
	RETRO_RenderModel(RETRO_POLY_PHONG, RETRO_SHADE_NONE, Ball);
	RETRO_RotateModel(matrix, Caps);
	RETRO_TranslateModel(centre.x, centre.y, centre.z, Caps);
	RETRO_ProjectModel(RETRO_PROJECTION_SCALE, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, Caps);
	RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_NONE, Caps, false);

	// Integrate the light through the polygonal beam volumes.
	memset(Light, 0, sizeof(Light));
	for (const mat3 &hole : HoleFrame) {
		DrawBeam(matrix * hole, centre);
	}

	// Lay the light over the picture
	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		if (Light[i] <= 0.0f) continue;
		int color = RETRO.framebuffer[i];
		float lit = 1.0f - (1.0f - (float)Level[color] / (LIGHT_LEVELS - 1)) * expf(-Light[i]);
		RETRO.framebuffer[i] = Base[color] + (int)(lit * (LIGHT_LEVELS - 1) + 0.5f);
	}
}

void DEMO_Initialize(void)
{
	BuildBeam();

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float dx = (x + 0.5f - RETRO_WIDTH / 2.0f) / RETRO_PROJECTION_EYEDISTANCE;
			float dy = (y + 0.5f - RETRO_HEIGHT / 2.0f) / RETRO_PROJECTION_EYEDISTANCE;
			RayLength[y * RETRO_WIDTH + x] = sqrtf(1.0f + dx * dx + dy * dy);
		}
	}

	BuildPalette();

	BuildWall();

	// +z, -z, +x, -x, +y, -y; rotateY turns +z towards +x, rotateX towards -y
	HoleFrame[0] = identity();
	HoleFrame[1] = rotateX(M_PI);
	HoleFrame[2] = rotateY(M_PI / 2);
	HoleFrame[3] = rotateY(-M_PI / 2);
	HoleFrame[4] = rotateX(-M_PI / 2);
	HoleFrame[5] = rotateX(M_PI / 2);

	// Initialize light source at the eye
	RETRO_InitializeLightSource(0, 0, -1);

	Ball = RETRO_Load3DModel("assets/spherequads.obj");
	Ball->c = BALL_START;
	Ball->shades = BALL_SHADES;

	Caps = RETRO_Allocate3DModel();
	BuildCaps(Caps);
	Caps->c = GREY_START + LIGHT_LEVELS - 1;
}
