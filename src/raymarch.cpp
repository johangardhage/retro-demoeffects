//
// Raymarch
//
// raytrace.cpp's own scene and its own sphere, down to the position
// and bounce formulas, so the two are directly comparable frame for frame -
// what differs is only how the ray finds the sphere.
//
// raytrace.cpp solves |O + t D - C|^2 = r^2 for t in closed form.
// Here the ray is walked instead: the sphere's signed distance field
//
//   SDF(p) = |p - C| - r
//
// bounds how far p is from the surface, so stepping by exactly that
// distance can never step through it,
//
//   t += SDF(O + t D)
//
// until the walk lands within HIT_EPS. For a sphere SDF is an exact bound,
// not merely a fair one, so the walk needs no safety margin and converges
// in a handful of steps - sphere tracing at its cheapest, on the one shape
// simple enough to check the technique against a closed form. The floor and
// sky stay raytrace.cpp's plane equation and sun glow, not marched,
// since a plane already has one.
//
// The surface normal is the field's own gradient, six calls of finite
// difference around the hit rather than the analytic p - C a sphere would
// let a demo take as a shortcut - marched like any other SDF, so the normal
// too is the general technique and not a fact special-cased for this shape.
// From there the mirror shading, and the shadow ray asking whether the
// sphere lies between a floor point and the light, are raytrace.cpp's
// own, marched instead of solved.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retrovector.h"

#define FOCAL 260.0f // pixels; sets the field of view

#define FLOOR_Y -110.0f // world y of the floor, below the camera at the origin
#define TILE_SIZE 70.0f // world units per checker tile
#define FOG_DENSITY 0.0016f // darkens the floor with distance
#define FLOOR_ALBEDO_DARK 0.08f // reflectance of a dark tile
#define FLOOR_ALBEDO_LIGHT 0.85f // and of a light one

#define SPHERE_RADIUS 55.0f
#define SPHERE_Z 340.0f // world z the sphere drifts and bounces at
#define SPHERE_DRIFT_X 130.0f // world units either side of centre
#define SPHERE_DRIFT_SPEED 0.35 // radians a second
#define SPHERE_BOUNCE_HEIGHT 85.0f // world units risen at the top of a bounce
#define SPHERE_BOUNCE_SPEED 0.55 // bounces a second
#define MIRROR_REFLECTIVITY 0.92f // short of 1, so the sphere reads darker than what it reflects

#define LIGHT_ORBIT_RADIUS 260.0f
#define LIGHT_Y 170.0f // above the camera, so it can be seen in the sky
#define LIGHT_SPEED 0.5 // radians a second
#define SUN_SHININESS 200.0f // higher is a tighter sun disc and highlight

#define NEAR_T 0.001f // t past which a hit counts, so a ray does not re-hit its own origin

#define MARCH_STEPS 32
#define MARCH_MAXDIST 1400.0f
#define HIT_EPS 0.1f // a sphere's SDF is exact, so the walk can close in tightly

#define SHADOW_STEPS 32

#define SKY_START 0
#define SKY_SHADES 48
#define FLOOR_START (SKY_START + SKY_SHADES)
#define FLOOR_SHADES 64
#define MIRROR_START (FLOOR_START + FLOOR_SHADES)
#define MIRROR_SHADES 128

static vec3 SphereCenter;

// The exact signed distance from p to the sphere's surface
static float SphereSDF(vec3 p)
{
	return length(p - SphereCenter) - SPHERE_RADIUS;
}

// Sphere tracing against SphereSDF; a ray that would pass behind the floor
// is capped at maxdist, since the sphere never reaches there
static bool MarchSphere(vec3 origin, vec3 dir, float maxdist, float &t)
{
	t = 0.0f;
	for (int i = 0; i < MARCH_STEPS; i++) {
		float d = SphereSDF(origin + dir * t);
		if (d < HIT_EPS) return true;
		t += d;
		if (t > maxdist) break;
	}
	return false;
}

// raytrace.cpp's SphereShadowed, marched instead of solved
static bool SphereOccluded(vec3 origin, vec3 dir, float maxdist)
{
	float t = 0.0f;
	for (int i = 0; i < SHADOW_STEPS && t < maxdist; i++) {
		float d = SphereSDF(origin + dir * t);
		if (d < HIT_EPS) return true;
		t += d;
	}
	return false;
}

// The field's own gradient, by central difference - six calls that make any
// SDF self-shading, with no per-shape normal to derive by hand
static vec3 EstimateNormal(vec3 p)
{
	const float e = 0.6f;
	float dx = SphereSDF(p + vec3{ e, 0, 0 }) - SphereSDF(p - vec3{ e, 0, 0 });
	float dy = SphereSDF(p + vec3{ 0, e, 0 }) - SphereSDF(p - vec3{ 0, e, 0 });
	float dz = SphereSDF(p + vec3{ 0, 0, e }) - SphereSDF(p - vec3{ 0, 0, e });
	return normalize(vec3{ dx, dy, dz });
}

struct EnvironmentHit {
	bool floor; // true if the floor was hit, false for the sky
	float brightness; // 0..1, already carrying the tile's own albedo
};

// raytrace.cpp's TraceEnvironment, unchanged but for asking SphereOccluded
// rather than SphereShadowed whether the floor is lit. floort carries the
// plane hit distance over from DEMO_Render's own march-distance cap, so the
// primary ray's floor division is not paid twice; a fresh ray, such as the
// mirror's reflection, passes -1.0f and this works it out itself.
static EnvironmentHit TraceEnvironment(vec3 origin, vec3 dir, vec3 light, float floort = -1.0f)
{
	EnvironmentHit hit;

	if (floort < 0.0f && dir.y < -NEAR_T) {
		floort = (FLOOR_Y - origin.y) / dir.y;
	}
	if (floort > NEAR_T) {
		vec3 p = origin + dir * floort;
		vec3 tolight = light - p;
		float lightdist = length(tolight);
		vec3 tolightdir = tolight / lightdist;

		float lambert = MAX(dot(vec3{ 0, 1, 0 }, tolightdir), 0.0f);
		bool shadowed = SphereOccluded(p + vec3{ 0, 1, 0 } * 0.5f, tolightdir, lightdist);
		float diffuse = shadowed ? 0.0f : lambert;
		float ambient = 0.22f;
		float fog = expf(-floort * FOG_DENSITY);

		int tilex = (int)floorf(p.x / TILE_SIZE);
		int tilez = (int)floorf(p.z / TILE_SIZE);
		float albedo = ((tilex + tilez) & 1) == 0 ? FLOOR_ALBEDO_LIGHT : FLOOR_ALBEDO_DARK;

		hit.floor = true;
		hit.brightness = CLAMP01(albedo * (ambient + (1.0f - ambient) * diffuse) * fog);
		return hit;
	}

	// Sky: a little brighter toward the horizon than the zenith, plus the
	// light's own glow wherever the ray runs close to its direction
	float horizon = CLAMP01(1.0f - MAX(dir.y, 0.0f) * 1.1f);
	float glow = powf(MAX(dot(dir, normalize(light - origin)), 0.0f), SUN_SHININESS);

	hit.floor = false;
	hit.brightness = CLAMP01(0.15f + 0.35f * horizon + glow);
	return hit;
}

static unsigned char TraceScene(vec3 origin, vec3 dir, vec3 light)
{
	// A ray behind the floor can never reach the sphere, so the march need not
	// walk that far; keep the plane hit so a miss need not divide for it again
	float floort = -1.0f;
	if (dir.y < -NEAR_T) {
		floort = (FLOOR_Y - origin.y) / dir.y;
	}
	float maxdist = floort > NEAR_T ? MIN(MARCH_MAXDIST, floort) : MARCH_MAXDIST;

	float t;
	if (MarchSphere(origin, dir, maxdist, t)) {
		vec3 p = origin + dir * t;
		vec3 normal = EstimateNormal(p);
		vec3 reflected = dir - normal * (2.0f * dot(dir, normal));

		EnvironmentHit env = TraceEnvironment(p + normal * 0.5f, reflected, light);
		float brightness = CLAMP01(env.brightness * MIRROR_REFLECTIVITY);
		return MIRROR_START + (unsigned char)(brightness * (MIRROR_SHADES - 1));
	}

	EnvironmentHit env = TraceEnvironment(origin, dir, light, floort);
	if (env.floor) return FLOOR_START + (unsigned char)(env.brightness * (FLOOR_SHADES - 1));
	return SKY_START + (unsigned char)(env.brightness * (SKY_SHADES - 1));
}

void DEMO_Render(double time, double deltatime)
{
	// raytrace.cpp's own bounce, boingball's parabola: w = 2 frac(phase) - 1,
	// height = PEAK (1 - w^2), and the same side-to-side drift
	float bouncephase = fmod(time * SPHERE_BOUNCE_SPEED, 1.0);
	float w = 2.0f * bouncephase - 1.0f;
	float bounce = SPHERE_BOUNCE_HEIGHT * (1.0f - w * w);

	SphereCenter = { SPHERE_DRIFT_X * sinf(time * SPHERE_DRIFT_SPEED), FLOOR_Y + SPHERE_RADIUS + bounce, SPHERE_Z };

	float lightangle = time * LIGHT_SPEED;
	vec3 light = { LIGHT_ORBIT_RADIUS * cosf(lightangle), LIGHT_Y, SPHERE_Z + LIGHT_ORBIT_RADIUS * sinf(lightangle) };

	for (int sy = 0; sy < RETRO_HEIGHT; sy++) {
		for (int sx = 0; sx < RETRO_WIDTH; sx++) {
			vec3 origin = { 0, 0, 0 };
			vec3 dir = normalize(vec3{ (float)sx - RETRO_WIDTH / 2.0f, RETRO_HEIGHT / 2.0f - (float)sy, FOCAL });
			unsigned char color = TraceScene(origin, dir, light);
			RETRO_PutPixel(sx, sy, color);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_CreateGradientPalette(SKY_START, SKY_START + SKY_SHADES, RETRO_MIDNIGHTBLUE, RETRO_WHITE);
	RETRO_CreateGradientPalette(FLOOR_START, FLOOR_START + FLOOR_SHADES, RETRO_BLACK, RETRO_WHITE);
	RETRO_CreateGradientPalette(MIRROR_START, MIRROR_START + MIRROR_SHADES, RETRO_BLACK, RETRO_WHITE);
}
