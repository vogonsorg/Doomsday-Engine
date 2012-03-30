/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * edit_map.h: Runtime map building.
 */

#ifndef __DOOMSDAY_MAP_EDITOR_H__
#define __DOOMSDAY_MAP_EDITOR_H__

#include "p_mapdata.h"
#include "m_binarytree.h"
#include "materials.h"

#ifdef __cplusplus
extern "C" {
#endif

// Editable map.
typedef struct editmap_s {
    uint numVertexes;
    Vertex** vertexes;
    uint numLineDefs;
    LineDef** lineDefs;
    uint numSideDefs;
    SideDef** sideDefs;
    uint numSectors;
    Sector** sectors;
    uint numPolyObjs;
    Polyobj** polyObjs;

    // The following is for game-specific map object data.
    gameobjdata_t gameObjData;
} editmap_t;

//extern editmap_t editMap;

boolean         MPE_Begin(const char* mapUri);
boolean         MPE_End(void);

uint            MPE_VertexCreate(float x, float y);
boolean         MPE_VertexCreatev(size_t num, float *values, uint *indices);
uint            MPE_SidedefCreate(uint sector, short flags,
                                  materialid_t topMaterial,
                                  float topOffsetX, float topOffsetY, float topRed,
                                  float topGreen, float topBlue,
                                  materialid_t middleMaterial,
                                  float middleOffsetX, float middleOffsetY,
                                  float middleRed, float middleGreen,
                                  float middleBlue, float middleAlpha,
                                  materialid_t bottomMaterial,
                                  float bottomOffsetX, float bottomOffsetY,
                                  float bottomRed, float bottomGreen,
                                  float bottomBlue);
uint            MPE_LinedefCreate(uint v1, uint v2, uint frontSide, uint backSide,
                                  int flags);
uint            MPE_SectorCreate(float lightlevel, float red, float green, float blue);
uint            MPE_PlaneCreate(uint sector, float height,
                                materialid_t material,
                                float matOffsetX, float matOffsetY,
                                float r, float g, float b, float a,
                                float normalX, float normalY, float normalZ);
uint            MPE_PolyobjCreate(uint *lines, uint linecount,
                                  int tag, int sequenceType, float startX, float startY);

boolean         MPE_GameObjProperty(const char *objName, uint idx,
                                    const char *propName, valuetype_t type,
                                    void *data);

// Non-public (temporary)
// Flags for MPE_PruneRedundantMapData().
#define PRUNE_LINEDEFS      0x1
#define PRUNE_VERTEXES      0x2
#define PRUNE_SIDEDEFS      0x4
#define PRUNE_SECTORS       0x8
#define PRUNE_ALL           (PRUNE_LINEDEFS|PRUNE_VERTEXES|PRUNE_SIDEDEFS|PRUNE_SECTORS)

void            MPE_PruneRedundantMapData(editmap_t* map, int flags);

boolean         MPE_RegisterUnclosedSectorNear(Sector* sec, double x, double y);
void            MPE_PrintUnclosedSectorList(void);
void            MPE_FreeUnclosedSectorList(void);

GameMap*        MPE_GetLastBuiltMap(void);
Vertex*         createVertex(void);

#define ET_prev             link[0]
#define ET_next             link[1]
#define ET_edge             hedges

// An edge tip is where an edge meets a vertex.
typedef struct edgetip_s {
    // Link in list. List is kept in ANTI-clockwise order.
    struct edgetip_s* link[2]; // {prev, next};

    /// Angle that line makes at vertex (degrees; 0 is E, 90 is N).
    double angle;

    // Half-edge on each side of the edge. Left is the side of increasing
    // angles, right is the side of decreasing angles. Either can be NULL
    // for one sided edges.
    struct hedge_s* hedges[2];
} edgetip_t;

struct edgetip_s* MPE_NewEdgeTip(void);
void MPE_DeleteEdgeTip(struct edgetip_s* tip);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
