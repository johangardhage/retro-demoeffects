//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROCAMERA_H_
#define _RETROCAMERA_H_

#include "retro.h"
#include "retromodel.h"
#include "retromath.h"
#include "retromatrix.h"

// A pinhole's worth of focal length, decoupled from any near-plane offset.
// RETRO_PROJECTION_EYEDISTANCE (retromath.h) is not reused here: it is half
// of a coupled scale*eyedistance pair meant for a model that sits some
// distance in front of the camera, and there is no scale/eyedistance choice
// that gives a camera-relative point a plain q = focal / depth.
#define RETRO_CAMERA_FOCAL 250

//
// A free-flight camera: an eye position and an orthonormal right/down/forward
// frame, turned about its own current axes rather than about fixed world
// ones. Right, down and forward are screen +x, screen +y and the view
// direction, matching the y-down, z-away convention RETRO_ProjectVertex and
// RETRO_SortFaces already assume (retromath.h).
//
// rotate(ax, ay, az) builds a rotation from Euler angles taken
// against the world's fixed axes, which is the wrong tool here: yawing left,
// then pitching up, then yawing again would have to recover ax, ay, az from
// the frame this camera is already at, and two of those three axes are by
// then arbitrary directions in world space, not the coordinate axes Euler
// angles are measured from. RETRO_YawCamera, RETRO_PitchCamera and
// RETRO_RollCamera below turn two of the frame's own vectors about the
// third instead, so the frame stays orthonormal and nothing is ever
// recovered from it - the turn a Descent or Wing Commander style ship needs
// after an arbitrary sequence of turns and rolls.
//
struct RETRO_Camera {
	vec3 pos;			// Eye position, world space
	vec3 right;			// Frame: screen +x
	vec3 down;			// Frame: screen +y
	vec3 forward;		// Frame: view direction
};

inline void RETRO_InitializeCamera(RETRO_Camera *camera, vec3 pos = { 0, 0, 0 })
{
	camera->pos = pos;
	camera->right = { 1, 0, 0 };
	camera->down = { 0, 1, 0 };
	camera->forward = { 0, 0, 1 };
}

// Eye at pos with the given orthonormal frame. The three directions are
// taken as authored (dir), the same slot RETRO_UnitCrossProduct and
// RETRO_NormalizeUnitVector write. They are not rotated: this is the write
// of the camera's own frame, not a turn of an existing one.
inline void RETRO_PlaceCamera(RETRO_Camera *camera, vec3 pos, UnitVector right, UnitVector down, UnitVector forward)
{
	camera->pos = pos;
	camera->right = right.dir;
	camera->down = down.dir;
	camera->forward = forward.dir;
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

// World point to camera-relative rotated coordinates: vertex->pos is read
// as a world position, not a model-space one. The frame's columns are
// right, down and forward, so transpose(frame) is the view rotation.
inline void RETRO_ViewVertex(Vertex *vertex, const RETRO_Camera *camera)
{
	mat3 frame = { camera->right, camera->down, camera->forward };
	vertex->rpos = transpose(frame) * (vertex->pos - camera->pos);
}

// World direction to camera-relative: the rotational half of
// RETRO_ViewVertex. A UnitVector does not translate, so the eye is not
// subtracted; only the frame is applied, and it lands in rdir, which is
// the slot RETRO_RotatedDot reads.
inline void RETRO_ViewUnitVector(UnitVector *direction, const RETRO_Camera *camera)
{
	mat3 frame = { camera->right, camera->down, camera->forward };
	direction->rdir = transpose(frame) * direction->dir;
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
inline void RETRO_FrameFromForward(UnitVector forward, UnitVector *right, UnitVector *down)
{
	UnitVector worlddown = { { 0, 1, 0 } };
	*right = RETRO_UnitCrossProduct(worlddown, forward);
	*down = RETRO_UnitCrossProduct(forward, *right);
}

// Pinhole projection of an already camera-relative vertex (see
// RETRO_ViewVertex): depth is rpos.z itself, with no near-plane offset to
// fold in, since the eye already sits at the origin of that space.
//
//   q  = 1 / rpos.z
//   sx = cx + focal * rpos.x * q
//   sy = cy + focal * rpos.y * q
//
// A vertex at or behind the eye is given q = 0 and parked at the principal
// point, for the caller to drop, matching RETRO_ProjectVertex.
inline void RETRO_ProjectViewVertex(Vertex *vertex, float focal = RETRO_CAMERA_FOCAL, float cx = (RETRO_WIDTH / 2.0), float cy = (RETRO_HEIGHT / 2.0))
{
	if (vertex->rpos.z <= 1.0f) {
		vertex->q = 0.0f;
		vertex->spos = { cx, cy };
	} else {
		vertex->q = 1.0f / vertex->rpos.z;
		vertex->spos = { cx + focal * vertex->rpos.x * vertex->q, cy + focal * vertex->rpos.y * vertex->q };
	}
}

#endif
