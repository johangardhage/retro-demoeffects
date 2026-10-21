//
// Rollercoaster
//
// A polygon rollercoaster: three trains running a looping track over a field,
// seen first from outside while the camera circles the coaster, then from a
// seat in the middle of one of the trains.
//
// The track and its supports are assets/rollercoaster.map. The track is a loop
// of 500 path_corner entities along its center line, the station at track1,
// each with the way across it and the way up from it. Around that line the
// track is built here: a thick spine below the riders, two thin rails at
// either side of it, and a pair of struts from the spine out to the rails at
// every other point. The supports are posts of straight pieces, each a brush,
// a square tube around its middle line; its sides are drawn and its ends,
// textured caulk, left open.
//
// All of it is built at three levels of detail, and the intro draws each piece
// by how far it is from the eye. Near, it is as above, each side mapped with
// its material's phong map by the way its corners face. Farther, the spine has
// four sides and the rails three, and the struts and the supports are tubes of
// three sides each filled flat with one shade. Farthest, the spine is three
// flat sides and the rails, the struts and the supports are lines. The ride
// draws every piece near.
//
// The world is z up and right handed, in the units of the track. The field is
// a square of the floor picture, with the coaster's shadow painted in, lying
// flat under it. Around it the sky is a dome, its picture wrapped once around
// the horizon and drawn to the zenith, and under the horizon a band of haze
// hangs down behind the field. Both are as far away as the horizon, so each
// pixel looks its color up by the direction it sees.
//
// A train is five cars, assets/rollercoaster_car1.obj in front and
// assets/rollercoaster_car2.obj behind it, each sitting on the track
// CAR_SPACING apart. The trains run a timing of the ride, worked out from
// the track: pulled up the lift, rolling down and round under gravity and
// friction, and braked back into the station. From it the middle car has a
// key every PATH_POINTS points of the track, when it passes and how fast it
// moves and turns then; between those it is eased along the track at those
// speeds. The other two trains run the same timing, TRAIN_LEAD seconds
// ahead. The eye rides the middle car's path, which passes the same points at
// the same times but sweeps between them on the keys' velocities and turns.
//
// The intro circles the coaster at a closing distance, sinking toward the
// field and turning faster as it goes. Then a white flash hands over to the
// ride, the eye in the middle car of the last train, which is not drawn. The
// ride speeds up toward its end and fades out to white. Return skips the
// intro.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retrocamera.h"
#include "lib/retromodel.h"
#include "lib/retromap.h"

// The clock: ride time runs from -INTRO_TIME, the start of the intro, through
// 0, the start of the ride, to RIDE_TIME, when it starts over
constexpr double INTRO_TIME = 25.80645161;
constexpr double RIDE_TIME = 66.5;
constexpr double SEQUENCE_TIME = INTRO_TIME + RIDE_TIME;

// The palette fades in from black at the start of the intro, flashes white at
// the start of the ride and fades out to white at the end, each a ramp of
// levels of 256 held once it is full
constexpr double FADE_IN_START = -25.5;		// level (t - this) * FADE_IN_RATE
constexpr double FADE_IN_RATE = 170;
constexpr double FLASH_RATE = 1024;			// level t * FLASH_RATE
constexpr double FADE_OUT_END = 66.6;		// level (this - t) * FADE_OUT_RATE
constexpr double FADE_OUT_RATE = 512;

// From BOOST_TIME on the ride runs ahead by BOOST_RATE times the square of the
// time since
constexpr double BOOST_TIME = 64.516127;
constexpr double BOOST_RATE = 1.5;

// The intro camera, a stage s seconds after INTRO_STAGE seconds before the
// ride: it circles the coaster's z axis at INTRO_RADIUS - INTRO_CLOSING * s
// out and INTRO_DEPTH + INTRO_SINK * s down, looking level at the axis,
// turned INTRO_TURN + INTRO_SPIN * s + INTRO_SPINUP * s * s radians around it
constexpr double INTRO_STAGE = 25;
constexpr float INTRO_RADIUS = 190;
constexpr float INTRO_CLOSING = 4;
constexpr float INTRO_DEPTH = 15;
constexpr float INTRO_SINK = 1.2f;
constexpr float INTRO_TURN = -3.1f;
constexpr float INTRO_SPIN = 0.05f;
constexpr float INTRO_SPINUP = 0.002f;

// The lens, and how near each camera draws. 240 either way is the original's
// focal length across, and the same down keeps that field of view on the
// square pixels of this screen
constexpr float FOCAL = 240;
constexpr float INTRO_NEARPLANE = 0.05f;
constexpr float RIDE_NEARPLANE = 0.4f;

// The levels of detail, nearest first. The intro draws a piece of the track
// at a level while the eye is nearer it than the level's distance, and the
// last level beyond; a support likewise
constexpr int LEVELS = 3;
constexpr float TRACK_LEVEL_DISTANCE[LEVELS - 1] = { 40, 70 };
constexpr float SUPPORT_LEVEL_DISTANCE[LEVELS - 1] = { 60, 120 };

// The track, its center line where the riders' eyes are. The sides are each
// level's but the last, where the rails and the struts are lines
constexpr int TRACK_POINTS = 500;
constexpr float SPINE_DROP = 2.5f;			// below the center line
constexpr float SPINE_RADIUS = 0.5f;
constexpr int SPINE_SIDES[LEVELS] = { 6, 4, 3 };
constexpr float SPINE_TWIST = M_PI / 6;		// from up, near, so a flat side is on top rather than an edge
constexpr float RAIL_DROP = 1.3f;
constexpr float RAIL_SPREAD = 1.3f;			// to either side
constexpr float RAIL_RADIUS = 0.2f;
constexpr int RAIL_SIDES[LEVELS - 1] = { 4, 3 };
constexpr float STRUT_ANGLE = 0.7f;			// up from across, out of the spine's middle
constexpr float STRUT_START = 0.4f;
constexpr float STRUT_END = 1.65f;
constexpr float STRUT_RADIUS = 0.15f;
constexpr int STRUT_SIDES = 3;
constexpr int SUPPORT_SIDES = 3;			// farther; near it is the brush
constexpr float SUPPORT_ON_PLANE = 0.001f;	// how near a side a corner of it is

// The track is built and culled in pieces of this many of its points. Beyond
// the nearest level a piece has a ring at only every RING_STEP of its points
constexpr int PIECE_POINTS = 4;
constexpr int RING_STEP = 2;

// The middle car of a train has a key every PATH_POINTS points of the track,
// from the station. The last is back at the station, a lap on
constexpr int PATH_POINTS = 4;
constexpr int PATH_KEYS = TRACK_POINTS / PATH_POINTS + 1;

// How a train moves, worked out on the track sampled POINT_SAMPLES times a
// point, at each point and along the spline between. The chain pulls it up
// the lift at LIFT_SPEED until its middle car is at sample LIFT_TOP, then it
// rolls, the square of its speed up by 2 GRAVITY for every unit it drops and
// down by FRICTION for every unit it goes, and down by each BRAKE_RATE more
// for every unit past sample BRAKE_START, but never slower than the lift. It
// takes each step between samples at the speed at the end of it
constexpr int POINT_SAMPLES = 2;
constexpr int SAMPLES = TRACK_POINTS * POINT_SAMPLES;
constexpr float LIFT_SPEED = 9;
constexpr int LIFT_TOP = 100;
constexpr float GRAVITY = 27;
constexpr float FRICTION = 0.495f;
constexpr int BRAKES = 3;
constexpr int BRAKE_START[BRAKES] = { 875, 925, 950 };
constexpr float BRAKE_RATE[BRAKES] = { 8.5f, 18, 9 };

// Three trains of five cars, the first and the second ahead of the ride's
// own by this many seconds of the timing
constexpr int TRAINS = 3;
constexpr float TRAIN_LEAD[TRAINS] = { 60.0f, 40.7f, 0.0f };
constexpr int RIDE_TRAIN = 2;
constexpr int CARS = 5;
constexpr int MIDDLE_CAR = 2;				// the one the timing is of, and the eye rides in
constexpr float CAR_SPACING = 5.3f;			// along the track, between their middles
constexpr float CAR_REACH = 6;				// from its middle to its farthest corner
constexpr unsigned char CAR_GREY = 8;		// the color of all but its picture

// The field: a square at FLOOR_HEIGHT, from FLOOR_CORNER along FLOOR_ROWS,
// the way the picture's rows run down it, and FLOOR_COLUMNS
constexpr float FLOOR_HEIGHT = -57.5f;
constexpr vec3 FLOOR_CORNER = { -210, 40, FLOOR_HEIGHT };
constexpr vec3 FLOOR_ROWS = { 240, -240, 0 };
constexpr vec3 FLOOR_COLUMNS = { 240, 240, 0 };
constexpr int FLOOR_SIZE = 512;

// The sky picture's columns go once around, from SKY_AZIMUTH radians, and
// its rows from the zenith down to the horizon. The haze picture's run down
// from the horizon to HAZE_DEPTH radians below it
constexpr int SKY_WIDTH = 1024;
constexpr int SKY_HEIGHT = 256;
constexpr int HAZE_HEIGHT = 64;
constexpr float SKY_AZIMUTH = 1.0f;
constexpr float HAZE_DEPTH = 0.65f * M_PI / 4;

// Toward the sun. It casts the shadows painted on the field, and lights the
// track and the supports
constexpr vec3 SUN = { -0.3f, -0.7f, 1.0f };

// Each material is a ramp of 16 in the palette, from black to its lightest,
// and is shaded through a phong map PHONG_SIZE across, a disk at the lightest
// in its middle texel and a level darker for every PHONG_FALLOFF of the square
// of the texels out from it, down to black. A corner reads the map as far out
// from the middle as it is turned from facing the sun, or straight away from
// it, PHONG_REACH texels when it is square to it. A flat side is FLAT_LEVEL up
// its ramp, and up to FLAT_RANGE more by the square of how much it faces the
// sun or away from it. A line is LINE_LEVEL up its ramp
constexpr int RAMP_GREY = 16;
constexpr int RAMP_RED = 32;
constexpr int RAMP_BLUE = 96;
constexpr int RAMP_PURPLE = 112;
constexpr int RAMP_LIGHTEST = 15;
constexpr int PHONG_SIZE = 64;
constexpr float PHONG_FALLOFF = 78.77f;
constexpr float PHONG_REACH = 31.5f;
constexpr float FLAT_LEVEL = 8;
constexpr float FLAT_RANGE = 5.9f;
constexpr int LINE_LEVEL = 10;

// Room for every side, line and piece the track and the supports are built of
constexpr int MAX_QUADS = 20000;
constexpr int MAX_LINES = 2000;
constexpr int MAX_PIECES = 400;

// A point of the track's center line, how far along it, and its frame
struct TrackPoint {
	vec3 pos;
	vec3 side;
	vec3 up;
	float distance;
};

// Where along the track something is, and the way it faces: forward along
// it, side across it and up from it
struct TrackFrame {
	vec3 pos;
	vec3 forward;
	vec3 side;
	vec3 up;
};

// A side of a tube, its corners and where they read its material's phong
// map, and the way it faces out. A flat side has no map and is filled with
// color
struct Quad {
	vec3 pos[4];
	vec2 uv[4];
	vec3 normal;
	unsigned char *map;
	unsigned char color;
};

// A line and its color
struct Line {
	vec3 pos[2];
	unsigned char color;
};

// A part of the track or a support. Its sides at level i are those from
// quad[i] up to quad[i + 1], and its lines likewise. The distances it
// changes level at, and the box around it
struct Piece {
	int quad[LEVELS + 1];
	int line[LEVELS + 1];
	const float *distance;
	vec3 low;
	vec3 high;
};

// A key of the middle car's path: when it passes, and how fast it moves and
// its forward and up turn then, a second. Where it is and its up are its
// track point's, and its forward is the way it moves
struct PathKey {
	float time;
	vec3 velocity;
	vec3 dforward;
	vec3 dup;
};

// A sample of the track, how far along it, and how fast the middle car of a
// train passes it and when
struct Sample {
	vec3 pos;
	vec3 up;
	float distance;
	float speed;
	float time;
};

static TrackPoint Track[TRACK_POINTS + 1];
static Sample Samples[SAMPLES + 1];
static PathKey Path[PATH_KEYS];
static Quad Quads[MAX_QUADS];
static Line Lines[MAX_LINES];
static Piece Pieces[MAX_PIECES];
static int QuadCount;
static int LineCount;
static int PieceCount;
static Model3D *FrontCar;
static Model3D *Car;
static unsigned char *CarMap;
static unsigned char *Sky;
static unsigned char *Haze;
static unsigned char *Floor;
static RETRO_Palette Palette[RETRO_COLORS];
static unsigned char PhongMap[RETRO_COLORS / 16][PHONG_SIZE * PHONG_SIZE];
static RETRO_Camera Camera;

//
// The track
//

// The track's point i, any number of times around
static const TrackPoint &TrackPointAt(int i)
{
	return Track[WRAP(i, TRACK_POINTS)];
}

// The track's frame distance along it, from the station, any number of times
// around. The center line runs through the points as a Catmull-Rom spline,
// and its frame turns from one point's to the next
static TrackFrame TrackAt(float distance)
{
	distance = mod(distance, Track[TRACK_POINTS].distance);
	int k = 0;
	while (k < TRACK_POINTS - 1 && Track[k + 1].distance <= distance) {
		k++;
	}
	float t = (distance - Track[k].distance) / (Track[k + 1].distance - Track[k].distance);
	vec3 p0 = TrackPointAt(k - 1).pos;
	vec3 p1 = Track[k].pos;
	vec3 p2 = Track[k + 1].pos;
	vec3 p3 = TrackPointAt(k + 2).pos;
	TrackFrame frame;
	frame.pos = RETRO_CatmullRom(p0, p1, p2, p3, t);
	frame.forward = normalize(RETRO_CatmullRomDerivative(p0, p1, p2, p3, t));
	vec3 up = mix(Track[k].up, Track[k + 1].up, t);
	frame.up = normalize(up - frame.forward * dot(up, frame.forward));
	frame.side = cross(frame.forward, frame.up);
	return frame;
}

// How far along the track the middle car of a train is time seconds into
// the ride, a cubic between the keys either side that passes each at its speed
static float RideDistance(double time)
{
	RETRO_KeyframeLoop loop = RETRO_LoopKeyframes(time, Path, PATH_KEYS, sizeof(PathKey));
	const PathKey &a = Path[loop.key];
	const PathKey &b = Path[loop.key + 1];
	float from = Track[loop.key * PATH_POINTS].distance;
	float to = Track[(loop.key + 1) * PATH_POINTS].distance;
	return RETRO_Hermite(from, length(a.velocity), to, length(b.velocity), b.time - a.time, loop.t);
}

// Where the middle car's path is time seconds into the ride, and the way it
// faces: between the keys either side, a cubic in each of where it is, its
// forward and its up, that passes each key at its velocity and turns
static TrackFrame PathAt(double time)
{
	RETRO_KeyframeLoop loop = RETRO_LoopKeyframes(time, Path, PATH_KEYS, sizeof(PathKey));
	const PathKey &a = Path[loop.key];
	const PathKey &b = Path[loop.key + 1];
	const TrackPoint &p = Track[loop.key * PATH_POINTS];
	const TrackPoint &q = Track[(loop.key + 1) * PATH_POINTS];
	float h = b.time - a.time;
	TrackFrame frame;
	frame.pos = RETRO_Hermite(p.pos, a.velocity, q.pos, b.velocity, h, loop.t);
	frame.forward = normalize(RETRO_Hermite(normalize(a.velocity), a.dforward, normalize(b.velocity), b.dforward, h, loop.t));
	vec3 up = RETRO_Hermite(p.up, a.dup, q.up, b.dup, h, loop.t);
	frame.up = normalize(up - frame.forward * dot(up, frame.forward));
	frame.side = cross(frame.forward, frame.up);
	return frame;
}

//
// Building the track and the supports
//

// Where a surface facing normal reads the phong map: as far out from its
// middle as it is turned from facing the sun
static vec2 PhongUV(vec3 normal)
{
	vec3 sun = normalize(SUN);
	vec3 u = normalize(cross(sun, { 0, 0, 1 }));
	vec3 v = cross(sun, u);
	vec2 r = vec2{ dot(normal, u), dot(normal, v) } * PHONG_REACH;
	return r + PHONG_SIZE * 0.5f;
}

// A piece that changes level at distance, its levels from the nearest, each
// begun in turn and the piece then ended
static void BeginPiece(const float *distance)
{
	if (PieceCount == MAX_PIECES) {
		RETRO_RageQuit("Too many pieces of track\n");
	}
	Piece *piece = &Pieces[PieceCount];
	piece->distance = distance;
	piece->low = { INFINITY, INFINITY, INFINITY };
	piece->high = -piece->low;
}

static void BeginLevel(int i)
{
	Pieces[PieceCount].quad[i] = QuadCount;
	Pieces[PieceCount].line[i] = LineCount;
}

static void EndPiece(void)
{
	BeginLevel(LEVELS);
	PieceCount++;
}

// Grows the piece's box to hold p
static void Bound(vec3 p)
{
	Piece *piece = &Pieces[PieceCount];
	piece->low = min(piece->low, p);
	piece->high = max(piece->high, p);
}

// A side between four corners, each reading the phong map as out from it
// faces, the side facing normal, of the material of ramp. Phong mapped, or
// flat and shaded up its ramp by how much it faces the sun or away from it
static void AddQuad(const vec3 *pos, const vec3 *out, vec3 normal, int ramp, bool flat)
{
	if (QuadCount == MAX_QUADS) {
		RETRO_RageQuit("Too many sides in the track\n");
	}
	Quad *quad = &Quads[QuadCount++];
	for (int k = 0; k < 4; k++) {
		quad->pos[k] = pos[k];
		quad->uv[k] = PhongUV(out[k]);
		Bound(pos[k]);
	}
	float facing = dot(normalize(normal), normalize(SUN));
	quad->normal = normal;
	quad->map = flat ? NULL : PhongMap[ramp / 16];
	quad->color = ramp + (int)(FLAT_LEVEL + FLAT_RANGE * facing * facing);
}

// A line from a to b in the material of ramp
static void AddLine(vec3 a, vec3 b, int ramp)
{
	if (LineCount == MAX_LINES) {
		RETRO_RageQuit("Too many lines in the track\n");
	}
	Line *line = &Lines[LineCount++];
	line->pos[0] = a;
	line->pos[1] = b;
	line->color = ramp + LINE_LEVEL;
	Bound(a);
	Bound(b);
}

//
// A tube of the given sides through rings, each around the pos of its frame.
// Its corners point up, turned twist and then a side at a time toward side,
// and are shaded by the way they point, or its sides flat
//
static void AddTube(const TrackFrame *ring, int rings, float radius, int sides, float twist, int ramp, bool flat)
{
	for (int k = 0; k < rings - 1; k++) {
		for (int j = 0; j < sides; j++) {
			vec3 pos[4];
			vec3 out[4];
			for (int c = 0; c < 4; c++) {
				const TrackFrame &r = ring[k + c / 2];
				float angle = twist + 2 * M_PI * (j + (c == 1 || c == 2)) / sides;
				out[c] = r.up * cosf(angle) + r.side * sinf(angle);
				pos[c] = r.pos + out[c] * radius;
			}
			vec3 normal = cross(pos[2] - pos[0], pos[3] - pos[1]);
			AddQuad(pos, out, dot(normal, out[0] + out[2]) < 0 ? -normal : normal, ramp, flat);
		}
	}
}

// A tube along a straight line from a to b, its first corner pointing up
static void AddStraightTube(vec3 a, vec3 b, vec3 up, float radius, int sides, int ramp, bool flat)
{
	vec3 forward = normalize(b - a);
	vec3 side = normalize(cross(forward, up));
	TrackFrame ring[2] = { { a, forward, side, up }, { b, forward, side, up } };
	AddTube(ring, 2, radius, sides, 0, ramp, flat);
}

// The track's points, from the map's path_corner chain round from track1
static void LoadTrack(const RETRO_Map *map)
{
	const RETRO_MapEntity *start = RETRO_FindMapEntity(map, "targetname", "track1");
	const RETRO_MapEntity *point = start;
	for (int i = 0; i < TRACK_POINTS; i++) {
		if (point == NULL) {
			RETRO_RageQuit("The track breaks off after %d points: assets/rollercoaster.map\n", i);
		}
		Track[i].pos = RETRO_MapVector(*point, "origin");
		Track[i].up = RETRO_MapVector(*point, "up");
		const char *target = RETRO_MapValue(*point, "target");
		point = target ? RETRO_FindMapEntity(map, "targetname", target) : NULL;
	}
	if (point != start) {
		RETRO_RageQuit("The track is not a loop of %d points: assets/rollercoaster.map\n", TRACK_POINTS);
	}
	// The map's up leans along the spline on the steep banks, and its side is
	// not unit, so a ring built from them pitches and narrows there while the
	// cars, which use TrackAt, do not. Stand each point up off the spline the
	// same way TrackAt does at a control point.
	for (int i = 0; i < TRACK_POINTS; i++) {
		vec3 forward = normalize(TrackPointAt(i + 1).pos - TrackPointAt(i - 1).pos);
		vec3 up = normalize(Track[i].up - forward * dot(Track[i].up, forward));
		Track[i].up = up;
		Track[i].side = cross(forward, up);
	}
	Track[TRACK_POINTS] = Track[0];
	for (int i = 0; i <= TRACK_POINTS; i++) {
		Track[i].distance = i == 0 ? 0 : Track[i - 1].distance + distance(Track[i - 1].pos, Track[i].pos);
	}
}


// The track's sample i, any number of times around
static const Sample &SampleAt(int i)
{
	return Samples[WRAP(i, SAMPLES)];
}

// The way the track runs through sample i, from the samples either side
static vec3 SampleForward(int i)
{
	return normalize(SampleAt(i + 1).pos - SampleAt(i - 1).pos);
}

//
// The track's samples, where the middle car of a train passes each and when,
// and from them its keys: at each its time, its speed along the way the track
// runs through it, and how fast that way and the track's up turn, from the
// next sample and the time it takes to it
//
static void BuildPath(void)
{
	for (int i = 0; i <= SAMPLES; i++) {
		int k = i / POINT_SAMPLES;
		float t = (float)(i % POINT_SAMPLES) / POINT_SAMPLES;
		Sample *sample = &Samples[i];
		sample->pos = RETRO_CatmullRom(TrackPointAt(k - 1).pos, Track[k].pos, TrackPointAt(k + 1).pos, TrackPointAt(k + 2).pos, t);
		sample->up = normalize(mix(Track[k].up, TrackPointAt(k + 1).up, t));
		sample->distance = i == 0 ? 0 : Samples[i - 1].distance + distance(Samples[i - 1].pos, sample->pos);
	}
	const Sample &top = Samples[LIFT_TOP];
	for (int i = 0; i <= SAMPLES; i++) {
		Sample *sample = &Samples[i];
		float square = LIFT_SPEED * LIFT_SPEED + 2 * GRAVITY * (top.pos.z - sample->pos.z) - FRICTION * (sample->distance - top.distance);
		for (int brake = 0; brake < BRAKES; brake++) {
			square -= BRAKE_RATE[brake] * MAX(0.0f, sample->distance - Samples[BRAKE_START[brake]].distance);
		}
		sample->speed = i <= LIFT_TOP ? LIFT_SPEED : sqrtf(MAX(square, LIFT_SPEED * LIFT_SPEED));
		sample->time = i == 0 ? 0 : Samples[i - 1].time + (sample->distance - Samples[i - 1].distance) / sample->speed;
	}
	for (int k = 0; k < PATH_KEYS; k++) {
		int i = k * PATH_POINTS * POINT_SAMPLES;
		const Sample &sample = Samples[i];
		const Sample &next = SampleAt(i + 1);
		float time = distance(sample.pos, next.pos) / next.speed;
		Path[k].time = sample.time;
		Path[k].velocity = SampleForward(i) * sample.speed;
		Path[k].dforward = (SampleForward(i + 1) - SampleForward(i)) / time;
		Path[k].dup = (next.up - sample.up) / time;
	}
}

//
// The track in pieces of PIECE_POINTS points: the spine and the rails along
// them, with a ring at every step of their points, and the struts. Near, the
// spine is turned SPINE_TWIST and every point has a ring. At every other
// point a strut goes from the spine out to each rail, pointing STRUT_ANGLE up
// from across, inside the spine to inside the rail; at the last level it is
// a line along its middle
//
static void BuildTrack(void)
{
	for (int first = 0; first < TRACK_POINTS; first += PIECE_POINTS) {
		BeginPiece(TRACK_LEVEL_DISTANCE);
		for (int level = 0; level < LEVELS; level++) {
			BeginLevel(level);
			int step = level == 0 ? 1 : RING_STEP;
			int rings = PIECE_POINTS / step + 1;
			TrackFrame spine[PIECE_POINTS + 1];
			TrackFrame left[PIECE_POINTS + 1];
			TrackFrame right[PIECE_POINTS + 1];
			for (int k = 0; k < rings; k++) {
				const TrackPoint &point = Track[first + k * step];
				vec3 rail = point.pos - point.up * RAIL_DROP;
				spine[k] = { point.pos - point.up * SPINE_DROP, {}, point.side, point.up };
				left[k] = { rail - point.side * RAIL_SPREAD, {}, point.side, point.up };
				right[k] = { rail + point.side * RAIL_SPREAD, {}, point.side, point.up };
			}
			float twist = level == 0 ? SPINE_TWIST : 0;
			AddTube(spine, rings, SPINE_RADIUS, SPINE_SIDES[level], twist, RAMP_RED, level == LEVELS - 1);
			if (level == LEVELS - 1) {
				for (int k = 0; k < rings - 1; k++) {
					AddLine(left[k].pos, left[k + 1].pos, RAMP_BLUE);
					AddLine(right[k].pos, right[k + 1].pos, RAMP_BLUE);
				}
			} else {
				AddTube(left, rings, RAIL_RADIUS, RAIL_SIDES[level], 0, RAMP_BLUE, false);
				AddTube(right, rings, RAIL_RADIUS, RAIL_SIDES[level], 0, RAMP_BLUE, false);
			}
			for (int k = 1; k < PIECE_POINTS; k += 2) {
				const TrackPoint &point = Track[first + k];
				vec3 spine = point.pos - point.up * SPINE_DROP;
				for (int side = -1; side <= 1; side += 2) {
					// A strut's first corner points square to it in the
					// plane of the track's side and up
					vec3 out = point.side * (side * cosf(STRUT_ANGLE)) + point.up * sinf(STRUT_ANGLE);
					vec3 up = point.side * (side * sinf(STRUT_ANGLE)) - point.up * cosf(STRUT_ANGLE);
					vec3 start = spine + out * STRUT_START;
					vec3 end = spine + out * STRUT_END;
					if (level == LEVELS - 1) AddLine(end, start, RAMP_PURPLE);
					else AddStraightTube(start, end, up, STRUT_RADIUS, STRUT_SIDES, RAMP_PURPLE, level > 0);
				}
			}
		}
		EndPiece();
	}
}

//
// The supports: the map's brushes, each a piece of its own. Near, their sides
// textured support are drawn, each corner shaded by the way it points out from
// the tube's middle, between the two sides it is on. Farther, each is a tube
// of SUPPORT_SIDES flat sides along its middle line, between the middles of
// its ends, textured caulk, turned to the first point of the brush's first
// plane. Farthest, it is the line
//
static void BuildSupports(const RETRO_Map *map)
{
	for (int i = 0; i < map->brushes; i++) {
		const RETRO_MapBrush &brush = map->brush[i];
		BeginPiece(SUPPORT_LEVEL_DISTANCE);
		BeginLevel(0);
		vec3 end[2];
		int ends = 0;
		for (int j = 0; j < brush.planes; j++) {
			vec3 corner[RETRO_MAP_MAX_FACE];
			int corners = RETRO_MapBrushFace(brush, j, corner);
			if (strcmp(brush.plane[j].texture, "caulk") == 0) {
				if (ends == 2) {
					RETRO_RageQuit("A support has more than two ends: assets/rollercoaster.map\n");
				}
				end[ends] = {};
				for (int k = 0; k < corners; k++) {
					end[ends] += corner[k] * (1.0f / corners);
				}
				ends++;
				continue;
			}
			if (corners != 4) {
				RETRO_RageQuit("A support side is not four cornered: assets/rollercoaster.map\n");
			}
			vec3 out[4] = {};
			for (int k = 0; k < 4; k++) {
				for (int l = 0; l < brush.planes; l++) {
					const RETRO_MapPlane &plane = brush.plane[l];
					bool on = fabsf(dot(plane.normal, corner[k]) - plane.distance) < SUPPORT_ON_PLANE;
					if (on && strcmp(plane.texture, "support") == 0) out[k] += plane.normal;
				}
				out[k] = normalize(out[k]);
			}
			AddQuad(corner, out, brush.plane[j].normal, RAMP_GREY, false);
		}
		if (ends != 2) {
			RETRO_RageQuit("A support has not two ends: assets/rollercoaster.map\n");
		}
		BeginLevel(1);
		vec3 along = normalize(end[1] - end[0]);
		vec3 out = brush.plane[0].point[0] - end[0];
		out -= along * dot(out, along);
		AddStraightTube(end[0], end[1], normalize(out), length(out), SUPPORT_SIDES, RAMP_GREY, true);
		BeginLevel(2);
		AddLine(end[0], end[1], RAMP_GREY);
		EndPiece();
	}
}

//
// Drawing
//

//
// The sky and the haze, as far away as the horizon: each pixel's color is
// where the direction it sees falls on the dome or the band
//
static void DrawSky(void)
{
	unsigned char *pixel = RETRO.framebuffer;
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			vec3 ray = RETRO_ViewRay(&Camera, { x + 0.5f, y + 0.5f });
			float elevation = atan2f(ray.z, sqrtf(ray.x * ray.x + ray.y * ray.y));
			float azimuth = atan2f(ray.y, ray.x) - SKY_AZIMUTH;
			int u = WRAP(azimuth * (SKY_WIDTH / (2 * M_PI)), SKY_WIDTH);
			if (elevation >= 0) {
				int v = CLAMP((M_PI / 2 - elevation) * (SKY_HEIGHT / (M_PI / 2)), 0, SKY_HEIGHT);
				*pixel++ = Sky[v * SKY_WIDTH + u];
			} else {
				int v = CLAMP(-elevation * (HAZE_HEIGHT / HAZE_DEPTH), 0, HAZE_HEIGHT);
				*pixel++ = Haze[v * SKY_WIDTH + u];
			}
		}
	}
}

static void DrawFloor(void)
{
	vec3 corner[4] = { FLOOR_CORNER, FLOOR_CORNER + FLOOR_ROWS, FLOOR_CORNER + FLOOR_ROWS + FLOOR_COLUMNS, FLOOR_CORNER + FLOOR_COLUMNS };
	vec2 uv[4] = { { 0, 0 }, { 0, FLOOR_SIZE }, { FLOOR_SIZE, FLOOR_SIZE }, { FLOOR_SIZE, 0 } };
	RETRO_CameraVertex vertex[4];
	for (int i = 0; i < 4; i++) {
		vertex[i] = RETRO_ViewVertex(&Camera, corner[i]);
		vertex[i].uv = uv[i];
	}
	RETRO_ProjectedPolygon projected;
	RETRO_CameraClipProject(Camera.lens, vertex, 4, &projected);
	if (projected.count >= 3) RETRO_DrawTexMapPolygon(projected.point, projected.count, Floor, FLOOR_SIZE, FLOOR_SIZE);
}

//
// A line from a to b in the camera's frame, a pixel wide, a step along it
// for every pixel across or down it covers, where it is nearer than what is
// drawn there. 1 / depth runs evenly along it on the screen, as it does
// across a polygon
//
static void DrawLine(vec3 a, vec3 b, unsigned char color)
{
	if (!RETRO_CutViewLine(&a, &b, Camera.lens.nearplane)) return;
	PolygonPoint from = RETRO_ProjectViewPoint(Camera.lens, a);
	PolygonPoint to = RETRO_ProjectViewPoint(Camera.lens, b);
	vec2 delta = to.pos - from.pos;
	int steps = (int)ceilf(MAX(fabsf(delta.x), fabsf(delta.y)));
	for (int i = 0; i <= steps; i++) {
		float t = steps > 0 ? (float)i / steps : 0;
		int x = (int)floorf(from.pos.x + delta.x * t);
		int y = (int)floorf(from.pos.y + delta.y * t);
		if (x < 0 || x >= RETRO_WIDTH || y < 0 || y >= RETRO_HEIGHT) continue;
		int offset = y * RETRO_WIDTH + x;
		if (RETRO_DepthTest(offset, mix(from.q, to.q, t))) RETRO.framebuffer[offset] = color;
	}
}

//
// The track and the supports, a piece at a time, each of its sides facing
// the eye and its lines. The intro draws a piece at the first level whose
// distance the eye is nearer than, or the last, and the ride the nearest
//
static void DrawPieces(const RETRO_CameraFrustum &frustum, bool riding)
{
	for (int i = 0; i < PieceCount; i++) {
		const Piece &piece = Pieces[i];
		vec3 center = (piece.low + piece.high) * 0.5f;
		if (RETRO_FrustumSphereOutside(frustum, RETRO_ViewPoint(&Camera, center), distance(piece.low, piece.high) * 0.5f)) continue;
		float away = distance(center, Camera.pos);
		int level = 0;
		while (!riding && level < LEVELS - 1 && away >= piece.distance[level]) {
			level++;
		}
		for (int j = piece.quad[level]; j < piece.quad[level + 1]; j++) {
			const Quad &quad = Quads[j];
			if (dot(quad.normal, quad.pos[0] - Camera.pos) >= 0) continue;
			RETRO_CameraVertex vertex[4];
			for (int k = 0; k < 4; k++) {
				vertex[k] = RETRO_ViewVertex(&Camera, quad.pos[k]);
				vertex[k].uv = quad.uv[k];
			}
			RETRO_ProjectedPolygon projected;
			RETRO_CameraClipProject(Camera.lens, vertex, 4, &projected);
			if (projected.count < 3) continue;
			if (quad.map) RETRO_DrawTexMapPolygon(projected.point, projected.count, quad.map, PHONG_SIZE, PHONG_SIZE);
			else RETRO_DrawFlatPolygon(projected.point, projected.count, quad.color);
		}
		for (int j = piece.line[level]; j < piece.line[level + 1]; j++) {
			const Line &line = Lines[j];
			DrawLine(RETRO_ViewPoint(&Camera, line.pos[0]), RETRO_ViewPoint(&Camera, line.pos[1]), line.color);
		}
	}
}


//
// A car with its middle on frame: its picture mapped around the outside, and
// the rest in grey, seen from either side so the seats show from above
//
static void DrawCar(const Model3D *model, const TrackFrame &frame)
{
	int carmap = RETRO_ModelMaterial(model, "carmap");
	vec3 world[RETRO_MAX_VERTICES];
	RETRO_CameraVertex eye[RETRO_MAX_VERTICES];
	for (int i = 0; i < model->vertices; i++) {
		vec3 p = model->vertex[i].pos;
		world[i] = frame.pos + frame.side * p.x + frame.forward * p.y + frame.up * p.z;
		eye[i] = RETRO_ViewVertex(&Camera, world[i]);
	}
	for (int i = 0; i < model->faces; i++) {
		const Face &face = model->face[i];
		vec3 n = face.facenormal.dir;
		vec3 normal = frame.side * n.x + frame.forward * n.y + frame.up * n.z;
		if (face.material == carmap && dot(normal, world[face.vertex[0]] - Camera.pos) >= 0) continue;
		RETRO_CameraVertex vertex[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face.vertices; j++) {
			vertex[j] = eye[face.vertex[j]];
			vertex[j].uv = model->uv[face.uv[j]];
		}
		RETRO_ProjectedPolygon projected;
		RETRO_CameraClipProject(Camera.lens, vertex, face.vertices, &projected);
		if (projected.count < 3) continue;
		if (face.material == carmap) {
			RETRO_DrawTexMapPolygon(projected.point, projected.count, CarMap, RETRO_TEXMAP_SIZE, RETRO_TEXMAP_SIZE);
		} else {
			RETRO_DrawFlatPolygon(projected.point, projected.count, CAR_GREY);
		}
	}
}

// The trains time seconds into the ride, all but the one the eye rides in,
// when it does
static void DrawTrains(const RETRO_CameraFrustum &frustum, double time, bool riding)
{
	for (int train = 0; train < TRAINS; train++) {
		if (riding && train == RIDE_TRAIN) continue;
		float distance = RideDistance(time + TRAIN_LEAD[train]);
		for (int car = 0; car < CARS; car++) {
			TrackFrame frame = TrackAt(distance + (MIDDLE_CAR - car) * CAR_SPACING);
			if (RETRO_FrustumSphereOutside(frustum, RETRO_ViewPoint(&Camera, frame.pos), CAR_REACH)) continue;
			DrawCar(car == 0 ? FrontCar : Car, frame);
		}
	}
}

//
// The palette time seconds into the ride: faded in from black at the start
// of the intro, flashed white at the start of the ride and faded out to white
// at its end
//
static void SetPalette(double time)
{
	if (time < 0) {
		RETRO_Fade((time - FADE_IN_START) * FADE_IN_RATE / 256, Palette);
		return;
	}
	float level = CLAMP01(MIN(time * FLASH_RATE, (FADE_OUT_END - time) * FADE_OUT_RATE) / 256);
	RETRO_Palette white = { 255, 255, 255 };
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, mix(white, Palette[i], level));
	}
}

// The intro camera, circling the coaster, time seconds into the ride
static void PlaceIntroCamera(double time)
{
	float stage = time + INTRO_STAGE;
	float radius = INTRO_RADIUS - INTRO_CLOSING * stage;
	float depth = INTRO_DEPTH + INTRO_SINK * stage;
	float turn = INTRO_TURN + INTRO_SPIN * stage + INTRO_SPINUP * stage * stage;
	vec3 forward = { sinf(turn), cosf(turn), 0 };
	vec3 pos = forward * -radius - vec3{ 0, 0, depth };
	RETRO_LookAlong(&Camera, pos, forward, { 0, 0, -1 });
	Camera.lens.nearplane = INTRO_NEARPLANE;
}

// The ride camera, on the path of the middle of its train
static void PlaceRideCamera(double time)
{
	TrackFrame frame = PathAt(time + TRAIN_LEAD[RIDE_TRAIN]);
	RETRO_PlaceCamera(&Camera, frame.pos, frame.side, -frame.up, frame.forward);
	Camera.lens.nearplane = RIDE_NEARPLANE;
}

void DEMO_Render(RETRO_Time time)
{
	// Return skips the rest of the intro, putting the clock ahead to the ride
	static double skipped = 0;
	double ride = mod(time.total + skipped, SEQUENCE_TIME) - INTRO_TIME;
	if (RETRO_KeyPressed(SDL_SCANCODE_RETURN) && ride < 0) {
		skipped -= ride;
		ride = 0;
	}
	SetPalette(ride);
	if (ride > BOOST_TIME) {
		ride += (ride - BOOST_TIME) * (ride - BOOST_TIME) * BOOST_RATE;
	}

	bool riding = ride >= 0;
	if (riding) PlaceRideCamera(ride);
	else PlaceIntroCamera(ride);
	RETRO_CameraFrustum frustum = RETRO_LensFrustum(Camera.lens);

	DrawSky();
	RETRO_ClearDepthBuffer();
	DrawFloor();
	DrawPieces(frustum, riding);
	DrawTrains(frustum, ride, riding);
}

void DEMO_Initialize(void)
{
	RETRO_Image *floor = RETRO_LoadImage("assets/rollercoaster_floor_512x512.pcx");
	Floor = floor->data;
	memcpy(Palette, floor->palette, sizeof(Palette));
	Sky = RETRO_LoadImage("assets/rollercoaster_sky_1024x256.pcx")->data;
	Haze = RETRO_LoadImage("assets/rollercoaster_horizon_1024x64.pcx")->data;
	CarMap = RETRO_LoadImage("assets/rollercoaster_car_256x256.pcx")->data;

	FrontCar = RETRO_Load3DModel("assets/rollercoaster_car1.obj");
	Car = RETRO_Load3DModel("assets/rollercoaster_car2.obj");

	// Each ramp's phong map, a disk lightest in its middle texel and darker out
	// to black
	for (int i = 0; i < RETRO_COLORS / 16; i++) {
		for (int y = 0; y < PHONG_SIZE; y++) {
			for (int x = 0; x < PHONG_SIZE; x++) {
				vec2 r = vec2{ (float)x, (float)y } - PHONG_SIZE * 0.5f;
				PhongMap[i][y * PHONG_SIZE + x] = i * 16 + (int)MAX(0.0f, RAMP_LIGHTEST - dot(r, r) / PHONG_FALLOFF);
			}
		}
	}

	RETRO_Map *map = RETRO_LoadMap("assets/rollercoaster.map");
	LoadTrack(map);
	BuildPath();
	BuildTrack();
	BuildSupports(map);
	RETRO_FreeMap(map);

	Camera.lens.focalx = FOCAL;
	Camera.lens.focaly = FOCAL;
}
