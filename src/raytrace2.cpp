//
// Reflective raytraced spheres
//
// Three moving chrome spheres over a shadowed checkerboard. Every ray finds
// the nearest sphere or floor; reflected rays repeat that same scene query,
// so the balls reflect one another as well as the floor and sky. Throughput
// falls at each bounce. A fixed bounce limit bounds work between mirrors.
// Camera directions are normalized per pixel, at pixel centres.
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

#define SPHERES 3
#define MAX_BOUNCES 6
#define SPHERE_Z 340.0f // world z the sphere drifts and bounces at
#define MIRROR_REFLECTIVITY 0.92f // short of 1, so the ball reads darker than what it reflects

#define LIGHT_ORBIT_RADIUS 260.0f
#define LIGHT_Y 170.0f // above the camera, so it can be seen in the sky
#define SUN_SHININESS 200.0f // higher is a tighter sun disc and highlight

#define NEAR_T 0.001f // t past which a hit counts, so a ray does not re-hit its own origin

#define SKY_START 0
#define SKY_SHADES 48
#define FLOOR_START (SKY_START + SKY_SHADES)
#define FLOOR_SHADES 64
#define MIRROR_START (FLOOR_START + FLOOR_SHADES)
#define MIRROR_SHADES 128

struct Sphere { vec3 center; float radius; };
static Sphere Balls[SPHERES];

// The nearer root of |O + tD - C|^2 = r^2, D unit. allowinside also returns
// the far root when the near one is behind the origin, which a reflection
// bounce needs when it starts fractionally inside the sphere it just left -
// but a shadow ray, always cast from outside every sphere, must not get: a
// graze it should clear would otherwise read as a hit on the sphere's far
// side.
static bool IntersectSphere(const Sphere &sphere, vec3 origin, vec3 dir, float &t, bool allowinside = true)
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

static bool SphereShadowed(vec3 origin, vec3 dir, float maxdist)
{
	for (const Sphere &sphere : Balls) {
		float t;
		if (IntersectSphere(sphere, origin, dir, t, false) && t < maxdist) return true;
	}
	return false;
}

struct EnvironmentHit {
	bool floor; // true if the floor was hit, false for the sky
	float brightness; // 0..1, already carrying the tile's own albedo
};

// The floor and sky are seen by primary, reflection and shadow rays alike.
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
	float throughput = 1;
	for (int bounce = 0; bounce < MAX_BOUNCES; bounce++) {
		float nearest = 1e30f;
		if (dir.y < -NEAR_T) {
			float floorhit = (FLOOR_Y - origin.y) / dir.y;
			if (floorhit > NEAR_T) nearest = floorhit;
		}
		int hit = -1;
		for (int i = 0; i < SPHERES; i++) {
			float t;
			if (IntersectSphere(Balls[i], origin, dir, t) && t < nearest) {
				nearest = t;
				hit = i;
			}
		}
		if (hit < 0) {
			EnvironmentHit env = TraceEnvironment(origin, dir, light);
			float brightness = CLAMP01(env.brightness * throughput);
			if (bounce > 0) return MIRROR_START + (int)(brightness * (MIRROR_SHADES - 1));
			return env.floor ? FLOOR_START + (int)(brightness * (FLOOR_SHADES - 1))
				: SKY_START + (int)(brightness * (SKY_SHADES - 1));
		}
		vec3 p = origin + dir * nearest;
		vec3 normal = normalize(p - Balls[hit].center);
		dir = normalize(dir - normal * (2 * dot(dir, normal)));
		origin = p + normal * 0.01f;
		throughput *= MIRROR_REFLECTIVITY;
	}
	// Unresolved deep mirror paths receive no invented environment hit.
	return MIRROR_START;
}

void DEMO_Render(double time, double deltatime)
{
	double phase = fmod(time, 20.0) * (2 * M_PI / 20.0);
	Balls[0] = {{-83.0f + 12.0f * (float)sin(phase), FLOOR_Y + 62.0f, 325.0f + 20.0f * (float)cos(phase)}, 62.0f};
	Balls[1] = {{ 83.0f + 12.0f * (float)sin(phase + M_PI), FLOOR_Y + 62.0f, 350.0f - 20.0f * (float)cos(phase)}, 62.0f};
	Balls[2] = {{24.0f * (float)sin(phase), FLOOR_Y + 72.0f + 48.0f * (float)(1 - cos(phase)), 480.0f}, 72.0f};
	vec3 light = {LIGHT_ORBIT_RADIUS * (float)cos(phase), LIGHT_Y, SPHERE_Z + LIGHT_ORBIT_RADIUS * (float)sin(phase)};
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			vec3 origin = { 0, 0, 0 };
			vec3 dir = normalize(vec3{ x + 0.5f - RETRO_WIDTH / 2.0f, RETRO_HEIGHT / 2.0f - y - 0.5f, FOCAL });
			unsigned char color = TraceScene(origin, dir, light);
			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_CreateGradientPalette(SKY_START, SKY_START + SKY_SHADES, RETRO_MIDNIGHTBLUE, RETRO_WHITE);
	RETRO_CreateGradientPalette(FLOOR_START, FLOOR_START + FLOOR_SHADES, RETRO_BLACK, RETRO_WHITE);
	RETRO_CreateGradientPalette(MIRROR_START, MIRROR_START + MIRROR_SHADES, RETRO_BLACK, RETRO_WHITE);
}
