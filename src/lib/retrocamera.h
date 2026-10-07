//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROCAMERA_H_
#define _RETROCAMERA_H_

#include <float.h> // FLT_MIN
#include "retro.h"
#include "retrogfx.h"
#include "retromodel.h"
#include "retromath.h"
#include "retromatrix.h"
#include "retropoly.h"

// A pinhole's worth of focal length, decoupled from any near-plane offset.
// RETRO_PROJECTION_EYEDISTANCE (retromath.h) is not reused here: it is half
// of a coupled scale*eyedistance pair meant for a model that sits some
// distance in front of the camera, and there is no scale/eyedistance choice
// that gives a camera-relative point a plain q = focal / depth.
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
//     RETRO_AimCamera - a first-person view, where the heading turns about
//     the world's vertical and the horizon never tilts.
//
struct RETRO_Camera {
	vec3 pos;			// Eye position, world space
	vec3 right;			// Frame: screen +x
	vec3 down;			// Frame: screen +y
	vec3 forward;		// Frame: view direction
	RETRO_CameraLens lens;
};

inline void RETRO_InitializeCamera(RETRO_Camera *camera, vec3 pos = { 0, 0, 0 })
{
	camera->pos = pos;
	camera->right = { 1, 0, 0 };
	camera->down = { 0, 1, 0 };
	camera->forward = { 0, 0, 1 };
}

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
//
inline void RETRO_AimCamera(RETRO_Camera *camera, vec3 right, vec3 down, vec3 forward, float yaw, float pitch)
{
	camera->right = right;
	camera->down = down;
	camera->forward = forward;
	RETRO_YawCamera(camera, yaw);
	RETRO_PitchCamera(camera, pitch);
}

// Two axes perpendicular to forward, in the camera's own right/down
// convention: right is world-down cross forward, down is forward cross
// right. Building a ring's cross-section frame from its own tangent, or a
// camera's frame from its look direction, is the same operation, so a
// caller that already has forward - the camera does, for its own forward -
// is not made to recompute it for a second frame at the same station.
// world-down is only ever the reference used to build right - it does not
// appear in the result, so the frame does not inherit a roll from it, only
// an orientation.
inline void RETRO_FrameFromForward(vec3 forward, vec3 *right, vec3 *down)
{
	vec3 worlddown = { 0, 1, 0 };
	*right = normalize(cross(worlddown, forward));
	*down = normalize(cross(forward, *right));
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

// A world point in the camera's frame
inline vec3 RETRO_ViewPoint(const RETRO_Camera *camera, vec3 point)
{
	return RETRO_ViewDirection(camera, point - camera->pos);
}

// *******************************************************************
// Through the lens
// *******************************************************************

// The most corners a polygon handed to RETRO_ClipProjectViewPolygon may have.
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

// The corner t of the way along an edge. Every field is linear along an edge
// in the camera's frame, so a cut corner is the same mix of the edge's ends
// in all of them.
inline RETRO_CameraVertex RETRO_MixCameraVertex(const RETRO_CameraVertex &a, const RETRO_CameraVertex &b, float t)
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

// The part of a polygon on the side of a plane through the eye where
// dot(plane, eye) is not negative
inline int RETRO_ClipViewPolygonToPlane(const RETRO_CameraVertex *vertex, int count, RETRO_CameraVertex *clipped, vec3 plane)
{
	int points = 0;
	for (int i = 0; i < count; i++) {
		const RETRO_CameraVertex &a = vertex[i];
		const RETRO_CameraVertex &b = vertex[(i + 1) % count];
		float da = dot(plane, a.eye);
		float db = dot(plane, b.eye);
		if (da >= 0) clipped[points++] = a;
		if ((da >= 0) != (db >= 0)) clipped[points++] = RETRO_MixCameraVertex(a, b, da / (da - db));
	}
	return points;
}

// The part of a polygon at or past the near plane. A cut corner is set on the
// plane exactly, so rounding cannot leave it a hair behind.
inline int RETRO_ClipViewPolygonToNearPlane(const RETRO_CameraVertex *vertex, int count, RETRO_CameraVertex *clipped, float nearplane)
{
	int points = 0;
	for (int i = 0; i < count; i++) {
		const RETRO_CameraVertex &a = vertex[i];
		const RETRO_CameraVertex &b = vertex[(i + 1) % count];
		bool ainside = a.eye.z >= nearplane;
		bool binside = b.eye.z >= nearplane;
		if (ainside) clipped[points++] = a;
		if (ainside != binside) {
			RETRO_CameraVertex &v = clipped[points++];
			v = RETRO_MixCameraVertex(a, b, (nearplane - a.eye.z) / (b.eye.z - a.eye.z));
			v.eye.z = nearplane;
		}
	}
	return points;
}

//
// Clip a polygon in the camera's frame to the view, then project it
//
// The four sides of the view are planes through the eye, each a pixel outside
// the edge of lens.view so rounding at the edge leaves no gap. They meet at
// the eye, so between them they also cut away everything behind it, and a
// polygon reaching far past the edge of the screen is not projected far off
// it. The near plane is cut last.
//
// count may be up to RETRO_CAMERA_MAX_POLYGON, and point must have room for
// RETRO_CAMERA_CLIP_PLANES more. Returns the corners written; fewer than three
// is nothing to draw.
//
inline int RETRO_ClipProjectViewPolygon(const RETRO_CameraLens &lens, const RETRO_CameraVertex *vertex, int count, PolygonPoint *point)
{
	float left = (lens.view.x0 - 1 - lens.center.x) / lens.focalx;
	float right = (lens.view.x1 + 1 - lens.center.x) / lens.focalx;
	float top = (lens.view.y0 - 1 - lens.center.y) / lens.focaly;
	float bottom = (lens.view.y1 + 1 - lens.center.y) / lens.focaly;
	vec3 sides[4] = { { 1, 0, -left }, { -1, 0, right }, { 0, 1, -top }, { 0, -1, bottom } };

	// Most polygons are inside all four sides, and a cut that keeps every
	// corner would only copy them
	bool inside = true;
	for (int i = 0; i < count && inside; i++) {
		for (const vec3 &side : sides) {
			if (dot(side, vertex[i].eye) < 0) {
				inside = false;
				break;
			}
		}
	}

	RETRO_CameraVertex clipped[2][RETRO_CAMERA_MAX_POLYGON + RETRO_CAMERA_CLIP_PLANES];
	const RETRO_CameraVertex *from = vertex;
	for (int i = 0; i < 4 && !inside; i++) {
		count = RETRO_ClipViewPolygonToPlane(from, count, clipped[i % 2], sides[i]);
		if (count < 3) return 0;
		from = clipped[i % 2];
	}
	count = RETRO_ClipViewPolygonToNearPlane(from, count, clipped[0], lens.nearplane);
	if (count < 3) return 0;

	for (int i = 0; i < count; i++) {
		point[i] = RETRO_ProjectViewPoint(lens, clipped[0][i].eye);
		point[i].c = clipped[0][i].c;
		point[i].uv = clipped[0][i].uv;
		point[i].lightuv = clipped[0][i].lightuv;
		for (int j = 0; j < RETRO_MAX_TINTS; j++) {
			point[i].tint[j] = clipped[0][i].tint[j];
		}
	}
	return count;
}

// *******************************************************************
// A flat world through the lens
// *******************************************************************

//
// How much the direction through a point on the screen climbs, along a
// world's unit up
//
// The direction is RETRO_ViewRay's. It is linear in the point, so the
// horizon, where the climb is zero, is a straight line.
//
inline float RETRO_ViewClimb(const RETRO_Camera *camera, vec2 p, vec3 up)
{
	return dot(RETRO_ViewRay(camera, p), up);
}

//
// The sky and the ground of a flat world, split along the horizon
//
// The lens's view is filled with sky, then the part of it below the horizon
// with ground: the view's corners cut along the horizon as a polygon's edges
// are cut at a plane. The ground goes through the depth buffer at the
// faintest depth a cleared buffer still takes, so anything drawn after it
// covers it.
//
inline void RETRO_DrawHorizon(const RETRO_Camera *camera, vec3 up, unsigned char sky, unsigned char ground)
{
	const ClipRect &view = camera->lens.view;
	RETRO_DrawRectangle(view.x0, view.y0, view.x1 - 1, view.y1 - 1, sky);

	vec2 corner[4] = { { (float)view.x0, (float)view.y0 }, { (float)view.x1, (float)view.y0 }, { (float)view.x1, (float)view.y1 }, { (float)view.x0, (float)view.y1 } };
	PolygonPoint below[5];
	int points = 0;
	for (int i = 0; i < 4; i++) {
		vec2 a = corner[i], b = corner[(i + 1) % 4];
		float ca = RETRO_ViewClimb(camera, a, up), cb = RETRO_ViewClimb(camera, b, up);
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
	if (points >= 3) RETRO_DrawFlatPolygon(below, points, ground, view);
}

//
// A world line, a pixel at a time through the depth buffer
//
// It is cut at nearplane, which may be further out than the lens's own, so
// that neither end lands far off the screen and the steps stay few. q is
// linear across the screen, so it is interpolated along the steps as it is
// across a face. Only the lens's view is drawn in.
//
inline void RETRO_DrawViewLine(const RETRO_Camera *camera, vec3 from, vec3 to, unsigned char color, float nearplane)
{
	vec3 a = RETRO_ViewPoint(camera, from);
	vec3 b = RETRO_ViewPoint(camera, to);
	if (a.z < nearplane && b.z < nearplane) return;
	if (a.z < nearplane || b.z < nearplane) {
		vec3 cut = mix(a, b, (nearplane - a.z) / (b.z - a.z));
		cut.z = nearplane;
		if (a.z < nearplane) a = cut;
		else b = cut;
	}

	const ClipRect &view = camera->lens.view;
	PolygonPoint pa = RETRO_ProjectViewPoint(camera->lens, a);
	PolygonPoint pb = RETRO_ProjectViewPoint(camera->lens, b);
	vec2 delta = pb.pos - pa.pos;
	int steps = MAX((int)ceilf(MAX(fabsf(delta.x), fabsf(delta.y))), 1);
	for (int i = 0; i <= steps; i++) {
		float t = (float)i / steps;
		int x = (int)floorf(pa.pos.x + delta.x * t);
		int y = (int)floorf(pa.pos.y + delta.y * t);
		if (x < view.x0 || x >= view.x1 || y < view.y0 || y >= view.y1) continue;
		int offset = y * RETRO_WIDTH + x;
		if (RETRO_DepthTest(offset, mix(pa.q, pb.q, t))) {
			RETRO.framebuffer[offset] = color;
		}
	}
}

#endif
