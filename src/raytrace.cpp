//
// Raytraced sphere
//
// A true raytracer, not an environment map: every pixel casts its own ray and
// the mirror ball's reflection is found by actually reflecting that ray off
// the surface it hits, so the floor slides across the ball with the parallax
// a lookup table cannot give it. Camera rays are the pinhole
//
//   dir = normalize(sx - W/2,  H/2 - sy,  FOCAL)
//
// from the origin, +Y up and +Z into the screen. A ray hits the sphere where
//
//   |O + t D - C|^2 = r^2
//   t^2 + 2t D.(O-C) + |O-C|^2 - r^2 = 0
//
// solved for the near root; D is unit, so the quadratic's leading term is 1.
// The floor is the plane y = FLOOR_Y, hit at t = (FLOOR_Y - O.y) / D.y, and
// the tile a hit point falls on is the parity of its floor(x / TILE_SIZE) +
// floor(z / TILE_SIZE).
//
// The one light is also the sky's sun: a ray that misses the sphere and the
// floor is shaded by how close it runs to the light's own direction,
//
//   glow = max(0, D . normalize(light - O)) ^ SUN_SHININESS
//
// which paints a bright disc where a camera ray looks straight at it. The
// mirror ball's reflected ray is traced through the exact same function, so
// when that ray's reflection direction happens to line up with the light,
// the same glow term fires and reads as the ball's specular highlight - one
// piece of code, not two, and the highlight already falls at the right spot
// on the curve because it is the light's own reflection, not a cosine lobe
// glued on afterward. A point on the floor asks the same question of a
// shadow ray toward the light, occluded only by the sphere.
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
#define MIRROR_REFLECTIVITY 0.92f // short of 1, so the ball reads darker than what it reflects

#define LIGHT_ORBIT_RADIUS 260.0f
#define LIGHT_Y 170.0f // above the camera, so it can be seen in the sky
#define LIGHT_SPEED 0.5 // radians a second
#define SUN_SHININESS 200.0f // higher is a tighter sun disc and highlight

#define NEAR_T 0.001f // t past which a hit counts, so a ray does not re-hit its own origin

#define SKY_START 0
#define SKY_SHADES 48
#define FLOOR_START (SKY_START + SKY_SHADES)
#define FLOOR_SHADES 64
#define MIRROR_START (FLOOR_START + FLOOR_SHADES)
#define MIRROR_SHADES 128

static vec3 SphereCenter;

// The nearer root of |O + tD - C|^2 = r^2, D unit
static bool IntersectSphere(vec3 origin, vec3 dir, float &t)
{
	vec3 oc = origin - SphereCenter;
	float b = dot(dir, oc);
	float c = dot(oc, oc) - SPHERE_RADIUS * SPHERE_RADIUS;
	float disc = b * b - c;
	if (disc < 0.0f) return false;

	float tt = -b - sqrtf(disc);
	if (tt <= NEAR_T) return false;

	t = tt;
	return true;
}

static bool SphereShadowed(vec3 origin, vec3 dir, float maxdist)
{
	vec3 oc = origin - SphereCenter;
	float b = dot(dir, oc);
	float c = dot(oc, oc) - SPHERE_RADIUS * SPHERE_RADIUS;
	float disc = b * b - c;
	if (disc < 0.0f) return false;

	float t = -b - sqrtf(disc);
	return t > NEAR_T && t < maxdist;
}

struct EnvironmentHit {
	bool floor; // true if the floor was hit, false for the sky
	float brightness; // 0..1, already carrying the tile's own albedo
};

// What a ray sees once the sphere is out of the way: the checkered floor,
// shadowed and fogged, or the sky with the light's own glow riding on it.
// Called both for camera rays that miss the sphere and, with an origin and
// direction on the ball's surface, for the ray its mirror reflects - so a
// tile's albedo is folded into brightness here rather than left to the
// caller, and the mirror ball's reflection shows the checker pattern
// instead of a flat lambert-shaded gray.
static EnvironmentHit TraceEnvironment(vec3 origin, vec3 dir, vec3 light)
{
	EnvironmentHit hit;

	if (dir.y < -NEAR_T) {
		float t = (FLOOR_Y - origin.y) / dir.y;
		if (t > NEAR_T) {
			vec3 p = origin + dir * t;
			vec3 tolight = light - p;
			float lightdist = length(tolight);
			vec3 tolightdir = tolight / lightdist;

			float lambert = MAX(dot(vec3{ 0, 1, 0 }, tolightdir), 0.0f);
			bool shadowed = SphereShadowed(p + vec3{ 0, 1, 0 } * 0.5f, tolightdir, lightdist);
			float diffuse = shadowed ? 0.0f : lambert;
			float ambient = 0.22f;
			float fog = expf(-t * FOG_DENSITY);

			int tilex = (int)floorf(p.x / TILE_SIZE);
			int tilez = (int)floorf(p.z / TILE_SIZE);
			float albedo = ((tilex + tilez) & 1) == 0 ? FLOOR_ALBEDO_LIGHT : FLOOR_ALBEDO_DARK;

			hit.floor = true;
			hit.brightness = CLAMP01(albedo * (ambient + (1.0f - ambient) * diffuse) * fog);
			return hit;
		}
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
	float t;
	if (IntersectSphere(origin, dir, t)) {
		vec3 p = origin + dir * t;
		vec3 normal = normalize(p - SphereCenter);
		vec3 reflected = reflect(dir, normal);

		EnvironmentHit env = TraceEnvironment(p + normal * 0.5f, reflected, light);
		float brightness = CLAMP01(env.brightness * MIRROR_REFLECTIVITY);
		return MIRROR_START + (unsigned char)(brightness * (MIRROR_SHADES - 1));
	}

	EnvironmentHit env = TraceEnvironment(origin, dir, light);
	if (env.floor) return FLOOR_START + (unsigned char)(env.brightness * (FLOOR_SHADES - 1));
	return SKY_START + (unsigned char)(env.brightness * (SKY_SHADES - 1));
}

void DEMO_Render(double time, double deltatime)
{
	// The sphere bounces on boingball's own parabola, w = 2 frac(phase) - 1,
	// height = PEAK (1 - w^2), and drifts back and forth across the floor
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
