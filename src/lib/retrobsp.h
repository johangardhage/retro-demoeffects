//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROBSP_H_
#define _RETROBSP_H_

#include "retro.h"
#include "retromap.h"
#include "retrovector.h"

#define BSP_VERSION			29
#define HEADER_LUMPS		15
#define MAX_MAP_HULLS		4
#define MAX_MAP_LEAFS		8192	// The most leaves Quake's engine holds
#define MIPLEVELS			4
#define MAXLIGHTMAPS		4
#define NUM_AMBIENTS		4

// Quake BSP version 29 lump order, from id Software's bspfile.h.
enum
{
	LUMP_ENTITIES = 0,
	LUMP_PLANES,
	LUMP_TEXTURES,
	LUMP_VERTEXES,
	LUMP_VISIBILITY,
	LUMP_NODES,
	LUMP_TEXINFO,
	LUMP_FACES,
	LUMP_LIGHTING,
	LUMP_CLIPNODES,
	LUMP_LEAFS,
	LUMP_MARKSURFACES,
	LUMP_EDGES,
	LUMP_SURFEDGES,
	LUMP_MODELS
};

// Values for dplane_t.type: the axis a plane is perpendicular to, or ANY* when not axis-aligned.
enum
{
	PLANE_X = 0,
	PLANE_Y,
	PLANE_Z,
	PLANE_ANYX,
	PLANE_ANYY,
	PLANE_ANYZ
};

// Values for dleaf_t.contents and clipnode children: what fills a region of space.
enum
{
	CONTENTS_EMPTY = -1,
	CONTENTS_SOLID = -2,
	CONTENTS_WATER = -3,
	CONTENTS_SLIME = -4,
	CONTENTS_LAVA = -5,
	CONTENTS_SKY = -6,
	CONTENTS_ORIGIN = -7,
	CONTENTS_CLIP = -8,
	CONTENTS_CURRENT_0 = -9,
	CONTENTS_CURRENT_90 = -10,
	CONTENTS_CURRENT_180 = -11,
	CONTENTS_CURRENT_270 = -12,
	CONTENTS_CURRENT_UP = -13,
	CONTENTS_CURRENT_DOWN = -14
};

#define TEX_SPECIAL 1	// texinfo_t.flags bit: sky or liquid surface, drawn without a lightmap

//
// On-disk BSP structures. These are laid out to match the file image
// byte-for-byte (little-endian); the static_assert checks at the end of this
// file lock each struct to the size required by the format.
//

// A lump is one section of the BSP file.
struct lump_t
{
	int fileofs;	// Offset to the lump, in bytes, from the start of the file
	int filelen;	// Length of the lump, in bytes
};

// BSP file header: version followed by the lump directory.
struct dheader_t
{
	int version;				// BSP version, must be BSP_VERSION (29) for Quake
	lump_t lumps[HEADER_LUMPS];	// Directory of every lump, indexed by LUMP_*
};

// A model: a self-contained group of faces. Model 0 is the world.
struct dmodel_t
{
	vec3 mins;						// Bounding box minimum
	vec3 maxs;						// Bounding box maximum
	vec3 origin;					// Model origin (zero for the world)
	int headnode[MAX_MAP_HULLS];	// Root node of each hull; headnode[0] is the render BSP
	int visleafs;					// Number of visible leaves, excluding the solid leaf 0
	int firstface;					// First face in the face lump
	int numfaces;					// Number of faces
};

// Header of the texture lump: a count followed by an offset to each texture.
struct dmiptexlump_t
{
	int nummiptex;	// Number of textures
	int dataofs[1];	// [nummiptex] offsets to each miptex_t, or -1 when the texture is missing
};

// A mip texture: name, dimensions and offsets to its four mip levels.
struct miptex_t
{
	char name[16];						// Name of the texture
	unsigned int width;					// Width in texels, a multiple of 8
	unsigned int height;				// Height in texels, a multiple of 8
	unsigned int offsets[MIPLEVELS];	// Offsets to the four mip levels, from the start of this miptex_t
};

// A single map vertex.
struct dvertex_t
{
	vec3 point;	// X,Y,Z position
};

// A splitting plane.
struct dplane_t
{
	vec3 normal;	// Unit normal (Nx^2 + Ny^2 + Nz^2 = 1)
	float dist;		// Distance from the origin along the normal
	int type;		// Axis alignment, PLANE_X..PLANE_ANYZ
};

// One texture axis: a texel coordinate is dot(axis, point) + offset.
struct texaxis_t
{
	vec3 axis;		// Direction, scaled to texels per world unit
	float offset;	// Texel offset
};

// Texture mapping info shared by faces.
struct texinfo_t
{
	texaxis_t vecs[2];	// S and T projection vectors
	int miptex;			// Index into the texture lump
	int flags;			// Surface flags (TEX_SPECIAL)
};

// A face: one drawable convex polygon.
struct dface_t
{
	short planenum;						// Plane the face lies on
	short side;							// 0 if the face faces along the plane normal, 1 if opposed
	int firstedge;						// First entry in the surfedge lump
	short numedges;						// Number of edges (and vertices) in the face
	short texinfo;						// Index into the texinfo lump
	unsigned char styles[MAXLIGHTMAPS];	// Light styles affecting the face, 0xFF ends the list
	int lightofs;						// Byte offset into the lighting lump, or -1 for no lightmap
};

// An internal BSP node.
struct dnode_t
{
	int planenum;				// Splitting plane, index into the plane lump
	short children[2];			// Front/back child: >= 0 is a node index, < 0 is leaf ~child
	short mins[3];				// Bounding box minimum, for culling
	short maxs[3];				// Bounding box maximum, for culling
	unsigned short firstface;	// First face in the face lump
	unsigned short numfaces;	// Number of faces
};

// A collision-hull node (not used for rendering).
struct dclipnode_t
{
	int planenum;		// Splitting plane
	short children[2];	// Front/back child: >= 0 is a node index, < 0 is a CONTENTS_* value
};

// An edge: a pair of vertex indices.
struct dedge_t
{
	unsigned short v[2];	// Start and end vertex indices
};

// A BSP leaf: a convex region of space.
struct dleaf_t
{
	int contents;								// CONTENTS_* describing what fills the leaf
	int visofs;									// Offset into the visibility lump, or -1 for no vis info
	short mins[3];								// Bounding box minimum, for culling
	short maxs[3];								// Bounding box maximum, for culling
	unsigned short firstmarksurface;			// First entry in the marksurface lump
	unsigned short nummarksurfaces;				// Number of marksurfaces (faces) in the leaf
	unsigned char ambient_level[NUM_AMBIENTS];	// Ambient sound volumes (0 = silent, 0xFF = max)
};

struct RETRO_BSP
{
	char *bsp;
	dheader_t *header;
	unsigned char *colormap;
	RETRO_Palette palette[256];
	RETRO_Map *entities;		// The entities lump, read as a .map without brushes

	// A lump directory entry by LUMP_* index
	lump_t *lump(int id) { return &header->lumps[id]; }

	// The number of faces
	int numfaces() { return lump(LUMP_FACES)->filelen / sizeof(dface_t); }

	// The number of textures
	int numtextures() { return ((dmiptexlump_t *)miptexlump())->nummiptex; }

	// The number of marksurface entries
	int nummarksurfaces() { return lump(LUMP_MARKSURFACES)->filelen / sizeof(unsigned short); }

	// The number of visible leaves
	int numleaves() { return model(0)->visleafs; }

	// One vertex
	dvertex_t *vertex(int id) { return &((dvertex_t *)&bsp[lump(LUMP_VERTEXES)->fileofs])[id]; }

	// One edge (holds a start and end vertex index)
	dedge_t *edge(int id) { return &((dedge_t *)&bsp[lump(LUMP_EDGES)->fileofs])[id]; }

	// One surfedge entry: a signed edge index, negative when the edge is reversed
	int surfedge(int id) { return ((int *)&bsp[lump(LUMP_SURFEDGES)->fileofs])[id]; }

	// The number of surfedge entries
	int numsurfedges() { return lump(LUMP_SURFEDGES)->filelen / sizeof(int); }

	// One plane
	dplane_t *plane(int id) { return &((dplane_t *)&bsp[lump(LUMP_PLANES)->fileofs])[id]; }

	// One face
	dface_t *face(int id) { return &((dface_t *)&bsp[lump(LUMP_FACES)->fileofs])[id]; }

	// One marksurface entry: a face index referenced by a leaf
	unsigned short marksurface(int id) { return ((unsigned short *)&bsp[lump(LUMP_MARKSURFACES)->fileofs])[id]; }

	// One model (model 0 is the world and the main render hull)
	dmodel_t *model(int id) { return &((dmodel_t *)&bsp[lump(LUMP_MODELS)->fileofs])[id]; }

	// One BSP node
	dnode_t *node(int id) { return &((dnode_t *)&bsp[lump(LUMP_NODES)->fileofs])[id]; }

	// The root node of the render BSP (model 0)
	dnode_t *rootnode() { return node(model(0)->headnode[0]); }

	// One BSP leaf
	dleaf_t *leaf(int id) { return &((dleaf_t *)&bsp[lump(LUMP_LEAFS)->fileofs])[id]; }

	// The visibility list (run-length encoded PVS) at a dleaf_t.visofs offset
	unsigned char *visibility(int offset) { return (unsigned char *)&bsp[lump(LUMP_VISIBILITY)->fileofs] + offset; }

	// The texture lump header (dmiptexlump_t)
	unsigned char *miptexlump() { return (unsigned char *)&bsp[lump(LUMP_TEXTURES)->fileofs]; }

	// One mip texture (NULL when the slot has no data, i.e. dataofs == -1 for a missing texture)
	miptex_t *miptexture(int id) {
		int offset = ((dmiptexlump_t *)miptexlump())->dataofs[id];
		return offset >= 0 ? (miptex_t *)(miptexlump() + offset) : NULL;
	}

	// The lightmap samples at an offset, or NULL when there is no lightmap
	unsigned char *lightmap(int offset) { return offset >= 0 ? (unsigned char *)&bsp[lump(LUMP_LIGHTING)->fileofs] + offset : NULL; }

	// One texinfo
	texinfo_t *textureinfo(int id) { return &((texinfo_t *)&bsp[lump(LUMP_TEXINFO)->fileofs])[id]; }
};

static_assert(sizeof(lump_t) == 8, "lump_t must match Quake BSP");
static_assert(sizeof(dheader_t) == 124, "dheader_t must match Quake BSP");
static_assert(sizeof(dmodel_t) == 64, "dmodel_t must match Quake BSP");
static_assert(sizeof(miptex_t) == 40, "miptex_t must match Quake BSP");
static_assert(sizeof(dvertex_t) == 12, "dvertex_t must match Quake BSP");
static_assert(sizeof(dplane_t) == 20, "dplane_t must match Quake BSP");
static_assert(sizeof(dnode_t) == 24, "dnode_t must match Quake BSP");
static_assert(sizeof(dclipnode_t) == 8, "dclipnode_t must match Quake BSP");
static_assert(sizeof(texaxis_t) == 16, "texaxis_t must match Quake BSP");
static_assert(sizeof(texinfo_t) == 40, "texinfo_t must match Quake BSP");
static_assert(sizeof(dface_t) == 20, "dface_t must match Quake BSP");
static_assert(sizeof(dedge_t) == 4, "dedge_t must match Quake BSP");
static_assert(sizeof(dleaf_t) == 28, "dleaf_t must match Quake BSP");

//
// Load the 256-entry palette: three bytes, red, green and blue, per color
//
inline void RETRO_LoadBSPPalette(RETRO_BSP *bsp, const char *filename)
{
	int size;
	unsigned char *data = RETRO_LoadFile(filename, &size);
	if (size < 256 * 3) {
		RETRO_RageQuit("Palette file is too small: %s\n", filename);
	}

	for (int i = 0; i < 256; i++) {
		bsp->palette[i] = { data[i * 3 + 0], data[i * 3 + 1], data[i * 3 + 2] };
	}

	free(data);
}

//
// Load the colormap: 64 shaded rows of 256 palette indices, used for lightmapping
//
inline void RETRO_LoadBSPColormap(RETRO_BSP *bsp, const char *filename)
{
	int size;
	bsp->colormap = RETRO_LoadFile(filename, &size);
	if (size < 256 * 64) {
		RETRO_RageQuit("Colormap file is too small: %s\n", filename);
	}
}

//
// Load the BSP file into memory and verify its version
//
inline void RETRO_LoadBSPMap(RETRO_BSP *bsp, const char *filename)
{
	bsp->bsp = (char *)RETRO_LoadFile(filename);
	bsp->header = (dheader_t *)bsp->bsp;
	if (bsp->header->version != BSP_VERSION) {
		RETRO_RageQuit("BSP file is not version %d: %s\n", BSP_VERSION, filename);
	}
	if (bsp->numleaves() > MAX_MAP_LEAFS) {
		RETRO_RageQuit("BSP file has more than %d leaves: %s\n", MAX_MAP_LEAFS, filename);
	}

	// The lump's text is copied out to give it a terminator of its own
	lump_t *lump = bsp->lump(LUMP_ENTITIES);
	char *text = (char *)malloc(lump->filelen + 1);
	memcpy(text, bsp->bsp + lump->fileofs, lump->filelen);
	text[lump->filelen] = 0;
	bsp->entities = RETRO_ParseMap(text, filename);
	free(text);
}

//
// Release BSP and colormap allocations
//
inline void RETRO_FreeBSP(RETRO_BSP *bsp)
{
	free(bsp->bsp);
	bsp->bsp = NULL;
	free(bsp->colormap);
	bsp->colormap = NULL;
	RETRO_FreeMap(bsp->entities);
	bsp->entities = NULL;
}

//
// Load the BSP, palette and colormap that the renderer needs
//
inline RETRO_BSP RETRO_LoadBSP(const char *bspfilename, const char *palettefilename, const char *colormapfilename)
{
	RETRO_BSP bsp;
	RETRO_LoadBSPMap(&bsp, bspfilename);
	RETRO_LoadBSPPalette(&bsp, palettefilename);
	RETRO_LoadBSPColormap(&bsp, colormapfilename);
	return bsp;
}

// *******************************************************************
// Visibility
// *******************************************************************

//
// The leaf a point is in. From the root, each node's plane sends the point to
// the front child when the point is in front of it, and to the back child
// otherwise; a negative child is a leaf, stored as its index's complement.
//
inline dleaf_t *RETRO_BSPFindLeaf(RETRO_BSP *bsp, vec3 point)
{
	dnode_t *node = bsp->rootnode();
	for (;;) {
		dplane_t *plane = bsp->plane(node->planenum);
		short child = dot(plane->normal, point) > plane->dist ? node->children[0] : node->children[1];
		if (child < 0) {
			return bsp->leaf(~child);
		}
		node = bsp->node(child);
	}
}

//
// The leaves a leaf may see, by index in increasing order, from its
// potentially visible set; returns how many. leaves must hold MAX_MAP_LEAFS
// entries, which the loader holds every map to.
//
// Leaves are numbered from 1, leaf 0 being the solid outside, and bit i - 1
// of the set is leaf i. The set is run-length encoded: a zero byte skips 8
// leaves for each count in the byte after it, and any other byte holds the
// next 8 bits, least significant first. A leaf without a set sees them all.
//
inline int RETRO_BSPVisibleLeaves(RETRO_BSP *bsp, dleaf_t *leaf, int *leaves)
{
	int numleaves = bsp->numleaves();
	int count = 0;

	if (leaf->visofs < 0) {
		for (int i = 1; i <= numleaves; i++) {
			leaves[count++] = i;
		}
		return count;
	}

	unsigned char *visibilitylist = bsp->visibility(leaf->visofs);
	for (int i = 1; i <= numleaves; ) {
		if (*visibilitylist == 0) {
			i += 8 * visibilitylist[1];
			visibilitylist += 2;
		} else {
			for (int bit = 1; bit < 256 && i <= numleaves; bit <<= 1, i++) {
				if (*visibilitylist & bit) {
					leaves[count++] = i;
				}
			}
			visibilitylist++;
		}
	}
	return count;
}

#endif
