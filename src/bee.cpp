//
// Bee
//
// A bee hovering in front of a swirl while the camera flies around it, its
// wings beating, its antennae swaying and its mandibles working. The mesh is
// bee.md3, a surface for each of its 68 parts, held in POSES poses. Only the
// four wings, the two antennae and the mandibles move between them; the rest
// stands the same in every pose. The bee blends from one pose to the next
// every POSE_TIME seconds, the last back to the first, the wings beating
// twice for each sway of the antennae.
//
// Everything is lit by bee_envmap_256x256.pcx, read by the normal
// (RETRO_POLY_MATCAP), ENVMAP_RADIUS texels out from the middle. The normals
// are not turned by the camera but by a lighting turn of their own: the
// camera's yaw and a fixed tilt, with a bob on top that rocks it in pitch
// and, half as far, in yaw. The same bob lifts the camera BOB_HEIGHT either
// way. So the light swings with the camera as it circles the bee but not as
// it looks up and down.
//
// A keyframe table gives the camera's position and angles, and each frame
// mixes the two keyframes it falls between. By the last keyframe the camera
// has turned away and the bee is off the screen, and it all starts again.
//
// Positions are in the mesh's units, in the library's axes, x right, y down
// and z away, the bee facing -z. Angles are in degrees.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromodel.h"
#include "lib/retromatrix.h"
#include "lib/retrorender.h"

constexpr float FOCAL = 512.0f;            // the eye's distance from the screen, in pixels
constexpr float EYE = 30.0f;               // and from the camera, in the mesh's units
constexpr int ENVMAP_RADIUS = 64;          // the lighting map's reach from its middle

constexpr float TICK = 1.0f / 70;          // the timing below is in steps of a 70 Hz clock

constexpr int POSES = 4;
constexpr float POSE_TIME = 8 * TICK;      // seconds from one pose to the next

constexpr float LIGHT_PITCH = 8.8f;        // degrees the lighting is tilted down past the camera
constexpr float LIGHT_YAW = 9.7f;          // and turned past it
constexpr float BOB_PERIOD = 64 * TICK;    // seconds a bob takes
constexpr float BOB_ANGLE = 11.25f;        // degrees the lighting pitches either way, and yaws half as far
constexpr float BOB_HEIGHT = 16.0f;        // units the camera bobs either way

// Where the camera stands and how it is turned, at a time. Between two
// keyframes both move in a straight line
struct Keyframe {
	float time;
	vec3 position;
	vec3 angles;
};

static const Keyframe Keyframes[] = {
	{ 0.0f, { 230, -110, -340 }, { -33.4f, 15.8f, -5.3f } },
	{ 2.0f, { 230, -110, -340 }, { -0.9f, 15.8f, -5.3f } },
	{ 4.0f, { 650, -120, -600 }, { 11.4f, 47.5f, -5.3f } },
	{ 6.0f, { 280, -60, -670 }, { 0, 25.5f, 0 } },
	{ 8.0f, { 370, -340, -760 }, { 26.4f, 29.9f, 0 } },
	{ 10.0f, { 120, 10, -330 }, { 27.2f, 25.5f, 0 } },
	{ 12.0f, { -280, 250, -330 }, { -22, -41.3f, 0 } },
	{ 14.0f, { -700, -10, -550 }, { 0, -50.1f, -5.3f } },
	{ 16.0f, { -570, -20, 120 }, { 0, -50.1f, -5.3f } },
};

constexpr int KEYFRAMES = (int)(sizeof(Keyframes) / sizeof(Keyframes[0]));

void DEMO_Render(RETRO_Time time)
{
	// The camera between the two keyframes, bobbing
	RETRO_KeyframeLoop span = RETRO_LoopKeyframes(time.total, Keyframes, KEYFRAMES, sizeof(Keyframe));
	float now = (float)span.seconds;
	const Keyframe &from = Keyframes[span.key];
	const Keyframe &to = Keyframes[span.key + 1];
	float bob = (float)sin(2 * M_PI * now / BOB_PERIOD);
	vec3 eye = mix(from.position, to.position, span.t) + vec3{ 0, BOB_HEIGHT * bob, 0 };
	vec3 angles = mix(from.angles, to.angles, span.t);
	mat3 camera = rotate((float)radians(angles.x), (float)radians(angles.y), (float)radians(angles.z));
	float pitch = LIGHT_PITCH - BOB_ANGLE * bob;
	float yaw = angles.y + LIGHT_YAW + BOB_ANGLE / 2 * bob;
	mat3 lighting = rotate((float)radians(pitch), (float)radians(yaw), 0);

	// The bee between two poses, as the camera, standing at eye and turned by
	// camera, sees it, its normals turned by lighting instead
	float poses = now / POSE_TIME;
	int pose = (int)poses;
	RETRO_BlendModelPoses(pose % POSES, (pose + 1) % POSES, poses - pose);
	vec3 seen = camera * -eye;
	RETRO_RotateModel(camera);
	RETRO_TranslateModel(seen.x, seen.y, seen.z);
	RETRO_Get3DModel()->matrix = lighting;
	RETRO_RotateVertexNormals();

	RETRO_Blit(RETRO_ImageData());

	// A face reaching in front of the screen is dropped whole
	RETRO_ProjectModel(FOCAL / EYE, RETRO_WIDTH / 2.0f, RETRO_HEIGHT / 2.0f, NULL, FOCAL, true);
	RETRO_RenderModel(RETRO_POLY_MATCAP);
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/bee_background_320x240.pcx", true);

	Model3D *model = RETRO_LoadMD3Model("assets/bee.md3");
	model->envmap = RETRO_LoadImage("assets/bee_envmap_256x256.pcx")->data;
	model->envmapradius = ENVMAP_RADIUS;
}
