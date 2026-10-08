//
// Flight sim, over the bay
//
// A jet flown over San Francisco Bay from its cockpit: a flat sky and a flat
// sea split by a hard horizon, flat green land, and a few flat-shaded
// landmarks standing on it, with a HUD in its frame above an instrument panel.
// The screen is 200 rows, the panel and HUD laid out to the pixel on them.
//
// The world is read from a Quake .map, assets/flightsim.map. The brushes of
// its worldspawn are the ground and the fixed landmarks, each face in the
// material its texture names. Its point entities are what is built here from
// a few numbers: the suspension bridges, the carrier and the ships, and where
// the flight starts.
//
// The view is filled with sky, and the sea is a polygon over it: the view cut
// along the horizon, the straight line where the view's directions turn
// level, at any roll and pitch. On the sea lie the land, the runway and its
// markings, the top faces of the ground's brushes at sea level. The ground all
// lies in one plane, where depth decides nothing, so it is painted in the
// map's order instead: each polygon is given a depth a step nearer than the
// one before.
//
// The landmarks are the map's other brushes and the boxes built here, in feet,
// lit once as they are built, since the sun does not move: each face takes
// the shade of its material that
// its turn toward the light gives it. Faces turned away are skipped and the
// rest filled through the depth buffer, cleared after the ground, which can
// hide nothing above it from an eye that is always above it too. The bridges'
// cables are lines tested against the depth buffer a pixel at a time.
//
// Every polygon goes through the camera's lens, which cuts it to the four
// sides of the view as well as the near plane, so a ground triangle reaching
// miles past the edge of the screen is not projected to a point miles off it.
//
// The flight starts parked at the stern of a carrier outside the Golden Gate.
// The flight model is the arcade one of flightsim.cpp in feet and knots, with
// the throttle in percent of full power and a fuel load to burn. The aircraft
// is held an eye height above the sea, or above the carrier's deck. It lands
// on the deck, the runway or the land with its gear down and its wings near
// level, which the ground then holds level; touching down with the gear up,
// banked onto a wing tip or on the sea is a crash.
//
// The HUD: a heading tape, a boresight cross, speed and altitude, the load
// factor, and a pitch ladder lying on the world. The panel: a stores display,
// fuel, a radar of the ships, a compass and an attitude ball, a readout of
// altitude, speed and throttle, and the latitude and longitude.
//
// The chase view looks at the aircraft from behind and above, kept level with
// the horizon so the aircraft banks and pitches in front of it. The aircraft
// is read from an OBJ file, a few dozen flat faces in feet about the pilot's
// eye, its parts named by their materials, and lit as it turns. The view
// fills the screen, with no cockpit.
//
// The speed brake and the gear each add drag, and the gear shows on the
// aircraft. Every brush and every box is also a solid: flying the eye into
// one is a crash, as is touching down with the gear up, on a wing tip or on
// the sea, shown for a few seconds before the flight starts again.
//
// Up/Down pitch and Left/Right roll. A/D work the rudder and W/S the
// throttle. B puts out the speed brake and G the gear. R steps the radar's
// range, V changes between the cockpit and the chase view, and H turns the
// HUD on and off.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_HEIGHT 200

#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retrofont.h"
#include "lib/retropoly.h"
#include "lib/retromodel.h"
#include "lib/retrocamera.h"
#include "lib/retropalette.h"
#include "lib/retrovector.h"
#include "lib/retromatrix.h"
#include "lib/retromap.h"

// The view above the panel. Rows from VIEW_BOTTOM down are always panel.
#define VIEW_BOTTOM 162
#define VIEW_FOCAL 240.0f
#define VIEW_EYELEVEL 73.0f			// The boresight's row
#define VIEW_NEARPLANE 1.0f			// Feet
#define VIEW_LINENEARPLANE 20.0f	// Lines are cut further out, so no end lands far off the screen
#define VIEW_MAXPOLYGON (FACE_MAXCORNERS + RETRO_CAMERA_CLIP_PLANES)	// A face, and one more corner for each cut
#define FACE_MAXCORNERS RETRO_MAP_MAX_FACE	// A map brush's face is a square cut once by each other plane

// The palette: flat colors, sixteen grays at 17 apart, and a ramp of shades
// for each material
#define COLOR_BLACK 0
#define COLOR_SKY 1
#define COLOR_SEA 2
#define COLOR_LAND 3
#define COLOR_RUNWAY 4
#define COLOR_MARKING 5
#define COLOR_RED 6
#define COLOR_TEXT 7
#define COLOR_GAUGE 8
#define COLOR_BALLSKY 9
#define COLOR_BALLGROUND 10
#define COLOR_POSITION 11
#define GRAY(level) (15 + (level))	// level 1 to 15

#define MATERIAL_HULL 0
#define MATERIAL_DECK 1
#define MATERIAL_ORANGE 2
#define MATERIAL_STEEL 3
#define MATERIAL_CONCRETE 4
#define MATERIAL_STONE 5
#define MATERIAL_JET 6
#define MATERIAL_CANOPY 7
#define MATERIALS 8
#define MATERIAL_FIRST 32
#define MATERIAL_SHADES 16
#define MATERIAL_AMBIENT 0.45f
static const RETRO_Palette MaterialColor[MATERIALS] = {
	{ 119, 119, 136 },	// Hull
	{ 68, 68, 68 },		// Deck
	{ 204, 68, 34 },	// Orange
	{ 153, 153, 170 },	// Steel
	{ 221, 221, 204 },	// Concrete
	{ 187, 170, 136 },	// Stone
	{ 170, 170, 187 },	// Jet
	{ 51, 51, 68 },		// Canopy
};
static const vec3 Light = normalize(vec3{ -0.40f, 0.80f, 0.45f }); // toward the light, in the world: up, west and south

// The world, in feet: x east, y up and z south of the middle of the Golden
// Gate Bridge, which the map gives with y north and z up. Where its origin is,
// in degrees north and west, for the latitude and longitude readout.
#define ORIGIN_LATITUDE 37.8199f
#define ORIGIN_LONGITUDE 122.4783f
#define FEET_PER_DEGREE_LATITUDE 364000.0f
#define FEET_PER_DEGREE_LONGITUDE 287600.0f	// At this latitude
#define MILE 5280.0f

#define CARRIER_DECK 66.0f			// Height of the flight deck
#define CARRIER_DECKHALF vec3{ 540.0f, 4.0f, 125.0f }
#define CARRIER_DECKOFFSET 15.0f	// The deck overhangs to starboard

#define FLIGHT_PITCHRATE 1.2f		// Radians per second at full stick, at most
#define FLIGHT_ROLLRATE 2.5f
#define FLIGHT_RUDDERRATE 0.3f
#define FLIGHT_TURNLOAD 2.0f		// Load factor the wings pull in a bank, in g
#define FLIGHT_PULLLOAD 6.0f		// and the further load at full stick back,
#define FLIGHT_PUSHLOAD 4.0f		// and the load taken off at full stick forward
#define FLIGHT_THROTTLERATE 40.0f	// Percent per second the key is held
#define FLIGHT_KNOTSPERPERCENT 6.0f	// Full throttle draws the speed toward 600 knots
#define FLIGHT_SPOOL 3.0f			// Seconds the speed takes to close most of the gap to the throttle
#define FLIGHT_GRAVITY (GRAVITY / KNOT)	// One g: knots gained per second in a vertical dive
#define FLIGHT_STALLSPEED 130.0f	// Knots
#define FLIGHT_SINK 80.0f			// Feet per second of sink with the wings vertical
#define FLIGHT_STALLSINK 150.0f		// Further feet per second at no speed at all
#define FLIGHT_EYEHEIGHT 10.0f		// Eye above the ground when on it
#define FLIGHT_GROUNDBANK 0.26f		// Radians of bank the wing tips clear the ground at
#define FLIGHT_CEILING 50000.0f
#define FLIGHT_LOADLAG 0.2f			// Seconds the load factor takes to settle
#define KNOT 1.6878f				// Feet per second
#define GRAVITY 32.174f				// Feet per second per second

#define BRAKE_DRAG 40.0f			// Knots lost per second with the speed brake out
#define GEAR_DRAG 10.0f				// and with the gear down
#define CRASH_PAUSE 3.0f			// Seconds a crash is shown before the next flight
#define CHASE_DISTANCE 90.0f		// Feet behind the aircraft
#define CHASE_HEIGHT 20.0f			// and above it

#define FUEL_FULL 25000.0f			// Pounds
#define FUEL_IDLE 2.0f				// Pounds per second at no throttle
#define FUEL_PERPERCENT 0.25f		// and this much more for each percent

// The HUD, in its frame of two posts and a bar across the top
#define HUD_LEFT 85
#define HUD_RIGHT 234
#define HUD_POSTTOP 46
#define HUD_BAR 37
#define HUD_BARINSET 8
#define HUD_BOTTOM 129				// The ladder stays above the nose
#define HUD_CENTER 160
#define HUD_LADDERSTEP 10			// Degrees between pitch ladder rungs
#define HUD_RUNGIN 14				// Each half of a rung runs this far out from the middle
#define HUD_RUNGOUT 19
#define HUD_TAPEY 61				// The heading tape's dots
#define HUD_TAPESCALE 2.0f			// Pixels per degree
#define HUD_TAPEHALF 28				// Half the tape's width, in pixels
#define HUD_LADDERTOP 67			// The ladder stays below the tape

// The panel, which rounds down into each side
#define PANEL_TOP 146
#define PANEL_CORNER 16

#define RADAR_X 158					// The aircraft on the radar
#define RADAR_Y 165
#define RADAR_TOP 19.0f				// Pixels from it to the top of the screen, the radar's range
static const int RadarRanges[] = { 10, 20, 40 };	// Miles
#define COMPASS_X 209
#define COMPASS_SCALE 0.125f		// Pixels per degree
#define BALL_X 209
#define BALL_Y 173
#define BALL_RADIUS 12
#define BALL_SPREAD 1.2f			// Tangent of the angle to the ball's rim


//
// The ground: flat polygons at sea level, in the order they are painted
//
struct GroundPolygon {
	vec3 corner[FACE_MAXCORNERS];
	int corners;
	unsigned char color;
};

#define MAX_GROUND 512
static GroundPolygon Ground[MAX_GROUND];
static int GroundCount;

//
// The landmarks: flat faces, each with the one color its lighting gives it,
// and the lines of the cables
//
struct WorldFace {
	vec3 corner[FACE_MAXCORNERS];
	int corners;
	vec3 normal;		// Unit, outward
	unsigned char color;
};

struct WorldLine {
	vec3 a, b;
	unsigned char color;
};

#define MAX_FACES 512
#define MAX_LINES 256
#define MAX_CONTACTS 8
#define CABLE_SEGMENTS 16
static WorldFace Faces[MAX_FACES];
static WorldLine Lines[MAX_LINES];
static vec3 Contacts[MAX_CONTACTS];	// What the radar shows
static int FaceCount, LineCount, ContactCount;

// What can be flown into: a convex solid, the inside of a set of planes
struct Solid {
	int planes;
	vec3 normal[RETRO_MAP_MAX_PLANES];		// Unit, outward
	float distance[RETRO_MAP_MAX_PLANES];	// dot(normal, any point on the plane)
};

#define MAX_SOLIDS 128
static Solid Solids[MAX_SOLIDS];
static int SolidCount;

// The aircraft, in feet about the pilot's eye: x toward the right wing, y up
// and z toward the tail. Its gear is drawn only when it is down.
static Model3D *Jet;
static int JetCanopy, JetGear;	// Its materials
static vec3 Carrier;
static vec3 CarrierAlong;		// Toward its bow, or zero with no carrier
static vec3 StartPosition;		// Where a flight starts, on the ground
static float StartHeading;		// and which way it faces, in degrees clockwise from north

struct Aircraft {
	vec3 right;
	vec3 up;
	vec3 forward;
	vec3 position;		// The eye
	vec3 velocity;		// Feet per second, sink and all
	float speed;		// Knots along forward
	float throttle;		// Percent
	float fuel;			// Pounds
	float load;			// Load factor along up, in g
	bool brake;
	bool gear;
	float wreck;		// Seconds left showing a crash
};

static Aircraft Plane;
static RETRO_Camera Camera;
static bool ChaseView;
static int RadarRange;
static bool ShowHud = true;

// A point or direction of the map, z up and y north, in this world's axes
static vec3 FromMap(vec3 v)
{
	return { v.x, v.z, -v.y };
}

// *******************************************************************
// The map
// *******************************************************************

static void AddGround(const vec3 *corner, int corners, unsigned char color)
{
	GroundPolygon &polygon = Ground[GroundCount++];
	for (int i = 0; i < corners; i++) {
		polygon.corner[i] = corner[i];
	}
	polygon.corners = corners;
	polygon.color = color;
}

// The ground under (x, z): the carrier's flight deck, or sea level
static float GroundHeight(float x, float z)
{
	vec3 offset = { x - Carrier.x, 0, z - Carrier.z };
	vec3 starboard = cross(CarrierAlong, vec3{ 0, 1, 0 });
	vec3 deck = CARRIER_DECKHALF;
	if (length(CarrierAlong) > 0 && fabsf(dot(offset, CarrierAlong)) <= deck.x && fabsf(dot(offset, starboard) - CARRIER_DECKOFFSET) <= deck.z) return CARRIER_DECK;
	return 0.0f;
}

// Whether (x, z) is on the land or the runway rather than the sea: inside one
// of the ground's polygons. Each is convex, so a point is inside one when no
// two of its edges turn opposite ways around it.
static bool OverLand(float x, float z)
{
	for (int i = 0; i < GroundCount; i++) {
		const GroundPolygon &ground = Ground[i];
		bool negative = false, positive = false;
		for (int j = 0; j < ground.corners; j++) {
			vec3 a = ground.corner[j], b = ground.corner[(j + 1) % ground.corners];
			float turn = (b.x - a.x) * (z - a.z) - (b.z - a.z) * (x - a.x);
			if (turn < 0) negative = true;
			if (turn > 0) positive = true;
		}
		if (!(negative && positive)) return true;
	}
	return false;
}

// *******************************************************************
// The landmarks
// *******************************************************************

// The shade of a material a face turned toward normal takes
static unsigned char MaterialShade(int material, vec3 normal)
{
	float light = MAX(dot(normal, Light), 0.0f);
	return MATERIAL_FIRST + material * MATERIAL_SHADES + (int)(light * (MATERIAL_SHADES - 1) + 0.5f);
}

// A flat face, turned toward its outward normal
static void AddFace(const vec3 *corner, int corners, vec3 normal, int material)
{
	WorldFace &face = Faces[FaceCount++];
	for (int i = 0; i < corners; i++) {
		face.corner[i] = corner[i];
	}
	face.corners = corners;
	face.normal = normal;
	face.color = MaterialShade(material, normal);
}

static void AddLine(vec3 a, vec3 b, int material)
{
	Lines[LineCount++] = { a, b, (unsigned char)(MATERIAL_FIRST + material * MATERIAL_SHADES + MATERIAL_SHADES * 3 / 4) };
}

//
// A box about center, half its size along a level unit direction, up and
// across it
//
// Corner i is on the far side of each axis whose bit is set: 1 along, 2 up and
// 4 across. Each face is the four corners that agree on one bit, in order
// around it.
//
static void AddBox(vec3 center, vec3 along, vec3 half, int material)
{
	vec3 up = { 0, 1, 0 };
	vec3 across = cross(along, up);
	vec3 corner[8];
	for (int i = 0; i < 8; i++) {
		corner[i] = center + along * (i & 1 ? half.x : -half.x) + up * (i & 2 ? half.y : -half.y) + across * (i & 4 ? half.z : -half.z);
	}

	static const int Sides[6][4] = { { 0, 2, 6, 4 }, { 1, 3, 7, 5 }, { 0, 1, 5, 4 }, { 2, 3, 7, 6 }, { 0, 1, 3, 2 }, { 4, 5, 7, 6 } };
	Solid &solid = Solids[SolidCount++];
	solid.planes = 0;
	for (const int *side : Sides) {
		vec3 face[4] = { corner[side[0]], corner[side[1]], corner[side[2]], corner[side[3]] };
		vec3 normal = normalize(cross(face[1] - face[0], face[2] - face[0]));
		if (dot(normal, face[0] - center) < 0) normal = -normal;
		AddFace(face, 4, normal, material);
		solid.normal[solid.planes] = normal;
		solid.distance[solid.planes++] = dot(normal, face[0]);
	}
}

// A level deck at a height over the line from a to b
static void AddDeck(vec3 a, vec3 b, float height, float width, int material)
{
	a.y = b.y = height;
	vec3 along = normalize(b - a);
	AddBox((a + b) * 0.5f, along, { distance(a, b) / 2, 8.0f, width / 2 }, material);
}

//
// A suspension bridge between two anchorages: a deck, two towers of two legs
// tied by three beams, and a cable down each side, straight from each
// anchorage up to the nearer tower top, and hanging in a parabola between the
// tops to just above the deck
//
static void AddSuspensionBridge(vec3 a, vec3 b, float mainspan, float deckheight, float towerheight, float width, int material)
{
	AddDeck(a, b, deckheight, width, material);

	vec3 up = { 0, 1, 0 };
	vec3 along = normalize(b - a);
	vec3 across = cross(along, up);
	float length = distance(a, b);
	vec3 tower[2] = { a + along * ((length - mainspan) / 2), a + along * ((length + mainspan) / 2) };
	static const float Beams[3] = { 0.45f, 0.7f, 0.97f };
	for (vec3 base : tower) {
		for (int side = -1; side <= 1; side += 2) {
			AddBox(base + across * (side * width / 2) + up * (towerheight / 2), along, { 15, towerheight / 2, 15 }, material);
		}
		for (float beam : Beams) {
			AddBox(base + up * (towerheight * beam), along, { 10, 12, width / 2 + 15 }, material);
		}
	}

	for (int side = -1; side <= 1; side += 2) {
		vec3 offset = across * (side * width / 2);
		vec3 top[2] = { tower[0] + offset + up * towerheight, tower[1] + offset + up * towerheight };
		AddLine(a + offset + up * deckheight, top[0], material);
		AddLine(top[1], b + offset + up * deckheight, material);
		vec3 previous = top[0];
		for (int i = 1; i <= CABLE_SEGMENTS; i++) {
			float s = (float)i / CABLE_SEGMENTS;
			float sag = (2 * s - 1) * (2 * s - 1);
			vec3 point = mix(top[0], top[1], s);
			point.y = mix(deckheight + 20, towerheight, sag);
			AddLine(previous, point, material);
			previous = point;
		}
	}
}

// Which way a heading faces, in degrees clockwise from north
static vec3 HeadingDirection(float heading)
{
	float angle = heading * (float)M_PI / 180.0f;
	return { sinf(angle), 0, -cosf(angle) };
}

// A ship at sea level, its bow on a heading
static void AddShip(vec3 at, float heading)
{
	vec3 along = HeadingDirection(heading);
	AddBox(at + vec3{ 0, 15, 0 }, along, { 220, 15, 25 }, MATERIAL_HULL);
	AddBox(at + along * 20 + vec3{ 0, 48, 0 }, along, { 60, 18, 15 }, MATERIAL_HULL);
	AddBox(at - along * 40 + vec3{ 0, 55, 0 }, along, { 10, 25, 10 }, MATERIAL_STEEL);
	Contacts[ContactCount++] = at;
}

// The carrier at sea level, its bow on a heading: a hull, a flight deck
// overhanging to starboard, and the island with its mast
static void AddCarrier(vec3 at, float heading)
{
	vec3 along = HeadingDirection(heading);
	vec3 starboard = cross(along, vec3{ 0, 1, 0 });
	Carrier = at;
	CarrierAlong = along;
	AddBox(Carrier + vec3{ 0, 30, 0 }, along, { 546, 30, 67 }, MATERIAL_HULL);
	AddBox(Carrier + starboard * CARRIER_DECKOFFSET + vec3{ 0, CARRIER_DECK - CARRIER_DECKHALF.y, 0 }, along, CARRIER_DECKHALF, MATERIAL_DECK);
	AddBox(Carrier + starboard * 100 - along * 80 + vec3{ 0, CARRIER_DECK + 40, 0 }, along, { 75, 40, 15 }, MATERIAL_HULL);
	AddBox(Carrier + starboard * 100 - along * 80 + vec3{ 0, CARRIER_DECK + 110, 0 }, along, { 6, 30, 6 }, MATERIAL_STEEL);
	Contacts[ContactCount++] = Carrier;
}

// The materials a map's texture names, in the order of their palette ramps
static const char *const MaterialNames[MATERIALS] = { "hull", "deck", "orange", "steel", "concrete", "stone", "jet", "canopy" };

static int NamedMaterial(const char *name)
{
	for (int i = 0; i < MATERIALS; i++) {
		if (name && strcmp(name, MaterialNames[i]) == 0) return i;
	}
	RETRO_RageQuit("The map names a material that is not known: %s\n", name ? name : "(none)");
	return 0;
}

// The color a texture paints the ground in, or -1 for one that is not ground
static int GroundColor(const char *texture)
{
	if (strcmp(texture, "land") == 0) return COLOR_LAND;
	if (strcmp(texture, "runway") == 0) return COLOR_RUNWAY;
	if (strcmp(texture, "marking") == 0) return COLOR_MARKING;
	return -1;
}

//
// A brush of the map's worldspawn
//
// One textured as ground is painted as its top face. Any other is a solid,
// each face lit once in the material its texture names.
//
static void AddBrush(const RETRO_MapBrush &brush)
{
	vec3 corner[FACE_MAXCORNERS];
	int color = GroundColor(brush.plane[0].texture);
	if (color >= 0) {
		for (int i = 0; i < brush.planes; i++) {
			if (brush.plane[i].normal.z < 0.99f) continue;
			int corners = RETRO_MapBrushFace(brush, i, corner);
			for (int j = 0; j < corners; j++) {
				corner[j] = FromMap(corner[j]);
			}
			if (corners >= 3) AddGround(corner, corners, (unsigned char)color);
		}
		return;
	}

	Solid &solid = Solids[SolidCount++];
	solid.planes = brush.planes;
	for (int i = 0; i < brush.planes; i++) {
		const RETRO_MapPlane &plane = brush.plane[i];
		vec3 normal = FromMap(plane.normal);
		solid.normal[i] = normal;
		solid.distance[i] = plane.distance;
		int corners = RETRO_MapBrushFace(brush, i, corner);
		for (int j = 0; j < corners; j++) {
			corner[j] = FromMap(corner[j]);
		}
		if (corners >= 3) AddFace(corner, corners, normal, NamedMaterial(plane.texture));
	}
}

//
// The world, from the map
//
// The worldspawn's brushes in order, then each point entity built by its
// classname. A map's angle is counterclockwise from east, a heading here
// clockwise from north.
//
static void BuildWorld(void)
{
	RETRO_Map *map = RETRO_LoadMap("assets/flightsim.map");
	for (int i = 0; i < map->entities; i++) {
		const RETRO_MapEntity &entity = map->entity[i];
		const char *classname = RETRO_MapValue(entity, "classname");
		vec3 origin = FromMap(RETRO_MapVector(entity, "origin"));
		float heading = 90.0f - RETRO_MapNumber(entity, "angle");
		if (classname == NULL) continue;

		if (strcmp(classname, "worldspawn") == 0) {
			for (int j = 0; j < entity.brushes; j++) {
				AddBrush(map->brush[entity.firstbrush + j]);
			}
		} else if (strcmp(classname, "suspension_bridge") == 0) {
			const char *target = RETRO_MapValue(entity, "target");
			const RETRO_MapEntity *end = target ? RETRO_FindMapEntity(map, "targetname", target) : NULL;
			if (end == NULL) RETRO_RageQuit("A suspension bridge in the map has no other end\n");
			AddSuspensionBridge(origin, FromMap(RETRO_MapVector(*end, "origin")), RETRO_MapNumber(entity, "mainspan"), RETRO_MapNumber(entity, "deckheight"),
				RETRO_MapNumber(entity, "towerheight"), RETRO_MapNumber(entity, "width"), NamedMaterial(RETRO_MapValue(entity, "material")));
		} else if (strcmp(classname, "carrier") == 0) {
			AddCarrier(origin, heading);
		} else if (strcmp(classname, "ship") == 0) {
			AddShip(origin, heading);
		} else if (strcmp(classname, "info_player_start") == 0) {
			StartPosition = origin;
			StartHeading = heading;
		}
	}
	RETRO_FreeMap(map);
}

// *******************************************************************
// The flight
// *******************************************************************

// The whole frame turned about one axis. With the frame's own axes, positive
// angles are nose up about right, right wing down about forward and nose left
// about up.
static void TurnPlane(vec3 axis, float angle)
{
	mat3 turn = rotate(angle, axis);
	Plane.right = turn * Plane.right;
	Plane.up = turn * Plane.up;
	Plane.forward = turn * Plane.forward;
}

// Parked where the map starts a flight, facing its heading, with the engine
// idle and the gear down
static void StartFlight(void)
{
	Plane.forward = HeadingDirection(StartHeading);
	Plane.up = { 0, 1, 0 };
	Plane.right = cross(Plane.forward, Plane.up);
	Plane.position = StartPosition;
	Plane.position.y = GroundHeight(StartPosition.x, StartPosition.z) + FLIGHT_EYEHEIGHT;
	Plane.speed = 0.0f;
	Plane.throttle = 0.0f;
	Plane.fuel = FUEL_FULL;
	Plane.load = 1.0f;
	Plane.brake = false;
	Plane.gear = true;
	Plane.wreck = 0.0f;
	Plane.velocity = Plane.forward * (Plane.speed * KNOT);
}

// Whether a point is inside a solid, or on its surface
static bool InsideSolid(const Solid &solid, vec3 p)
{
	for (int i = 0; i < solid.planes; i++) {
		if (dot(solid.normal[i], p) > solid.distance[i]) return false;
	}
	return true;
}

//
// The load factor is the acceleration felt along up, in g: the change in
// velocity, plus the one g of standing still, so level flight reads 1.0 and
// inverted flight -1.0, and a banked turn or a pull reads more. Held on the
// ground, the sink the ground stops is no acceleration.
//
static void UpdateFlight(float timestep)
{
	if (timestep <= 0) return;
	vec3 worldup = { 0, 1, 0 };

	// A crash holds still until it has been seen, then the flight starts again
	if (Plane.wreck > 0) {
		Plane.wreck -= timestep;
		if (Plane.wreck <= 0) StartFlight();
		return;
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_B)) Plane.brake = !Plane.brake;
	if (RETRO_KeyPressed(SDL_SCANCODE_G)) Plane.gear = !Plane.gear;

	// The stick pitches the nose as fast as the wings can pull it round at the
	// speed, the load over the speed, but no faster than full stick moves it.
	// The wings push less hard than they pull. Below the stall speed they pull
	// no harder.
	float airspeed = MAX(Plane.speed, FLIGHT_STALLSPEED);
	float pullrate = MIN(FLIGHT_PITCHRATE, FLIGHT_PULLLOAD * FLIGHT_GRAVITY / airspeed);
	float pushrate = MIN(FLIGHT_PITCHRATE, FLIGHT_PUSHLOAD * FLIGHT_GRAVITY / airspeed);
	if (RETRO_KeyState(SDL_SCANCODE_UP)) TurnPlane(Plane.right, -pushrate * timestep);
	if (RETRO_KeyState(SDL_SCANCODE_DOWN)) TurnPlane(Plane.right, pullrate * timestep);
	if (RETRO_KeyState(SDL_SCANCODE_LEFT)) TurnPlane(Plane.forward, -FLIGHT_ROLLRATE * timestep);
	if (RETRO_KeyState(SDL_SCANCODE_RIGHT)) TurnPlane(Plane.forward, FLIGHT_ROLLRATE * timestep);
	if (RETRO_KeyState(SDL_SCANCODE_A)) TurnPlane(Plane.up, FLIGHT_RUDDERRATE * timestep);
	if (RETRO_KeyState(SDL_SCANCODE_D)) TurnPlane(Plane.up, -FLIGHT_RUDDERRATE * timestep);
	if (RETRO_KeyState(SDL_SCANCODE_W)) Plane.throttle += FLIGHT_THROTTLERATE * timestep;
	if (RETRO_KeyState(SDL_SCANCODE_S)) Plane.throttle -= FLIGHT_THROTTLERATE * timestep;
	Plane.throttle = clamp(Plane.throttle, 0.0f, 100.0f);

	// Banked right, the right wing points down and right.y is negative, and
	// -right.y of the wings' pull is sideways. A sideways acceleration over the
	// speed is the rate of turn. A positive turn about world up swings the nose
	// left, so the bank turns the heading by right.y itself.
	TurnPlane(worldup, FLIGHT_TURNLOAD * FLIGHT_GRAVITY * Plane.right.y / airspeed * timestep);

	// The turns are exact, but rounding still creeps in over thousands of them
	Plane.forward = normalize(Plane.forward);
	Plane.right = normalize(cross(Plane.forward, Plane.up));
	Plane.up = cross(Plane.right, Plane.forward);

	// The engine draws the speed toward the throttle while there is fuel
	float thrust = Plane.fuel > 0 ? Plane.throttle * FLIGHT_KNOTSPERPERCENT : 0.0f;
	Plane.speed = mix(Plane.speed, thrust, 1.0f - expf(-timestep / FLIGHT_SPOOL));
	Plane.speed -= Plane.forward.y * FLIGHT_GRAVITY * timestep;
	if (Plane.brake) Plane.speed -= BRAKE_DRAG * timestep;
	if (Plane.gear) Plane.speed -= GEAR_DRAG * timestep;
	Plane.speed = MAX(Plane.speed, 0.0f);
	Plane.fuel = MAX(Plane.fuel - (FUEL_IDLE + Plane.throttle * FUEL_PERPERCENT) * timestep, 0.0f);

	// Level wings lose nothing, vertical ones FLIGHT_SINK, inverted ones twice it
	float sink = (1.0f - Plane.up.y) * FLIGHT_SINK;
	if (Plane.speed < FLIGHT_STALLSPEED) sink += (1.0f - Plane.speed / FLIGHT_STALLSPEED) * FLIGHT_STALLSINK;

	vec3 velocity = Plane.forward * (Plane.speed * KNOT) - worldup * sink;
	Plane.position += velocity * timestep;
	Plane.position.y = MIN(Plane.position.y, FLIGHT_CEILING);

	// On the ground, which takes the gear, and the deck or land under it: the
	// sea is a crash, and so is a bank past what the wing tips clear. The nose
	// is turned about the horizontal axis across it, cross(forward, up), which
	// lifts it toward the sky whichever way up the aircraft is, by exactly the
	// angle it was pointing down. Then the wings are rolled level about the
	// nose: a turn of a about forward takes right to right cos a - up sin a,
	// level where tan a is right.y over up.y, and with up toward the sky.
	bool crashed = false;
	float ground = GroundHeight(Plane.position.x, Plane.position.z);
	float floor = ground + FLIGHT_EYEHEIGHT;
	if (Plane.position.y < floor) {
		bool deck = ground > 0;
		float bank = atan2f(Plane.right.y, Plane.up.y);
		crashed = !Plane.gear || fabsf(bank) > FLIGHT_GROUNDBANK || (!deck && !OverLand(Plane.position.x, Plane.position.z));
		Plane.position.y = floor;
		velocity.y = MAX(velocity.y, 0.0f);
		vec3 across = cross(Plane.forward, worldup);
		if (Plane.forward.y < 0 && length(across) > 0) {
			TurnPlane(normalize(across), -asinf(Plane.forward.y));
		}
		TurnPlane(Plane.forward, atan2f(Plane.right.y, Plane.up.y));
	}

	vec3 acceleration = (velocity - Plane.velocity) * (1.0f / timestep);
	float load = dot(acceleration + worldup * GRAVITY, Plane.up) / GRAVITY;
	Plane.load = mix(Plane.load, load, 1.0f - expf(-timestep / FLIGHT_LOADLAG));
	Plane.velocity = velocity;

	for (int i = 0; i < SolidCount && !crashed; i++) {
		crashed = InsideSolid(Solids[i], Plane.position);
	}
	if (crashed) {
		Plane.wreck = CRASH_PAUSE;
		Plane.speed = 0;
		Plane.velocity = {};
	}
}

static bool Airborne(void)
{
	return Plane.position.y > GroundHeight(Plane.position.x, Plane.position.z) + FLIGHT_EYEHEIGHT + 1.0f;
}

// The heading in degrees, clockwise from north, which is decreasing z
static float Heading(void)
{
	return mod(atan2f(Plane.forward.x, -Plane.forward.z) * 180.0f / (float)M_PI, 360.0f);
}

// *******************************************************************
// The view
// *******************************************************************

//
// The view, and the part of the screen it fills
//
// From the cockpit it is the aircraft's frame, with its up turned into the
// camera's down, above the panel. The chase view stands behind the aircraft
// on its heading and above it, and looks at it, its right kept level; climbing
// or diving straight up or down there is no heading, and the last one holds.
//
static void PlaceCamera(void)
{
	static vec3 ahead = { -1, 0, 0 };
	vec3 worldup = { 0, 1, 0 };
	if (!ChaseView) {
		Camera.pos = Plane.position;
		Camera.right = Plane.right;
		Camera.down = -Plane.up;
		Camera.forward = Plane.forward;
		Camera.lens.center = { RETRO_WIDTH / 2.0f, VIEW_EYELEVEL };
		Camera.lens.view = { 0, RETRO_WIDTH, 0, VIEW_BOTTOM };
		return;
	}

	vec3 level = { Plane.forward.x, 0, Plane.forward.z };
	if (length(level) > 0.01f) ahead = normalize(level);
	Camera.pos = Plane.position - ahead * CHASE_DISTANCE + worldup * CHASE_HEIGHT;
	Camera.forward = normalize(Plane.position - Camera.pos);
	Camera.right = normalize(cross(Camera.forward, worldup));
	Camera.down = cross(Camera.forward, Camera.right);
	Camera.lens.center = { RETRO_WIDTH / 2.0f, RETRO_HEIGHT / 2.0f };
	Camera.lens.view = { 0, RETRO_WIDTH, 0, RETRO_HEIGHT };
}

// A world polygon onto the screen, through the lens
static int ProjectPolygon(const vec3 *corner, int corners, PolygonPoint *point)
{
	RETRO_CameraVertex vertex[FACE_MAXCORNERS] = {};
	for (int i = 0; i < corners; i++) {
		vertex[i].eye = RETRO_ViewPoint(&Camera, corner[i]);
	}
	return RETRO_ClipProjectViewPolygon(Camera.lens, vertex, corners, point);
}

// The ground in order, each polygon a step nearer than the one before
static void DrawGround(ClipRect clip)
{
	for (int i = 0; i < GroundCount; i++) {
		const GroundPolygon &ground = Ground[i];
		PolygonPoint polygon[VIEW_MAXPOLYGON];
		int points = ProjectPolygon(ground.corner, ground.corners, polygon);
		for (int j = 0; j < points; j++) {
			polygon[j].q = i + 2.0f;
		}
		if (points >= 3) RETRO_DrawFlatPolygon(polygon, points, ground.color, clip);
	}
}

static void DrawFace(const WorldFace &face, ClipRect clip)
{
	if (dot(face.normal, Plane.position - face.corner[0]) <= 0) return;

	PolygonPoint polygon[VIEW_MAXPOLYGON];
	int points = ProjectPolygon(face.corner, face.corners, polygon);
	if (points >= 3) RETRO_DrawFlatPolygon(polygon, points, face.color, clip);
}

// A direction of the aircraft's own, in the world. The model's z runs toward
// the tail.
static vec3 PlaneDirection(vec3 model)
{
	return Plane.right * model.x + Plane.up * model.y - Plane.forward * model.z;
}

// The aircraft, lit as it is turned now
static void DrawJet(ClipRect clip)
{
	for (int i = 0; i < Jet->faces; i++) {
		const Face &face = Jet->face[i];
		if (face.material == JetGear && !Plane.gear) continue;

		vec3 corner[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face.vertices; j++) {
			corner[j] = Plane.position + PlaneDirection(Jet->vertex[face.vertex[j]].pos);
		}
		vec3 normal = PlaneDirection(face.facenormal.dir);
		if (dot(normal, Camera.pos - corner[0]) <= 0) continue;

		int material = face.material == JetCanopy ? MATERIAL_CANOPY : face.material == JetGear ? MATERIAL_DECK : MATERIAL_JET;
		PolygonPoint polygon[VIEW_MAXPOLYGON];
		int points = ProjectPolygon(corner, face.vertices, polygon);
		if (points >= 3) RETRO_DrawFlatPolygon(polygon, points, MaterialShade(material, normal), clip);
	}
}

// *******************************************************************
// The cockpit
// *******************************************************************

// Text whose cells end at right
static void TextRight(const char *text, int right, int y, unsigned char color)
{
	RETRO_PutString(text, right - RETRO_StringWidth(text), y, color);
}

// The nose ahead of the panel, curving down to each side, in bands of gray
// below its top edge, with the probe on it
static int CowlingTop(int x)
{
	float u = (x + 0.5f - RETRO_WIDTH / 2.0f) / (RETRO_WIDTH / 2.0f);
	return 134 + (int)(12.0f * u * u);
}

static void DrawCowling(void)
{
	for (int x = 0; x < RETRO_WIDTH; x++) {
		int top = CowlingTop(x);
		for (int y = top; y < VIEW_BOTTOM; y++) {
			int depth = y - top;
			RETRO_PutPixel(x, y, depth < 1 ? GRAY(7) : depth < 4 ? GRAY(6) : GRAY(5));
		}
	}
	RETRO_DrawVline(HUD_CENTER, 129, 133, GRAY(6));
	RETRO_DrawHline(HUD_CENTER - 1, HUD_CENTER + 1, 133, GRAY(6));
}

//
// The HUD
//
// The ladder is drawn on the world, as in flightsim2.cpp: each rung is the
// direction straight ahead across the ground at its elevation, projected, and
// lies along the ground's right as it lands on the screen. Every rung is the
// same pair of short dashes, the horizon among them.
//
static void DrawLadder(void)
{
	vec3 worldup = { 0, 1, 0 };
	vec3 ahead = { Plane.forward.x, 0, Plane.forward.z };
	if (length(ahead) < 0.01f) return;
	ahead = normalize(ahead);
	vec3 across = cross(ahead, worldup);

	// The ground's right on the screen, which y grows down
	vec2 along = normalize(vec2{ dot(across, Plane.right), -dot(across, Plane.up) });
	RETRO_Rectangle clip = { HUD_LEFT + 1, HUD_RIGHT, HUD_LADDERTOP, HUD_BOTTOM };
	for (int degrees = -90 + HUD_LADDERSTEP; degrees < 90; degrees += HUD_LADDERSTEP) {
		float elevation = degrees * (float)M_PI / 180.0f;
		vec3 eye = RETRO_ViewDirection(&Camera, ahead * cosf(elevation) + worldup * sinf(elevation));
		if (eye.z <= 0.1f) continue;
		vec2 center = RETRO_ProjectViewPoint(Camera.lens, eye).pos;
		for (int side = -1; side <= 1; side += 2) {
			vec2 a = center + along * (float)(side * HUD_RUNGIN);
			vec2 b = center + along * (float)(side * HUD_RUNGOUT);
			RETRO_DrawLine((int)floorf(a.x), (int)floorf(a.y), (int)floorf(b.x), (int)floorf(b.y), GRAY(8), clip);
		}
	}
}

// A dot every five degrees, every ten numbered in tens, over a tick at the
// heading
static void DrawHeadingTape(void)
{
	float heading = Heading();
	float half = HUD_TAPEHALF / HUD_TAPESCALE;
	for (int degrees = (int)ceilf((heading - half) / 5.0f) * 5; degrees <= heading + half; degrees += 5) {
		int x = HUD_CENTER + (int)roundf((degrees - heading) * HUD_TAPESCALE);
		RETRO_PutPixel(x, HUD_TAPEY, GRAY(8));
		int compass = WRAP(degrees, 360);
		if (compass % 10 != 0) continue;

		char label[4];
		snprintf(label, sizeof(label), "%02d", compass / 10);
		RETRO_PutString(label, x - 4, HUD_TAPEY - 8, GRAY(8));
	}
	RETRO_DrawVline(HUD_CENTER, HUD_TAPEY + 2, HUD_TAPEY + 4, GRAY(8));
}

static void DrawHud(void)
{
	DrawLadder();
	DrawHeadingTape();

	int y = (int)VIEW_EYELEVEL;
	RETRO_DrawHline(HUD_CENTER - 2, HUD_CENTER + 2, y, GRAY(8));
	RETRO_DrawVline(HUD_CENTER, y - 2, y + 2, GRAY(8));

	char text[16];
	snprintf(text, sizeof(text), "%d", (int)roundf(Plane.speed));
	TextRight(text, 111, 84, GRAY(8));
	TextRight("KT", 111, 92, GRAY(8));
	snprintf(text, sizeof(text), "%d", (int)roundf(Plane.position.y - FLIGHT_EYEHEIGHT));
	TextRight(text, 231, 84, GRAY(8));
	TextRight("FT", 231, 92, GRAY(8));
	snprintf(text, sizeof(text), "%.1f G", Plane.load);
	RETRO_PutString(text, 98, 121, GRAY(8));
}

// The posts the HUD glass stands on, and the bar across their tops
static void DrawHudFrame(void)
{
	RETRO_DrawLine(HUD_LEFT, HUD_POSTTOP, HUD_LEFT + HUD_BARINSET, HUD_BAR, GRAY(7));
	RETRO_DrawHline(HUD_LEFT + HUD_BARINSET, HUD_RIGHT - HUD_BARINSET, HUD_BAR, GRAY(7));
	RETRO_DrawLine(HUD_RIGHT - HUD_BARINSET, HUD_BAR, HUD_RIGHT, HUD_POSTTOP, GRAY(7));
	RETRO_DrawVline(HUD_LEFT, HUD_POSTTOP, PANEL_TOP - 1, COLOR_BLACK);
	RETRO_DrawVline(HUD_RIGHT, HUD_POSTTOP, PANEL_TOP - 1, COLOR_BLACK);
}

//
// The panel
//

// Its top edge: level across the middle, and a quarter circle down into each
// side
static int PanelTop(int x)
{
	int inset = MIN(x, RETRO_WIDTH - 1 - x);
	if (inset >= PANEL_CORNER) return PANEL_TOP;
	float d = PANEL_CORNER - (inset + 0.5f);
	return PANEL_TOP + PANEL_CORNER - (int)sqrtf(PANEL_CORNER * PANEL_CORNER - d * d);
}

// A raised plate: lit along the top and left, shadowed along the bottom and
// right
static void Plate(int x1, int y1, int x2, int y2, unsigned char fill)
{
	RETRO_DrawRectangle(x1, y1, x2, y2, fill);
	RETRO_DrawHline(x1, x2, y1, GRAY(7));
	RETRO_DrawVline(x1, y1, y2, GRAY(7));
	RETRO_DrawHline(x1, x2, y2, GRAY(1));
	RETRO_DrawVline(x2, y1, y2, GRAY(1));
}

// A display: a plate with a row of buttons along each edge and a black
// screen inside
static void Display(int x1, int y1, int x2, int y2)
{
	Plate(x1, y1, x2, y2, GRAY(3));
	for (int x = x1 + 8; x + 2 < x2 - 6; x += 8) {
		RETRO_DrawRectangle(x, y1 + 2, x + 2, y1 + 3, GRAY(6));
		RETRO_DrawRectangle(x, y2 - 3, x + 2, y2 - 2, GRAY(6));
	}
	for (int y = y1 + 8; y + 2 < y2 - 6; y += 7) {
		RETRO_DrawRectangle(x1 + 2, y, x1 + 3, y + 2, GRAY(6));
		RETRO_DrawRectangle(x2 - 3, y, x2 - 2, y + 2, GRAY(6));
	}
	RETRO_DrawRectangle(x1 + 6, y1 + 6, x2 - 6, y2 - 6, COLOR_BLACK);
}

static void DrawButtons(void)
{
	Plate(7, 156, 34, 165, GRAY(2));
	RETRO_PutString("EJECT", 9, 158, COLOR_TEXT);
	Plate(7, 168, 34, 177, GRAY(2));
	RETRO_PutString("BRAKE", 9, 170, Plane.brake ? COLOR_TEXT : GRAY(9));
	Plate(7, 180, 34, 196, GRAY(2));
	RETRO_PutString("GEAR", 11, 182, Plane.gear ? COLOR_TEXT : GRAY(9));
	RETRO_DrawHline(13, 29, 193, GRAY(9));
	RETRO_DrawVline(21, 190, 193, GRAY(9));
	RETRO_DrawRectangle(19, 189, 23, 190, GRAY(9));
}

// The aircraft seen from ahead with what hangs under its wings
static void DrawStores(void)
{
	Display(41, 148, 96, 198);
	RETRO_DrawHline(62, 75, 161, COLOR_TEXT);
	RETRO_DrawVline(62, 161, 166, COLOR_TEXT);
	RETRO_DrawVline(75, 161, 166, COLOR_TEXT);
	RETRO_DrawLine(62, 166, 52, 180, COLOR_TEXT);
	RETRO_DrawLine(75, 166, 85, 180, COLOR_TEXT);
	RETRO_DrawVline(57, 173, 179, COLOR_TEXT);
	RETRO_DrawVline(80, 173, 179, COLOR_TEXT);
	RETRO_DrawVline(52, 180, 187, COLOR_TEXT);
	RETRO_DrawVline(85, 180, 187, COLOR_TEXT);
	RETRO_DrawHline(50, 52, 187, COLOR_TEXT);
	RETRO_DrawHline(85, 87, 187, COLOR_TEXT);
}

static void DrawFuel(void)
{
	RETRO_PutString("FUEL", 103, 150, GRAY(9));
	Plate(105, 157, 120, 182, GRAY(3));
	RETRO_DrawRectangle(107, 159, 118, 180, COLOR_BLACK);
	for (int y = 160; y < 180; y += 3) {
		RETRO_PutPixel(108, y, GRAY(6));
		RETRO_PutPixel(117, y, GRAY(6));
	}
	int level = (int)roundf(Plane.fuel / FUEL_FULL * 20);
	if (level > 0) RETRO_DrawRectangle(110, 180 - level + 1, 115, 180, COLOR_GAUGE);

	char text[8];
	snprintf(text, sizeof(text), "%5d", (int)Plane.fuel);
	Plate(100, 183, 126, 191, GRAY(1));
	RETRO_PutString(text, 102, 185, GRAY(11));
}

//
// The radar, heading up: the aircraft at the meeting of the lines that bound
// what the nose can see, ten miles to the top of the screen, and a blip for
// every ship
//
static void DrawRadar(void)
{
	Plate(129, 144, 188, 191, GRAY(5));
	RETRO_DrawRectangle(131, 146, 186, 189, COLOR_BLACK);
	RETRO_DrawLine(139, 148, RADAR_X - 2, RADAR_Y - 1, GRAY(5));
	RETRO_DrawLine(178, 148, RADAR_X + 3, RADAR_Y - 1, GRAY(5));
	RETRO_PutPixel(RADAR_X, RADAR_Y + 1, GRAY(9));
	RETRO_DrawHline(RADAR_X - 1, RADAR_X + 1, RADAR_Y + 2, GRAY(9));
	char range[8];
	snprintf(range, sizeof(range), "%d MI", RadarRanges[RadarRange]);
	RETRO_PutString(range, RADAR_X - RETRO_StringWidth(range) / 2, 182, COLOR_TEXT);
	float scale = RADAR_TOP / (RadarRanges[RadarRange] * MILE);

	vec3 ahead = { Plane.forward.x, 0, Plane.forward.z };
	if (length(ahead) < 0.01f) return;
	ahead = normalize(ahead);
	vec3 right = cross(ahead, vec3{ 0, 1, 0 });
	for (int i = 0; i < ContactCount; i++) {
		vec3 offset = Contacts[i] - Plane.position;
		int x = RADAR_X + (int)floorf(dot(offset, right) * scale);
		int y = RADAR_Y - (int)floorf(dot(offset, ahead) * scale);
		if (x >= 131 && x < 186 && y >= 146 && y <= 189) {
			RETRO_DrawHline(x, x + 1, y, COLOR_BALLSKY);
		}
	}
}

// The strip of compass points, turning under a red mark at the heading
static void DrawCompass(void)
{
	static const char Points[4][2] = { "N", "E", "S", "W" };
	float heading = Heading();
	Plate(191, 148, 228, 158, GRAY(1));
	for (int degrees = 0; degrees < 360; degrees += 45) {
		float offset = mod(degrees - heading + 180.0f, 360.0f) - 180.0f;
		int x = COMPASS_X + (int)roundf(offset * COMPASS_SCALE);
		if (degrees % 90 != 0) {
			RETRO_PutPixel(x, 153, GRAY(8));
		} else if (x - 2 >= 193 && x + 1 <= 226) {
			RETRO_PutString(Points[degrees / 90], x - 2, 150, GRAY(10));
		}
	}
	RETRO_DrawVline(COMPASS_X, 149, 157, COLOR_RED);
}

//
// The attitude ball: the view's horizon again, as RETRO_ViewClimb finds it, through a
// wider lens, sky above and ground below, with a white line along it a pixel
// wide and the aircraft fixed at the middle. climb grows by steep for each
// pixel straight across the line.
//
static void DrawAttitude(void)
{
	Plate(193, 159, 225, 187, GRAY(3));
	float slope = BALL_SPREAD / BALL_RADIUS;
	float steep = slope * hypotf(Plane.right.y, Plane.up.y);
	for (int dy = -BALL_RADIUS; dy <= BALL_RADIUS; dy++) {
		for (int dx = -BALL_RADIUS; dx <= BALL_RADIUS; dx++) {
			if (dx * dx + dy * dy > BALL_RADIUS * BALL_RADIUS) continue;
			float climb = Plane.forward.y + (Plane.right.y * dx - Plane.up.y * dy) * slope;
			unsigned char color = fabsf(climb) < steep * 0.5f ? GRAY(14) : climb > 0 ? COLOR_BALLSKY : COLOR_BALLGROUND;
			RETRO_PutPixel(BALL_X + dx, BALL_Y + dy, color);
		}
	}
	RETRO_DrawHline(BALL_X - 6, BALL_X - 3, BALL_Y, GRAY(15));
	RETRO_DrawHline(BALL_X + 3, BALL_X + 6, BALL_Y, GRAY(15));
	RETRO_PutPixel(BALL_X, BALL_Y, GRAY(15));
}

static void DrawReadout(void)
{
	Display(231, 148, 286, 198);
	char text[16];
	snprintf(text, sizeof(text), "%d FT", (int)roundf(Plane.position.y - FLIGHT_EYEHEIGHT));
	TextRight(text, 282, 156, COLOR_TEXT);
	snprintf(text, sizeof(text), "%d KTS", (int)roundf(Plane.speed));
	TextRight(text, 282, 165, COLOR_TEXT);
	snprintf(text, sizeof(text), "%d%%THRST", (int)roundf(Plane.throttle));
	TextRight(text, 282, 174, COLOR_TEXT);
	TextRight("1.0 XMAG", 282, 183, COLOR_TEXT);
}

// Latitude and longitude, back from the map's feet
static void DrawPosition(void)
{
	char text[16];
	Plate(287, 178, 318, 195, COLOR_POSITION);
	snprintf(text, sizeof(text), "%.1fN", ORIGIN_LATITUDE - Plane.position.z / FEET_PER_DEGREE_LATITUDE);
	TextRight(text, 318, 180, GRAY(14));
	snprintf(text, sizeof(text), "%.1fW", ORIGIN_LONGITUDE - Plane.position.x / FEET_PER_DEGREE_LONGITUDE);
	TextRight(text, 318, 188, GRAY(14));
}

// What the panel warns of, if anything
static const char *Message(void)
{
	if (Plane.wreck > 0) return "CRASH";
	if (Plane.speed < FLIGHT_STALLSPEED && Airborne()) return "STALL";
	return NULL;
}

static void DrawPanel(void)
{
	for (int x = 0; x < RETRO_WIDTH; x++) {
		int top = PanelTop(x);
		RETRO_PutPixel(x, top, GRAY(6));
		RETRO_DrawVline(x, top + 1, RETRO_HEIGHT - 1, GRAY(1));
	}

	DrawButtons();
	DrawStores();
	DrawFuel();
	DrawRadar();
	DrawCompass();
	DrawAttitude();
	DrawReadout();
	DrawPosition();

	RETRO_DrawRectangle(291, 155, 298, 158, GRAY(3));
	RETRO_DrawRectangle(300, 155, 306, 158, GRAY(3));
	RETRO_DrawRectangle(291, 160, 306, 162, GRAY(3));
	Plate(291, 165, 314, 174, GRAY(3));
	RETRO_PutString("ECM", 296, 167, GRAY(11));

	Plate(100, 191, 217, 199, COLOR_BLACK);
	const char *message = Message();
	if (message) RETRO_PutString(message, 159 - RETRO_StringWidth(message) / 2, 193, COLOR_RED);
}

void DEMO_Render(RETRO_Time time)
{
	UpdateFlight(time.delta);
	if (RETRO_KeyPressed(SDL_SCANCODE_V)) ChaseView = !ChaseView;
	if (RETRO_KeyPressed(SDL_SCANCODE_R)) RadarRange = (RadarRange + 1) % (int)(sizeof(RadarRanges) / sizeof(RadarRanges[0]));
	if (RETRO_KeyPressed(SDL_SCANCODE_H)) ShowHud = !ShowHud;
	PlaceCamera();

	ClipRect clip = Camera.lens.view;
	RETRO_ClearDepthBuffer();
	RETRO_DrawHorizon(&Camera, { 0, 1, 0 }, COLOR_SKY, COLOR_SEA);
	DrawGround(clip);
	RETRO_ClearDepthBuffer();
	for (int i = 0; i < FaceCount; i++) {
		DrawFace(Faces[i], clip);
	}
	if (ChaseView) DrawJet(clip);
	for (int i = 0; i < LineCount; i++) {
		RETRO_DrawViewLine(&Camera, Lines[i].a, Lines[i].b, Lines[i].color, VIEW_LINENEARPLANE);
	}

	if (ChaseView) {
		const char *message = Message();
		if (message) RETRO_PutString(message, RETRO_WIDTH / 2 - RETRO_StringWidth(message) / 2, 8, COLOR_RED);
		return;
	}
	DrawCowling();
	if (ShowHud) DrawHud();
	DrawHudFrame();
	DrawPanel();
}

void DEMO_Initialize(void)
{
	RETRO_SetFont(RETRO_FONT_SMALL_5X6);

	RETRO_Palette palette[RETRO_COLORS] = {};
	palette[COLOR_SKY] = { 68, 68, 119 };
	palette[COLOR_SEA] = { 0, 51, 102 };
	palette[COLOR_LAND] = { 17, 85, 34 };
	palette[COLOR_RUNWAY] = { 34, 34, 34 };
	palette[COLOR_MARKING] = { 204, 204, 204 };
	palette[COLOR_RED] = { 204, 0, 0 };
	palette[COLOR_TEXT] = { 51, 170, 51 };
	palette[COLOR_GAUGE] = { 34, 136, 34 };
	palette[COLOR_BALLSKY] = { 102, 102, 204 };
	palette[COLOR_BALLGROUND] = { 170, 0, 0 };
	palette[COLOR_POSITION] = { 85, 85, 170 };
	for (int level = 1; level <= 15; level++) {
		unsigned char value = 17 * level;
		palette[GRAY(level)] = { value, value, value };
	}
	for (int material = 0; material < MATERIALS; material++) {
		for (int shade = 0; shade < MATERIAL_SHADES; shade++) {
			float brightness = mix(MATERIAL_AMBIENT, 1.0f, (float)shade / (MATERIAL_SHADES - 1));
			palette[MATERIAL_FIRST + material * MATERIAL_SHADES + shade] = RETRO_ShadeColor(MaterialColor[material], brightness);
		}
	}
	RETRO_SetPalette(palette);

	BuildWorld();

	Camera.lens.focalx = VIEW_FOCAL;
	Camera.lens.focaly = VIEW_FOCAL;
	Camera.lens.nearplane = VIEW_NEARPLANE;

	Jet = RETRO_Load3DModel("assets/fa18.obj");
	JetCanopy = RETRO_ModelMaterial(Jet, "canopy");
	JetGear = RETRO_ModelMaterial(Jet, "gear");
	StartFlight();
}
