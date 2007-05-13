/* Generated by ../../engine/scripts/makedmt.py */

#ifndef __DOOMSDAY_PLAY_MAP_DATA_TYPES_H__
#define __DOOMSDAY_PLAY_MAP_DATA_TYPES_H__

#include "p_mapdata.h"

typedef struct vertex_s {
    runtime_mapdata_header_t header;
    float               pos[2];
    unsigned int        numsecowners;  // Number of sector owners.
    unsigned int*       secowners;     // Sector indices [numsecowners] size.
    unsigned int        numlineowners; // Number of line owners.
    struct lineowner_s* lineowners;    // Lineowner base ptr [numlineowners] size. A doubly, circularly linked list. The base is the line with the lowest angle and the next-most with the largest angle.
    boolean             anchored;      // One or more of our line owners are one-sided.
} vertex_t;

// Helper macros for accessing seg data elements.
#define FRONT 0
#define BACK  1

#define SG_v(n)					v[(n)]
#define SG_v1                   SG_v(0)
#define SG_v2                   SG_v(1)
#define SG_sector(n)			sec[(n)]
#define SG_frontsector          SG_sector(FRONT)
#define SG_backsector           SG_sector(BACK)

// Seg flags
#define SEGF_POLYOBJ			0x1 // Seg is part of a poly object.

// Seg frame flags
#define SEGINF_FACINGFRONT      0x0001
#define SEGINF_BACKSECSKYFIX    0x0002

typedef struct seg_s {
    runtime_mapdata_header_t header;
    struct vertex_s*    v[2];          // [Start, End] of the segment.
    float               length;        // Accurate length of the segment (v1 -> v2).
    float               offset;
    struct side_s*      sidedef;
    struct line_s*      linedef;
    struct sector_s*    sec[2];
    angle_t             angle;
    byte                side;          // 0=front, 1=back
    byte                flags;
    short               frameflags;
    struct biastracker_s tracker[3];   // 0=middle, 1=top, 2=bottom
    struct vertexillum_s illum[3][4];
    unsigned int        updated;
    struct biasaffection_s affected[MAX_BIAS_AFFECTED];
} seg_t;

typedef struct subsector_s {
    runtime_mapdata_header_t header;
    struct sector_s*    sector;
    unsigned int        segcount;
    struct seg_s*       firstseg;
    struct polyobj_s*   poly;          // NULL, if there is no polyobj.
    byte                flags;
    unsigned short      numverts;
    fvertex_t*          verts;         // A sorted list of edge vertices.
    fvertex_t           bbox[2];       // Min and max points.
    fvertex_t           midpoint;      // Center of vertices.
    struct subplaneinfo_s** planes;
    unsigned short      numvertices;
    struct fvertex_s*   vertices;
    int                 validcount;
    struct shadowlink_s* shadows;
    unsigned int        group;
} subsector_t;

// Surface flags.
#define SUF_TEXFIX      0x1         // Current texture is a fix replacement
                                    // (not sent to clients, returned via DMU etc).
#define SUF_GLOW        0x2         // Surface glows (full bright).
#define SUF_BLEND       0x4         // Surface possibly has a blended texture.
#define SUF_NO_RADIO    0x8         // No fakeradio for this surface.

typedef struct surface_s {
    runtime_mapdata_header_t header;
    int                 flags;         // SUF_ flags
    int                 oldflags;
    short               texture;
    short               oldtexture;
    boolean             isflat;        // true if current texture is a flat
    boolean             oldisflat;
    float               normal[3];     // Surface normal
    float               oldnormal[3];
    float               texmove[2];    // Texture movement X and Y
    float               oldtexmove[2];
    float               offx;          // Texture x offset
    float               oldoffx;
    float               offy;          // Texture y offset
    float               oldoffy;
    float               rgba[4];       // Surface color tint
    float               oldrgba[4];
    struct translation_s* xlat;
} surface_t;

enum {
    PLN_FLOOR,
    PLN_CEILING,
    NUM_PLANE_TYPES
};

typedef struct skyfix_s {
    float offset;
} skyfix_t;

#define PS_normal				surface.normal
#define PS_texture				surface.texture
#define PS_isflat				surface.isflat
#define PS_offx					surface.offx
#define PS_offy					surface.offy
#define PS_texmove				surface.texmove

typedef struct plane_s {
    runtime_mapdata_header_t header;
    float               height;        // Current height
    float               oldheight[2];
    surface_t           surface;
    float               glow;          // Glow amount
    float               glowrgb[3];    // Glow color
    float               target;        // Target height
    float               speed;         // Move speed
    degenmobj_t         soundorg;      // Sound origin for plane
    struct sector_s*    sector;        // Owner of the plane (temp)
    float               visheight;     // Visible plane height (smoothed)
    float               visoffset;
} plane_t;

// Helper macros for accessing sector floor/ceiling plane data elements.
#define SP_plane(n)				planes[(n)]

#define SP_planesurface(n)      SP_plane(n)->surface
#define SP_planeheight(n)       SP_plane(n)->height
#define SP_planenormal(n)       SP_plane(n)->surface.normal
#define SP_planetexture(n)      SP_plane(n)->surface.texture
#define SP_planeisflat(n)       SP_plane(n)->surface.isflat
#define SP_planeoffx(n)         SP_plane(n)->surface.offx
#define SP_planeoffy(n)         SP_plane(n)->surface.offy
#define SP_planergb(n)          SP_plane(n)->surface.rgba
#define SP_planeglow(n)         SP_plane(n)->glow
#define SP_planeglowrgb(n)      SP_plane(n)->glowrgb
#define SP_planetarget(n)       SP_plane(n)->target
#define SP_planespeed(n)        SP_plane(n)->speed
#define SP_planetexmove(n)      SP_plane(n)->surface.texmove
#define SP_planesoundorg(n)     SP_plane(n)->soundorg
#define SP_planevisheight(n)	SP_plane(n)->visheight

#define SP_ceilsurface          SP_planesurface(PLN_CEILING)
#define SP_ceilheight           SP_planeheight(PLN_CEILING)
#define SP_ceilnormal           SP_planenormal(PLN_CEILING)
#define SP_ceiltexture          SP_planetexture(PLN_CEILING)
#define SP_ceilisflat           SP_planeisflat(PLN_CEILING)
#define SP_ceiloffx             SP_planeoffx(PLN_CEILING)
#define SP_ceiloffy             SP_planeoffy(PLN_CEILING)
#define SP_ceilrgb              SP_planergb(PLN_CEILING)
#define SP_ceilglow             SP_planeglow(PLN_CEILING)
#define SP_ceilglowrgb          SP_planeglowrgb(PLN_CEILING)
#define SP_ceiltarget           SP_planetarget(PLN_CEILING)
#define SP_ceilspeed            SP_planespeed(PLN_CEILING)
#define SP_ceiltexmove          SP_planetexmove(PLN_CEILING)
#define SP_ceilsoundorg         SP_planesoundorg(PLN_CEILING)
#define SP_ceilvisheight        SP_planevisheight(PLN_CEILING)

#define SP_floorsurface         SP_planesurface(PLN_FLOOR)
#define SP_floorheight          SP_planeheight(PLN_FLOOR)
#define SP_floornormal          SP_planenormal(PLN_FLOOR)
#define SP_floortexture         SP_planetexture(PLN_FLOOR)
#define SP_floorisflat          SP_planeisflat(PLN_FLOOR)
#define SP_flooroffx            SP_planeoffx(PLN_FLOOR)
#define SP_flooroffy            SP_planeoffy(PLN_FLOOR)
#define SP_floorrgb             SP_planergb(PLN_FLOOR)
#define SP_floorglow            SP_planeglow(PLN_FLOOR)
#define SP_floorglowrgb         SP_planeglowrgb(PLN_FLOOR)
#define SP_floortarget          SP_planetarget(PLN_FLOOR)
#define SP_floorspeed           SP_planespeed(PLN_FLOOR)
#define SP_floortexmove         SP_planetexmove(PLN_FLOOR)
#define SP_floorsoundorg        SP_planesoundorg(PLN_FLOOR)
#define SP_floorvisheight       SP_planevisheight(PLN_FLOOR)

#define S_skyfix(n)				skyfix[(n)]
#define S_floorskyfix			S_skyfix(PLN_FLOOR)
#define S_ceilskyfix			S_skyfix(PLN_CEILING)

// Sector frame flags
#define SIF_VISIBLE         0x1		// Sector is visible on this frame.
#define SIF_FRAME_CLEAR     0x1		// Flags to clear before each frame.
#define SIF_LIGHT_CHANGED   0x2

// Sector flags.
#define SECF_INVIS_FLOOR    0x1
#define SECF_INVIS_CEILING  0x2

typedef struct ssecgroup_s {
	struct sector_s**   linked;     // [sector->planecount] size.
	                                // Plane attached to another sector.
} ssecgroup_t;

typedef struct sector_s {
    runtime_mapdata_header_t header;
    float               lightlevel;
    float               oldlightlevel;
    float               rgb[3];
    float               oldrgb[3];
    int                 validcount;    // if == validcount, already checked.
    struct mobj_s*      thinglist;     // List of mobjs in the sector.
    unsigned int        linecount;
    struct line_s**     Lines;         // [linecount] size.
    unsigned int        subscount;
    struct subsector_s** subsectors;   // [subscount] size.
    unsigned int        subsgroupcount;
    ssecgroup_t*        subsgroups;    // [subsgroupcount] size.
    skyfix_t            skyfix[2];     // floor, ceiling.
    degenmobj_t         soundorg;
    float               reverb[NUM_REVERB_DATA];
    int                 blockbox[4];   // Mapblock bounding box.
    unsigned int        planecount;
    struct plane_s**    planes;        // [planecount] size.
    struct sector_s*    containsector; // Sector that contains this (if any).
    boolean             permanentlink;
    boolean             unclosed;      // An unclosed sector (some sort of fancy hack).
    boolean             selfRefHack;   // A self-referencing hack sector which ISNT enclosed by the sector referenced. Bounding box for the sector.
    float               bounds[4];     // Bounding box for the sector
    int                 frameflags;
    int                 addspritecount; // frame number of last R_AddSprites
    struct sector_s*    lightsource;   // Main sky light source
    unsigned int        blockcount;    // Number of gridblocks in the sector.
    unsigned int        changedblockcount; // Number of blocks to mark changed.
    unsigned short*     blocks;        // Light grid block indices.
} sector_t;

// Parts of a wall segment.
typedef enum segsection_e {
    SEG_MIDDLE,
    SEG_TOP,
    SEG_BOTTOM
} segsection_t;

// Helper macros for accessing sidedef top/middle/bottom section data elements.
#define SW_surface(n)			sections[(n)]
#define SW_surfaceflags(n)      SW_surface(n).flags
#define SW_surfacetexture(n)    SW_surface(n).texture
#define SW_surfaceisflat(n)     SW_surface(n).isflat
#define SW_surfacenormal(n)     SW_surface(n).normal
#define SW_surfacetexmove(n)    SW_surface(n).texmove
#define SW_surfaceoffx(n)       SW_surface(n).offx
#define SW_surfaceoffy(n)       SW_surface(n).offy
#define SW_surfacergba(n)       SW_surface(n).rgba
#define SW_surfacetexlat(n)     SW_surface(n).xlat

#define SW_middlesurface        SW_surface(SEG_MIDDLE)
#define SW_middleflags          SW_surfaceflags(SEG_MIDDLE)
#define SW_middletexture        SW_surfacetexture(SEG_MIDDLE)
#define SW_middleisflat         SW_surfaceisflat(SEG_MIDDLE)
#define SW_middlenormal         SW_surfacenormal(SEG_MIDDLE)
#define SW_middletexmove        SW_surfacetexmove(SEG_MIDDLE)
#define SW_middleoffx           SW_surfaceoffx(SEG_MIDDLE)
#define SW_middleoffy           SW_surfaceoffy(SEG_MIDDLE)
#define SW_middlergba           SW_surfacergba(SEG_MIDDLE)
#define SW_middletexlat         SW_surfacetexlat(SEG_MIDDLE)

#define SW_topsurface           SW_surface(SEG_TOP)
#define SW_topflags             SW_surfaceflags(SEG_TOP)
#define SW_toptexture           SW_surfacetexture(SEG_TOP)
#define SW_topisflat            SW_surfaceisflat(SEG_TOP)
#define SW_topnormal            SW_surfacenormal(SEG_TOP)
#define SW_toptexmove           SW_surfacetexmove(SEG_TOP)
#define SW_topoffx              SW_surfaceoffx(SEG_TOP)
#define SW_topoffy              SW_surfaceoffy(SEG_TOP)
#define SW_toprgba              SW_surfacergba(SEG_TOP)
#define SW_toptexlat            SW_surfacetexlat(SEG_TOP)

#define SW_bottomsurface        SW_surface(SEG_BOTTOM)
#define SW_bottomflags          SW_surfaceflags(SEG_BOTTOM)
#define SW_bottomtexture        SW_surfacetexture(SEG_BOTTOM)
#define SW_bottomisflat         SW_surfaceisflat(SEG_BOTTOM)
#define SW_bottomnormal         SW_surfacenormal(SEG_BOTTOM)
#define SW_bottomtexmove        SW_surfacetexmove(SEG_BOTTOM)
#define SW_bottomoffx           SW_surfaceoffx(SEG_BOTTOM)
#define SW_bottomoffy           SW_surfaceoffy(SEG_BOTTOM)
#define SW_bottomrgba           SW_surfacergba(SEG_BOTTOM)
#define SW_bottomtexlat         SW_surfacetexlat(SEG_BOTTOM)

// Side frame flags
#define SIDEINF_TOPPVIS     0x0001
#define SIDEINF_MIDDLEPVIS  0x0002
#define SIDEINF_BOTTOMPVIS  0x0004

typedef struct side_s {
    runtime_mapdata_header_t header;
    surface_t           sections[3];
    blendmode_t         blendmode;
    struct sector_s*    sector;
    short               flags;
    short               frameflags;
} side_t;

// Helper macros for accessing linedef data elements.
#define L_v(n)					v[(n)]
#define L_v1                    L_v(0)
#define L_v2                    L_v(1)
#define L_vo(n)					vo[(n)]
#define L_vo1                   L_vo(0)
#define L_vo2                   L_vo(1)
#define L_side(n)  			    sides[(n)]
#define L_frontside             L_side(FRONT)
#define L_backside              L_side(BACK)
#define L_sector(n)             sides[(n)]->sector
#define L_frontsector           L_sector(FRONT)
#define L_backsector            L_sector(BACK)

typedef struct line_s {
    runtime_mapdata_header_t header;
    struct vertex_s*    v[2];
    short               flags;
    float               dx;
    float               dy;
    slopetype_t         slopetype;
    int                 validcount;
    struct side_s*      sides[2];
    fixed_t             bbox[4];
    struct lineowner_s* vo[2];         // Links to vertex line owner nodes [left, right]
    float               length;        // Accurate length
    binangle_t          angle;         // Calculated from front side's normal
    boolean             selfrefhackroot; // This line is the root of a self-referencing hack sector
    boolean             mapped[DDMAXPLAYERS]; // Whether the line has been mapped by each player yet.
} line_t;

typedef struct polyobj_s {
    runtime_mapdata_header_t header;
    unsigned int        numsegs;
    struct seg_s**      segs;
    int                 validcount;
    degenmobj_t         startSpot;
    angle_t             angle;
    int                 tag;           // reference tag assigned in HereticEd
    ddvertex_t*         originalPts;   // used as the base for the rotations
    ddvertex_t*         prevPts;       // use to restore the old point values
    fixed_t             bbox[4];
    fvertex_t           dest;
    int                 speed;         // Destination XY and speed.
    angle_t             destAngle;     // Destination angle.
    angle_t             angleSpeed;    // Rotation speed.
    boolean             crush;         // should the polyobj attempt to crush mobjs?
    int                 seqType;
    fixed_t             size;          // polyobj size (area of POLY_AREAUNIT == size of FRACUNIT)
    void*               specialdata;   // pointer a thinker, if the poly is moving
} polyobj_t;

typedef struct node_s {
    runtime_mapdata_header_t header;
    float               x;             // Partition line.
    float               y;             // Partition line.
    float               dx;            // Partition line.
    float               dy;            // Partition line.
    float               bbox[2][4];    // Bounding box for each child.
    unsigned int        children[2];   // If NF_SUBSECTOR it's a subsector.
} node_t;

#endif
