//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
// GLSL/GLM-style vector math - vec2, vec3, vec4 and ivec2, with operator
// overloading and free functions (dot, cross, length, normalize) at global scope, bare,
// matching GLSL naming exactly. This is the one header in the library that
// follows industry convention instead of this library's own: everywhere
// else is global scope with a RETRO_ prefix, specifically to keep names
// apart across many small demos. Here that collision risk is accepted on
// purpose - the demos this is for are small, and dot/cross/normalize/length
// read better unprefixed, the way GLSL itself reads.
//
// This is also the one header whose types carry no invariant: Vertex is a
// position that has been through a pipeline of named stages, and UnitVector
// is unit length by construction (see retromath.h) - vec3 and vec4 are
// neither. They are for a demo that just wants plain vector algebra - a
// particle's position, a velocity, a spline point - with no pipeline
// attached. Do not reach for vec3 in place of Vertex or UnitVector: nothing
// here tracks rotated vs. authored coordinates, a screen projection, or
// unit length, and mixing the two families (say, passing a vec3 where a
// UnitVector's assumed-unit invariant matters) would silently reopen the
// exact class of bug retromath.h's split exists to prevent.
//

#ifndef _RETROVECTOR_H_
#define _RETROVECTOR_H_

#include "retro.h"

struct vec2 {
	float x, y;
};

struct vec3 {
	float x, y, z;
};

struct vec4 {
	float x, y, z, w;
};

//
// Scalars
//

// GLSL's smoothstep: 0 at edge0, 1 at edge1 and the cubic 3t² - 2t³ between,
// whose slope is zero at both ends, so whatever it eases starts and stops
// without a jolt. x beyond either edge clamps to 0 or 1.
inline float smoothstep(float edge0, float edge1, float x)
{
	float t = CLAMP01((x - edge0) / (edge1 - edge0));
	return t * t * (3.0f - 2.0f * t);
}

inline double smoothstep(double edge0, double edge1, double x)
{
	double t = CLAMP01((x - edge0) / (edge1 - edge0));
	return t * t * (3.0 - 2.0 * t);
}

//
// vec2
//

inline vec2 operator+(vec2 a, vec2 b) { return { a.x + b.x, a.y + b.y }; }
inline vec2 operator-(vec2 a, vec2 b) { return { a.x - b.x, a.y - b.y }; }
inline vec2 operator-(vec2 v) { return { -v.x, -v.y }; }

inline vec2 operator*(vec2 a, vec2 b) { return { a.x * b.x, a.y * b.y }; }
inline vec2 operator*(vec2 v, float s) { return { v.x * s, v.y * s }; }
inline vec2 operator*(float s, vec2 v) { return v * s; }
inline vec2 operator/(vec2 a, vec2 b) { return { a.x / b.x, a.y / b.y }; }
inline vec2 operator/(vec2 v, float s) { return { v.x / s, v.y / s }; }

inline vec2 &operator+=(vec2 &a, vec2 b) { return a = a + b; }
inline vec2 &operator-=(vec2 &a, vec2 b) { return a = a - b; }
inline vec2 &operator*=(vec2 &v, float s) { return v = v * s; }
inline vec2 &operator/=(vec2 &v, float s) { return v = v / s; }

inline bool operator==(vec2 a, vec2 b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(vec2 a, vec2 b) { return !(a == b); }

inline float dot(vec2 a, vec2 b) { return a.x * b.x + a.y * b.y; }
inline float length(vec2 v) { return sqrt(dot(v, v)); }
inline float distance(vec2 a, vec2 b) { return length(a - b); }

// The z-component of the 3D cross product of two vectors with z = 0 - a
// scalar rather than a vec2, since that is the only part of the 3D
// result that survives. Its magnitude is the area of the parallelogram
// the two vectors span, and its sign is the turn from a to b: in the
// standard math (y-up) sense positive is counterclockwise, but this
// library's screen coordinates are y-down, which flips it - positive
// here is a clockwise turn on screen, negative counterclockwise.
inline float cross(vec2 a, vec2 b) { return a.x * b.y - a.y * b.x; }

// Multiplies by the reciprocal rather than dividing by len: one divide
// instead of two/three/four, and it matches this codebase's own
// hand-written normalize idiom (compute 1/length once, then scale) bit
// for bit, which a straight v / len would not - division and
// multiply-by-reciprocal round differently in IEEE 754.
inline vec2 normalize(vec2 v)
{
	float len = length(v);
	float invlen = len > 0.0f ? 1.0f / len : 0.0f;
	return v * invlen;
}

inline vec2 lerp(vec2 a, vec2 b, float t) { return a + (b - a) * t; }

//
// vec3
//

inline vec3 operator+(vec3 a, vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline vec3 operator-(vec3 a, vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline vec3 operator-(vec3 v) { return { -v.x, -v.y, -v.z }; }

inline vec3 operator*(vec3 a, vec3 b) { return { a.x * b.x, a.y * b.y, a.z * b.z }; }
inline vec3 operator*(vec3 v, float s) { return { v.x * s, v.y * s, v.z * s }; }
inline vec3 operator*(float s, vec3 v) { return v * s; }
inline vec3 operator/(vec3 a, vec3 b) { return { a.x / b.x, a.y / b.y, a.z / b.z }; }
inline vec3 operator/(vec3 v, float s) { return { v.x / s, v.y / s, v.z / s }; }

inline vec3 &operator+=(vec3 &a, vec3 b) { return a = a + b; }
inline vec3 &operator-=(vec3 &a, vec3 b) { return a = a - b; }
inline vec3 &operator*=(vec3 &v, float s) { return v = v * s; }
inline vec3 &operator/=(vec3 &v, float s) { return v = v / s; }

inline bool operator==(vec3 a, vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
inline bool operator!=(vec3 a, vec3 b) { return !(a == b); }

inline float dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline vec3 cross(vec3 a, vec3 b)
{
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}

inline float length(vec3 v) { return sqrt(dot(v, v)); }
inline float distance(vec3 a, vec3 b) { return length(a - b); }

// See vec2's normalize for why this multiplies by the reciprocal instead
// of dividing.
inline vec3 normalize(vec3 v)
{
	float len = length(v);
	float invlen = len > 0.0f ? 1.0f / len : 0.0f;
	return v * invlen;
}

inline vec3 lerp(vec3 a, vec3 b, float t) { return a + (b - a) * t; }

// GLSL's reflect: the incident direction I mirrored about the surface whose
// normal is N. N must be unit length; I need not be, and the result keeps
// its length. I points toward the surface, the result away from it.
inline vec3 reflect(vec3 I, vec3 N) { return I - N * (2.0f * dot(N, I)); }

inline vec3 min(vec3 a, vec3 b) { return { MIN(a.x, b.x), MIN(a.y, b.y), MIN(a.z, b.z) }; }
inline vec3 max(vec3 a, vec3 b) { return { MAX(a.x, b.x), MAX(a.y, b.y), MAX(a.z, b.z) }; }
inline vec3 abs(vec3 v) { return { fabs(v.x), fabs(v.y), fabs(v.z) }; }

//
// vec4
//

inline vec4 operator+(vec4 a, vec4 b) { return { a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w }; }
inline vec4 operator-(vec4 a, vec4 b) { return { a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w }; }
inline vec4 operator-(vec4 v) { return { -v.x, -v.y, -v.z, -v.w }; }

inline vec4 operator*(vec4 a, vec4 b) { return { a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w }; }
inline vec4 operator*(vec4 v, float s) { return { v.x * s, v.y * s, v.z * s, v.w * s }; }
inline vec4 operator*(float s, vec4 v) { return v * s; }
inline vec4 operator/(vec4 a, vec4 b) { return { a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w }; }
inline vec4 operator/(vec4 v, float s) { return { v.x / s, v.y / s, v.z / s, v.w / s }; }

inline vec4 &operator+=(vec4 &a, vec4 b) { return a = a + b; }
inline vec4 &operator-=(vec4 &a, vec4 b) { return a = a - b; }
inline vec4 &operator*=(vec4 &v, float s) { return v = v * s; }
inline vec4 &operator/=(vec4 &v, float s) { return v = v / s; }

inline bool operator==(vec4 a, vec4 b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }
inline bool operator!=(vec4 a, vec4 b) { return !(a == b); }

inline float dot(vec4 a, vec4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
inline float length(vec4 v) { return sqrt(dot(v, v)); }
inline float distance(vec4 a, vec4 b) { return length(a - b); }

// See vec2's normalize for why this multiplies by the reciprocal instead
// of dividing.
inline vec4 normalize(vec4 v)
{
	float len = length(v);
	float invlen = len > 0.0f ? 1.0f / len : 0.0f;
	return v * invlen;
}

inline vec4 lerp(vec4 a, vec4 b, float t) { return a + (b - a) * t; }

//
// ivec2
//
// GLSL's integer sibling of vec2 - pixel coordinates and grid indices, not
// continuous math. No dot, length or normalize: GLSL does not define those
// for its integer vector types either, since they only make sense on a
// continuous quantity.
//

struct ivec2 {
	int x, y;
};

inline ivec2 operator+(ivec2 a, ivec2 b) { return { a.x + b.x, a.y + b.y }; }
inline ivec2 operator-(ivec2 a, ivec2 b) { return { a.x - b.x, a.y - b.y }; }
inline ivec2 operator-(ivec2 v) { return { -v.x, -v.y }; }

inline ivec2 operator*(ivec2 a, ivec2 b) { return { a.x * b.x, a.y * b.y }; }
inline ivec2 operator*(ivec2 v, int s) { return { v.x * s, v.y * s }; }
inline ivec2 operator*(int s, ivec2 v) { return v * s; }
inline ivec2 operator/(ivec2 a, ivec2 b) { return { a.x / b.x, a.y / b.y }; }
inline ivec2 operator/(ivec2 v, int s) { return { v.x / s, v.y / s }; }

inline ivec2 &operator+=(ivec2 &a, ivec2 b) { return a = a + b; }
inline ivec2 &operator-=(ivec2 &a, ivec2 b) { return a = a - b; }
inline ivec2 &operator*=(ivec2 &v, int s) { return v = v * s; }
inline ivec2 &operator/=(ivec2 &v, int s) { return v = v / s; }

inline bool operator==(ivec2 a, ivec2 b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(ivec2 a, ivec2 b) { return !(a == b); }

#endif
