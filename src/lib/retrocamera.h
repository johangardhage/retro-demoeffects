//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROCAMERA_H_
#define _RETROCAMERA_H_

#include <float.h> // FLT_MIN
#include "retro.h"
#include "retromouse.h"
#include "retropoly.h"
#include "retrovector.h"

// A pinhole's worth of focal length, decoupled from any near-plane offset.
// RETRO_PROJECTION_EYEDISTANCE (retromath.h) is half of a coupled
// scale * eyedistance pair, for a model that sits some distance in front of
// the eye, and no choice of the two makes depth rpos.z itself with focal
// scaling only x and y, as a camera-relative point needs.
#define RETRO_CAMERA_FOCAL 250

//
// What the camera looks through: a pinhole with a focal length across and one
// down, the point on the screen the view direction lands on, the nearest depth
// worth drawing, and the part of the screen the view fills. The defaults are
// a RETRO_CAMERA_FOCAL pinhole centered on the whole screen.
//
struct RETRO_CameraLens {
	float focalx = RETRO_CAMERA_FOCAL;
	float focaly = RETRO_CAMERA_FOCAL;
	vec2 center = { RETRO_WIDTH / 2.0f, RETRO_HEIGHT / 2.0f };
	float nearplane = 1.0f;
	ClipRect view;
};

//
// A camera: an eye position and an orthonormal right/down/forward frame.
// Right, down and forward are screen +x, screen +y and the view direction,
// matching the y-down, z-away convention RETRO_ProjectVertex and
// RETRO_SortFaces already assume (retromath.h). A world keeps its own axes:
// a y-up world has the camera's down at -up.
//
// The frame is oriented one of two ways:
//
//   - Turned turn by turn, by RETRO_YawCamera, RETRO_PitchCamera and
//     RETRO_RollCamera, each about one of the frame's own current axes, so
//     the frame stays orthonormal after any sequence of turns and rolls -
//     the free flight of a Descent or Wing Commander style ship. Euler
//     angles taken against the world's fixed axes (rotate(ax, ay, az)) are
//     the wrong tool there: after a few turns, two of the frame's axes are
//     arbitrary directions, not the axes the angles are measured from.
//
//   - Aimed afresh from a heading and pitch kept as angles, by
//     RETRO_AimLook - a first-person view, where the heading turns about
//     the world's vertical and the horizon never tilts.
//
struct RETRO_Camera {
	vec3 pos = { 0, 0, 0 };			// Eye position, world space
	vec3 right = { 1, 0, 0 };			// Frame: screen +x
	vec3 down = { 0, 1, 0 };			// Frame: screen +y
	vec3 forward = { 0, 0, 1 };		// Frame: view direction
	RETRO_CameraLens lens;
};

// Eye at pos with the given orthonormal frame: the write of the camera's own
// frame, not a turn of an existing one
inline void RETRO_PlaceCamera(RETRO_Camera *camera, vec3 pos, vec3 right, vec3 down, vec3 forward)
{
	camera->pos = pos;
	camera->right = right;
	camera->down = down;
	camera->forward = forward;
}

// Move the eye along its own current axes: forward along the view direction,
// right and down for a strafe. Orientation is untouched.
inline void RETRO_MoveCamera(RETRO_Camera *camera, float forward, float right = 0, float down = 0)
{
	camera->pos += camera->forward * forward + camera->right * right + camera->down * down;
}

// Turn left/right: rotate right and forward about the frame's own down axis.
// Positive yaw turns right: forward moves toward right, and the world
// slides leftward on screen.
inline void RETRO_YawCamera(RETRO_Camera *camera, float angle)
{
	float c = cos(angle), s = sin(angle);

	vec3 right = camera->right * c - camera->forward * s;
	vec3 forward = camera->forward * c + camera->right * s;

	camera->right = right;
	camera->forward = forward;
}

// Pitch up/down: rotate down and forward about the frame's own right axis.
// Positive pitch tips the nose down: forward moves toward down, and the
// world slides upward on screen.
inline void RETRO_PitchCamera(RETRO_Camera *camera, float angle)
{
	float c = cos(angle), s = sin(angle);

	vec3 down = camera->down * c - camera->forward * s;
	vec3 forward = camera->forward * c + camera->down * s;

	camera->down = down;
	camera->forward = forward;
}

// Positive roll banks right: right moves toward down, and the scene
// rotates counterclockwise on screen.
inline void RETRO_RollCamera(RETRO_Camera *camera, float angle)
{
	float c = cos(angle), s = sin(angle);

	vec3 right = camera->right * c + camera->down * s;
	vec3 down = camera->down * c - camera->right * s;

	camera->right = right;
	camera->down = down;
}

//
// Aim the camera from a level frame: set the frame to right, down and
// forward, then turn it by yaw and pitch as RETRO_YawCamera and
// RETRO_PitchCamera do (positive yaw turns right, positive pitch tips the
// nose down). The yaw turns about the level frame's down axis, so about the
// world's vertical, and the pitch about the turned right axis, which stays
// level. Built afresh each time from a heading and pitch kept as angles, the
// frame never picks up a roll - the look of a first-person view, where
// turning the frame turn by turn would tilt the horizon after a look up.
// RETRO_AimLook calls this with both angles negated: its yaw turns left and
// its pitch looks up.
//
inline void RETRO_AimCamera(RETRO_Camera *camera, vec3 right, vec3 down, vec3 forward, float yaw, float pitch)
{
	camera->right = right;
	camera->down = down;
	camera->forward = forward;
	RETRO_YawCamera(camera, yaw);
	RETRO_PitchCamera(camera, pitch);
}

//
// A first-person look: a heading and a pitch kept as angles, moved by the
// mouse, and the camera aimed from them
//
// The angles turn the way a player thinks of them, the other way round from
// the camera's own turns: a positive yaw turns left and a positive pitch looks
// up. The mouse moves them as it moves a cursor, right turning right and down
// looking down. The yaw is kept within a turn either way, and the pitch held
// short of straight up or down, where the heading would stop meaning anything.
//
struct RETRO_Look {
	float yaw = 0;				// Radians, turning left
	float pitch = 0;			// Radians, looking up
	float sensitivity = 0.003f;	// Radians a unit of mouse motion turns
	float maxpitch = 1.5f;		// Radians the pitch is held within, up or down
};

// Turn the look by these angles, the look's way round
inline void RETRO_TurnLook(RETRO_Look *look, float yaw, float pitch)
{
	look->yaw = fmodf(look->yaw + yaw, 2 * M_PI);
	look->pitch = clamp(look->pitch + pitch, -look->maxpitch, look->maxpitch);
}

// Turn the look by the mouse's motion in a state already read. Reading the
// state takes the motion since the last read, so a caller that also wants
// the buttons reads it once and hands it here.
inline void RETRO_MouseLook(RETRO_Look *look, const RETRO_MouseState &mouse)
{
	RETRO_TurnLook(look, -mouse.xrel * look->sensitivity, -mouse.yrel * look->sensitivity);
}

// The same, reading the mouse
inline void RETRO_MouseLook(RETRO_Look *look)
{
	RETRO_MouseLook(look, RETRO_GetMouseState());
}

// Aim the camera from the look: the world's level frame - its right, down and
// forward at a yaw of zero - turned by the yaw and tipped by the pitch. The
// frame never picks up a roll, so the horizon stays level.
inline void RETRO_AimLook(RETRO_Camera *camera, const RETRO_Look &look, vec3 right, vec3 down, vec3 forward)
{
	RETRO_AimCamera(camera, right, down, forward, -look.yaw, -look.pitch);
}

// Whether a camera only yaws: its down is straight along the world's y. A
// ground offset's side and depth in its frame then do not depend on the
// offset's height, so a ground cell can be judged before its height is read.
inline bool RETRO_CameraOnlyYaws(const RETRO_Camera &camera)
{
	return camera.down.x == 0 && camera.down.z == 0;
}

// Two axes perpendicular to forward, in the camera's own right/down
// convention: right is reference cross forward, down is forward cross
// right. reference is the way this frame's down leans: down is reference
// with its part along forward taken out, normalized. {0, 1, 0} is the
// camera's own down, which a tunnel passes for both the camera and the ring;
// a y-up world passes {0, -1, 0}. It only chooses the orientation, so the
// frame does not inherit a roll from it. forward must not be parallel to it.
inline void RETRO_FrameFromForward(vec3 forward, vec3 reference, vec3 *right, vec3 *down)
{
	*right = normalize(cross(reference, forward));
	*down = normalize(cross(forward, *right));
}

// Eye at pos looking along forward, a unit direction, in the frame
// RETRO_FrameFromForward builds from reference: the way a camera following
// a path looks along the path, or a chase camera looks at what it follows
inline void RETRO_LookAlong(RETRO_Camera *camera, vec3 pos, vec3 forward, vec3 reference = { 0, 1, 0 })
{
	vec3 right, down;
	RETRO_FrameFromForward(forward, reference, &right, &down);
	RETRO_PlaceCamera(camera, pos, right, down, forward);
}

// *******************************************************************
// Into the camera's frame
// *******************************************************************

// A world direction in the camera's frame: right, down and forward of the eye.
// The rotation alone, so it does not move with the eye.
inline vec3 RETRO_ViewDirection(const RETRO_Camera *camera, vec3 direction)
{
	return { dot(direction, camera->right), dot(direction, camera->down), dot(direction, camera->forward) };
}

// Shifted to the eye first, so the point moves with the eye.
// RETRO_ViewDirection is the rotation alone.
inline vec3 RETRO_ViewPoint(const RETRO_Camera *camera, vec3 point)
{
	return RETRO_ViewDirection(camera, point - camera->pos);
}

// *******************************************************************
// Through the lens
// *******************************************************************

// The most corners a polygon handed to RETRO_CameraClipProjectPolygon may have.
// Each of the five cuts adds at most one more.
#define RETRO_CAMERA_MAX_POLYGON 32
#define RETRO_CAMERA_CLIP_PLANES 5

// The lens's pinhole, for a point in the camera's frame in front of the eye.
// The caller decides what is worth projecting first: a direction projects to
// where it points, at any distance.
inline PolygonPoint RETRO_ProjectViewPoint(const RETRO_CameraLens &lens, vec3 eye)
{
	PolygonPoint point = {};
	point.q = 1.0f / eye.z;
	point.pos = { lens.center.x + lens.focalx * eye.x * point.q, lens.center.y + lens.focaly * eye.y * point.q };
	return point;
}

// Where a world direction lands on the screen: a body so far off, as the sun,
// that only its direction counts. False when it is behind the eye.
inline bool RETRO_ProjectViewDirection(const RETRO_Camera *camera, vec3 direction, PolygonPoint *point)
{
	vec3 eye = RETRO_ViewDirection(camera, direction);
	if (eye.z <= 0.0f) return false;
	*point = RETRO_ProjectViewPoint(camera->lens, eye);
	return true;
}

// Half-open, as lens.view is: x1 and y1 are outside.
inline bool RETRO_LensViewContains(const RETRO_CameraLens &lens, int x, int y)
{
	return x >= lens.view.x0 && x < lens.view.x1 && y >= lens.view.y0 && y < lens.view.y1;
}

//
// The world direction through a point on the screen, not normalized: the
// pinhole run backward, forward plus right and down by how far the point is
// from the lens's center over its focal lengths. Through a pixel's center
// for a pixel (x, y) is p = (x + 0.5, y + 0.5).
//
inline vec3 RETRO_ViewRay(const RETRO_Camera *camera, vec2 p)
{
	const RETRO_CameraLens &lens = camera->lens;
	return camera->forward + camera->right * ((p.x - lens.center.x) / lens.focalx) + camera->down * ((p.y - lens.center.y) / lens.focaly);
}

//
// A polygon corner in the camera's frame, before the pinhole, with what a
// drawer interpolates
//
struct RETRO_CameraVertex {
	vec3 eye;						// Right, down and forward of the eye
	vec2 uv;						// Texture coordinates
	float c;						// Shade, or palette index
	float tint[RETRO_MAX_TINTS];	// Further light levels, for a shade table with tints
	vec2 lightuv;					// Light map coordinates
};

// A world point as such a corner, with nothing yet to interpolate
inline RETRO_CameraVertex RETRO_ViewVertex(const RETRO_Camera *camera, vec3 point)
{
	RETRO_CameraVertex vertex = {};
	vertex.eye = RETRO_ViewPoint(camera, point);
	return vertex;
}

// The corner t of the way along an edge. Every field is linear along an edge
// in the camera's frame, so a cut corner is the same mix of the edge's ends
// in all of them.
inline RETRO_CameraVertex RETRO_CameraMixVertex(const RETRO_CameraVertex &a, const RETRO_CameraVertex &b, float t)
{
	RETRO_CameraVertex v;
	v.eye = mix(a.eye, b.eye, t);
	v.uv = mix(a.uv, b.uv, t);
	v.lightuv = mix(a.lightuv, b.lightuv, t);
	v.c = mix(a.c, b.c, t);
	for (int j = 0; j < RETRO_MAX_TINTS; j++) {
		v.tint[j] = mix(a.tint[j], b.tint[j], t);
	}
	return v;
}

struct RETRO_CameraPlane {
	vec3 normal;
	float offset = 0;
};

inline float RETRO_CameraPlaneDistance(const RETRO_CameraPlane &plane, vec3 eye)
{
	return dot(plane.normal, eye) - plane.offset;
}

// The part of a polygon on the inward side of plane. A side of the view
// passes through the eye, so its offset is zero. The near plane lies along
// forward at its depth.
inline int RETRO_CameraClipToPlane(const RETRO_CameraVertex *vertex, int count, RETRO_CameraVertex *clipped, const RETRO_CameraPlane &plane)
{
	int points = 0;
	for (int i = 0; i < count; i++) {
		const RETRO_CameraVertex &a = vertex[i];
		const RETRO_CameraVertex &b = vertex[(i + 1) % count];
		float da = RETRO_CameraPlaneDistance(plane, a.eye);
		float db = RETRO_CameraPlaneDistance(plane, b.eye);
		if (da >= 0) clipped[points++] = a;
		if ((da >= 0) != (db >= 0)) clipped[points++] = RETRO_CameraMixVertex(a, b, da / (da - db));
	}
	return points;
}

// How much wider than the lens's view a cull keeps, so a shape on the screen
// edge is kept rather than thrown out
#define RETRO_CAMERA_CULL_WIDEN 1.05f

//
// How far the lens's view reaches from its center, per unit of depth
//
// A point in the camera's frame with eye.x = -eye.z * left sits on the left
// edge of lens.view, and likewise for the other three. A lens off center, or
// a view short of the screen, reaches further on one side than the other.
// Each is scaled by widen. Not slopes of the ground.
//
struct RETRO_LensSlopes {
	float left, right, top, bottom;
};

inline RETRO_LensSlopes RETRO_LensViewSlopes(const RETRO_CameraLens &lens, float widen = 1.0f)
{
	RETRO_LensSlopes slope;
	slope.left = (lens.center.x - lens.view.x0) / lens.focalx * widen;
	slope.right = (lens.view.x1 - lens.center.x) / lens.focalx * widen;
	slope.top = (lens.center.y - lens.view.y0) / lens.focaly * widen;
	slope.bottom = (lens.view.y1 - lens.center.y) / lens.focaly * widen;
	return slope;
}

// The same five inward-facing planes serve culling and clipping. Side
// planes pass through the eye; the last plane is the near depth.
struct RETRO_CameraFrustum {
	RETRO_CameraPlane plane[RETRO_CAMERA_CLIP_PLANES];
};

inline RETRO_CameraFrustum RETRO_LensFrustum(const RETRO_CameraLens &lens, float widen = 1.0f)
{
	RETRO_LensSlopes slope = RETRO_LensViewSlopes(lens, widen);
	return { { { { 1, 0, slope.left }, 0 }, { { -1, 0, slope.right }, 0 },
		{ { 0, 1, slope.top }, 0 }, { { 0, -1, slope.bottom }, 0 },
		{ { 0, 0, 1 }, lens.nearplane } } };
}

// A shared outside bit means all corners miss the same plane. A zero mask
// does not prove intersection, but conservatively keeps crossing polygons.
inline unsigned RETRO_CameraOutcode(const RETRO_CameraFrustum &frustum, vec3 eye)
{
	unsigned code = 0;
	for (int i = 0; i < RETRO_CAMERA_CLIP_PLANES; i++) {
		if (RETRO_CameraPlaneDistance(frustum.plane[i], eye) < 0) code |= 1u << i;
	}
	return code;
}

// Whether corners in the camera's frame all lie outside one plane of frustum.
// A walk that tests many shapes builds the frustum once and asks this.
inline bool RETRO_FrustumCornersOutside(const RETRO_CameraFrustum &frustum, const RETRO_CameraVertex *vertex, int count)
{
	unsigned outside = (1u << RETRO_CAMERA_CLIP_PLANES) - 1;
	for (int i = 0; i < count && outside; i++) {
		outside &= RETRO_CameraOutcode(frustum, vertex[i].eye);
	}
	return outside != 0;
}

// The same against the lens's view, widened by widen
inline bool RETRO_ViewCornersOutside(const RETRO_CameraLens &lens, const RETRO_CameraVertex *vertex, int count, float widen = RETRO_CAMERA_CULL_WIDEN)
{
	return RETRO_FrustumCornersOutside(RETRO_LensFrustum(lens, widen), vertex, count);
}

//
// Clip a polygon in the camera's frame to the view, then project it
//
// The four sides of the view are planes through the eye, each a pixel outside
// the edge of lens.view so rounding at the edge leaves no gap. They meet at
// the eye, so between them they also cut away everything behind it, and a
// polygon reaching far past the edge of the screen is not projected far off
// it. The near plane is cut last, and a corner cut there is set on it
// exactly, so rounding cannot leave it a hair behind.
//
// count may be up to RETRO_CAMERA_MAX_POLYGON, and point must have room for
// RETRO_CAMERA_CLIP_PLANES more, as RETRO_ProjectedPolygon has. Returns the
// corners written; fewer than three is nothing to draw.
//
inline int RETRO_CameraClipProjectPolygon(const RETRO_CameraLens &lens, const RETRO_CameraVertex *vertex, int count, PolygonPoint *point)
{
	RETRO_CameraLens grown = lens;
	grown.view = { lens.view.x0 - 1, lens.view.x1 + 1, lens.view.y0 - 1, lens.view.y1 + 1 };
	if (count < 3) return 0;
	if (count > RETRO_CAMERA_MAX_POLYGON) {
		RETRO_RageQuit("A polygon of %d corners is more than the lens can cut\n", count);
	}
	RETRO_CameraFrustum frustum = RETRO_LensFrustum(grown);
	RETRO_CameraVertex clipped[2][RETRO_CAMERA_MAX_POLYGON + RETRO_CAMERA_CLIP_PLANES];
	const RETRO_CameraVertex *from = vertex;
	int buffer = 0;
	for (const RETRO_CameraPlane &plane : frustum.plane) {
		bool inside = true;
		for (int i = 0; i < count && inside; i++) {
			inside = RETRO_CameraPlaneDistance(plane, from[i].eye) >= 0;
		}
		if (inside) continue;
		count = RETRO_CameraClipToPlane(from, count, clipped[buffer], plane);
		if (count < 3) return 0;
		from = clipped[buffer];
		buffer ^= 1;
	}

	for (int i = 0; i < count; i++) {
		vec3 eye = from[i].eye;
		eye.z = MAX(eye.z, lens.nearplane);
		point[i] = RETRO_ProjectViewPoint(lens, eye);
		point[i].c = from[i].c;
		point[i].uv = from[i].uv;
		point[i].lightuv = from[i].lightuv;
		for (int j = 0; j < RETRO_MAX_TINTS; j++) {
			point[i].tint[j] = from[i].tint[j];
		}
	}
	return count;
}

//
// A polygon projected through the lens
//
// The buffer holds RETRO_CAMERA_MAX_POLYGON corners and one more for each of
// the five cuts, which is every corner RETRO_CameraClipProjectPolygon can
// write. A shorter buffer overflows once a polygon crosses a side of the view
// as well as the near plane, the ordinary case for ground under a low eye.
//
struct RETRO_ProjectedPolygon {
	int count = 0;
	PolygonPoint point[RETRO_CAMERA_MAX_POLYGON + RETRO_CAMERA_CLIP_PLANES];
};

// Clip a polygon in the camera's frame to the view and project it into
// projected. Fewer than three corners is nothing to draw. More than
// RETRO_CAMERA_MAX_POLYGON is refused rather than written past the buffer.
inline void RETRO_CameraClipProject(const RETRO_CameraLens &lens, const RETRO_CameraVertex *vertex, int count, RETRO_ProjectedPolygon *projected)
{
	projected->count = RETRO_CameraClipProjectPolygon(lens, vertex, count, projected->point);
}

//
// A triangle projected through the lens
//
// The buffer holds the three corners and one more for each of the five cuts,
// which is every corner RETRO_CameraClipProjectPolygon can write for a
// triangle.
//
struct RETRO_ProjectedTriangle {
	int count = 0;
	PolygonPoint point[3 + RETRO_CAMERA_CLIP_PLANES];
};

// Clip a triangle in the camera's frame to the view and project it into
// triangle. Fewer than three corners is nothing to draw.
inline void RETRO_CameraClipProjectTriangle(const RETRO_CameraLens &lens, const RETRO_CameraVertex &a, const RETRO_CameraVertex &b, const RETRO_CameraVertex &c, RETRO_ProjectedTriangle *triangle)
{
	RETRO_CameraVertex corner[3] = { a, b, c };
	triangle->count = RETRO_CameraClipProjectPolygon(lens, corner, 3, triangle->point);
}

// *******************************************************************
// A flat world through the lens
// *******************************************************************

//
// The part of a lens view below the horizon, as a polygon
//
// The view's corners are cut along the horizon as a polygon's edges are cut
// at a plane. How much the ray through a point climbs along the world's unit
// up is linear in the point, so the horizon, where it is zero, is a straight
// line. below holds five points, one for each corner and one for a cut.
// Each point's q is the faintest depth a cleared buffer still takes, so
// ground drawn from them is covered by anything drawn after it. Fewer than
// three points is a view with no ground.
//
inline int RETRO_ClipHorizon(const RETRO_Camera *camera, vec3 up, PolygonPoint *below)
{
	const ClipRect &view = camera->lens.view;
	vec2 corner[4] = { { (float)view.x0, (float)view.y0 }, { (float)view.x1, (float)view.y0 }, { (float)view.x1, (float)view.y1 }, { (float)view.x0, (float)view.y1 } };
	int points = 0;
	for (int i = 0; i < 4; i++) {
		vec2 a = corner[i], b = corner[(i + 1) % 4];
		float ca = dot(RETRO_ViewRay(camera, a), up), cb = dot(RETRO_ViewRay(camera, b), up);
		if (ca < 0) {
			below[points] = {};
			below[points++].pos = a;
		}
		if ((ca < 0) != (cb < 0)) {
			below[points] = {};
			below[points++].pos = mix(a, b, ca / (ca - cb));
		}
	}
	for (int i = 0; i < points; i++) {
		below[i].q = FLT_MIN;
	}
	return points;
}

// The part of a line in the camera's frame at or past nearplane, with a cut
// end set on the plane exactly. False when none of it is.
inline bool RETRO_CutViewLine(vec3 *a, vec3 *b, float nearplane)
{
	if (a->z < nearplane && b->z < nearplane) return false;
	if (a->z < nearplane || b->z < nearplane) {
		vec3 cut = mix(*a, *b, (nearplane - a->z) / (b->z - a->z));
		cut.z = nearplane;
		if (a->z < nearplane) *a = cut;
		else *b = cut;
	}
	return true;
}

#endif
