//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
// GLSL/GLM-style 3x3 matrix - mat3, with operator overloading at global
// scope, bare, matching GLSL naming exactly. Stored as three columns, the
// same layout and multiplication convention as GLSL's mat3: mat3(c0, c1, c2)
// applied to a column vector v is v.x*c0 + v.y*c1 + v.z*c2, so column i is
// where basis vector i lands. This follows retrovector.h's own exception to
// the library's RETRO_ prefix, for the same reason: plain matrix algebra
// reads better unprefixed, the way GLSL itself reads.
//

#ifndef _RETROMATRIX_H_
#define _RETROMATRIX_H_

#include "retrovector.h"

struct mat3 {
	vec3 col0, col1, col2;
};

inline vec3 operator*(const mat3 &m, vec3 v) { return v.x * m.col0 + v.y * m.col1 + v.z * m.col2; }

inline mat3 operator*(const mat3 &a, const mat3 &b) { return { a * b.col0, a * b.col1, a * b.col2 }; }

inline mat3 identity() { return { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } }; }

// GLSL transpose. For an orthonormal rotation this is also the inverse.
inline mat3 transpose(const mat3 &m)
{
	return {
		{ m.col0.x, m.col1.x, m.col2.x },
		{ m.col0.y, m.col1.y, m.col2.y },
		{ m.col0.z, m.col1.z, m.col2.z }
	};
}

// GLM-style axis maps. The (c, s) overloads do not require c²+s² = 1; a
// squash effect can feed a pair that is not unit.
inline mat3 rotateX(float c, float s) { return { { 1, 0, 0 }, { 0, c, s }, { 0, -s, c } }; }
inline mat3 rotateY(float c, float s) { return { { c, 0, -s }, { 0, 1, 0 }, { s, 0, c } }; }
inline mat3 rotateZ(float c, float s) { return { { c, s, 0 }, { -s, c, 0 }, { 0, 0, 1 } }; }

inline mat3 rotateX(float angle) { return rotateX(cos(angle), sin(angle)); }
inline mat3 rotateY(float angle) { return rotateY(cos(angle), sin(angle)); }
inline mat3 rotateZ(float angle) { return rotateZ(cos(angle), sin(angle)); }

// Rz * Ry * Rx. Six-arg form is the same product of (c, s) maps.
inline mat3 rotate(float cx, float sx, float cy, float sy, float cz, float sz) { return rotateZ(cz, sz) * rotateY(cy, sy) * rotateX(cx, sx); }
inline mat3 rotate(float ax, float ay, float az) { return rotateZ(az) * rotateY(ay) * rotateX(ax); }

#endif
