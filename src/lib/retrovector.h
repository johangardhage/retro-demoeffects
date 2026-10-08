//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
// GLSL/GLM-style vector math - vec2, vec3, vec4 and ivec2, with operator
// overloading and free functions (dot, cross, length, normalize) at global scope, bare,
// matching GLSL naming exactly. This header and retromatrix.h are the two in
// the library that follow industry convention instead of this library's own:
// everywhere else is global scope with a RETRO_ prefix, specifically to keep
// names apart across many small demos. Here that collision risk is accepted
// on purpose - the demos this is for are small, and dot/cross/normalize/length
// read better unprefixed, the way GLSL itself reads.
//
// These types, like retromatrix.h's mat3, carry no invariant: Vertex is a
// position that has been through a pipeline of named stages, and UnitVector
// is unit length by construction (both in retromodel.h, with the helpers that
// keep them so in retromath.h) - vec3 and vec4 are neither. They are for a
// demo that just wants plain vector algebra - a particle's position, a
// velocity, a spline point - with no pipeline attached. Do not reach for vec3
// in place of Vertex or UnitVector: nothing here tracks rotated vs. authored
// coordinates, a screen projection, or unit length, and mixing the two
// families (say, passing a vec3 where a UnitVector's assumed-unit invariant
// matters) would silently reopen the exact class of bug that split exists to
// prevent.
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
// without a jolt. x beyond either edge clamps to 0 or 1. The edges must
// differ: equal ones divide by zero, as GLSL leaves them undefined.
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

// smoothstep's quintic, 6t⁵ - 15t⁴ + 10t³: the curvature is zero at both ends
// as well as the slope, so what it eases meets a hold with no jolt in its
// acceleration either. The edges must differ, as for smoothstep.
inline float smootherstep(float edge0, float edge1, float x)
{
	float t = CLAMP01((x - edge0) / (edge1 - edge0));
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

inline double smootherstep(double edge0, double edge1, double x)
{
	double t = CLAMP01((x - edge0) / (edge1 - edge0));
	return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// GLSL's clamp: x held to [low, high], both ends included, where CLAMP
// stops one short of its upper end.
inline float clamp(float x, float low, float high) { return x < low ? low : (x > high ? high : x); }
inline double clamp(double x, double low, double high) { return x < low ? low : (x > high ? high : x); }

// GLSL's mix: x at a = 0, y at a = 1 and the straight line between them,
// which carries on past either end when a does.
inline float mix(float x, float y, float a) { return x + (y - x) * a; }
inline double mix(double x, double y, double a) { return x + (y - x) * a; }

// GLSL's fract: what x has above the whole number below it, in [0, 1]. A
// negative x is measured from the whole number below it too, so fract(-0.25)
// is 0.75. The answer reaches 1 only for a negative x too small to tell from
// zero once it is added to 1, so a caller indexing with it clamps.
inline float fract(float x) { return x - floorf(x); }
inline double fract(double x) { return x - floor(x); }

// GLSL's step: 0 while x is below edge, 1 from edge on.
inline float step(float edge, float x) { return x < edge ? 0 : 1; }
inline double step(double edge, double x) { return x < edge ? 0 : 1; }

// GLSL's mod: x - y * floor(x / y), the remainder that takes the sign of y,
// so a negative x wraps up into [0, y] where fmod would leave it negative.
// It is taken from fmod, which is exact, and not from the quotient, which
// rounds. The answer reaches y only for a negative x too small to tell from
// zero once it is added to y, so a caller indexing with it clamps.
inline float mod(float x, float y)
{
	float r = fmodf(x, y);
	return r != 0 && (r < 0) != (y < 0) ? r + y : r;
}

inline double mod(double x, double y)
{
	double r = fmod(x, y);
	return r != 0 && (r < 0) != (y < 0) ? r + y : r;
}

// GLSL's inversesqrt: one over the square root of x, which is what scales a
// vector to unit length when x is its length squared.
inline float inversesqrt(float x) { return 1.0f / sqrtf(x); }
inline double inversesqrt(double x) { return 1.0 / sqrt(x); }

// GLSL's radians: an angle in degrees as radians. It takes and gives a
// double, so a whole number of degrees needs no cast.
inline double radians(double degrees) { return degrees * (M_PI / 180); }

//
// vec2
//

inline vec2 operator+(vec2 a, vec2 b) { return { a.x + b.x, a.y + b.y }; }
inline vec2 operator-(vec2 a, vec2 b) { return { a.x - b.x, a.y - b.y }; }
inline vec2 operator-(vec2 v) { return { -v.x, -v.y }; }
inline vec2 operator+(vec2 v, float s) { return { v.x + s, v.y + s }; }
inline vec2 operator+(float s, vec2 v) { return v + s; }
inline vec2 operator-(vec2 v, float s) { return { v.x - s, v.y - s }; }
inline vec2 operator-(float s, vec2 v) { return { s - v.x, s - v.y }; }

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

inline vec2 mix(vec2 a, vec2 b, float t) { return a + (b - a) * t; }

// v turned by angle radians, from +x toward +y: counterclockwise in the math
// (y-up) sense, so clockwise on this library's y-down screen, as cross reads
inline vec2 rotate(vec2 v, float angle)
{
	float c = cosf(angle);
	float s = sinf(angle);
	return { v.x * c - v.y * s, v.x * s + v.y * c };
}

inline vec2 min(vec2 a, vec2 b) { return { MIN(a.x, b.x), MIN(a.y, b.y) }; }
inline vec2 max(vec2 a, vec2 b) { return { MAX(a.x, b.x), MAX(a.y, b.y) }; }
inline vec2 min(vec2 v, float s) { return { MIN(v.x, s), MIN(v.y, s) }; }
inline vec2 max(vec2 v, float s) { return { MAX(v.x, s), MAX(v.y, s) }; }

//
// vec3
//

inline vec3 operator+(vec3 a, vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline vec3 operator-(vec3 a, vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline vec3 operator-(vec3 v) { return { -v.x, -v.y, -v.z }; }
inline vec3 operator+(vec3 v, float s) { return { v.x + s, v.y + s, v.z + s }; }
inline vec3 operator+(float s, vec3 v) { return v + s; }
inline vec3 operator-(vec3 v, float s) { return { v.x - s, v.y - s, v.z - s }; }
inline vec3 operator-(float s, vec3 v) { return { s - v.x, s - v.y, s - v.z }; }

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

inline vec3 mix(vec3 a, vec3 b, float t) { return a + (b - a) * t; }

// GLSL's reflect: the incident direction I mirrored about the surface whose
// normal is N. N must be unit length; I need not be, and the result keeps
// its length. I points toward the surface, the result away from it.
inline vec3 reflect(vec3 I, vec3 N) { return I - N * (2.0f * dot(N, I)); }

// GLSL's refract: the incident direction I bent through the surface whose
// normal is N, by Snell's law, with eta the ratio of the refractive indices,
// the side I comes from over the side it enters. I and N must be unit length
// and N must face against I. Past the critical angle the light cannot leave,
// and the result is zero.
inline vec3 refract(vec3 I, vec3 N, float eta)
{
	float d = dot(N, I);
	float k = 1.0f - eta * eta * (1.0f - d * d);
	return k < 0.0f ? vec3{ 0.0f, 0.0f, 0.0f } : I * eta - N * (eta * d + sqrtf(k));
}

inline vec3 min(vec3 a, vec3 b) { return { MIN(a.x, b.x), MIN(a.y, b.y), MIN(a.z, b.z) }; }
inline vec3 max(vec3 a, vec3 b) { return { MAX(a.x, b.x), MAX(a.y, b.y), MAX(a.z, b.z) }; }
inline vec3 min(vec3 v, float s) { return { MIN(v.x, s), MIN(v.y, s), MIN(v.z, s) }; }
inline vec3 max(vec3 v, float s) { return { MAX(v.x, s), MAX(v.y, s), MAX(v.z, s) }; }
inline vec3 abs(vec3 v) { return { fabs(v.x), fabs(v.y), fabs(v.z) }; }
inline vec3 clamp(vec3 v, float low, float high) { return { clamp(v.x, low, high), clamp(v.y, low, high), clamp(v.z, low, high) }; }

//
// vec4
//

inline vec4 operator+(vec4 a, vec4 b) { return { a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w }; }
inline vec4 operator-(vec4 a, vec4 b) { return { a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w }; }
inline vec4 operator-(vec4 v) { return { -v.x, -v.y, -v.z, -v.w }; }
inline vec4 operator+(vec4 v, float s) { return { v.x + s, v.y + s, v.z + s, v.w + s }; }
inline vec4 operator+(float s, vec4 v) { return v + s; }
inline vec4 operator-(vec4 v, float s) { return { v.x - s, v.y - s, v.z - s, v.w - s }; }
inline vec4 operator-(float s, vec4 v) { return { s - v.x, s - v.y, s - v.z, s - v.w }; }

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

inline vec4 mix(vec4 a, vec4 b, float t) { return a + (b - a) * t; }

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
