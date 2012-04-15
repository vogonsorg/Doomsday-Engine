/**\file p_objlink.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"
#include "de_defs.h"

#include "gridmap.h"

#define BLOCK_WIDTH                 (128)
#define BLOCK_HEIGHT                (128)

BEGIN_PROF_TIMERS()
  PROF_OBJLINK_SPREAD,
  PROF_OBJLINK_LINK
END_PROF_TIMERS()

typedef struct objlink_s {
    struct objlink_s* nextInBlock; /// Next in the same obj block, or NULL.
    struct objlink_s* nextUsed;
    struct objlink_s* next; /// Next in list of ALL objlinks.
    objtype_t type;
    void* obj;
} objlink_t;

typedef struct {
    objlink_t* head;
    /// Used to prevent repeated per-frame processing of a block.
    boolean  doneSpread;
} objlinkblock_t;

typedef struct {
    float origin[2]; /// Origin of the blockmap in world coordinates [x,y].
    Gridmap* gridmap;
} objlinkblockmap_t;

typedef struct {
    void* obj;
    objtype_t objType;
    vec3f_t objPos;
    float objRadius;
    float box[4];
} contactfinderparams_t;

typedef struct objcontact_s {
    struct objcontact_s* next; /// Next in the BSP leaf.
    struct objcontact_s* nextUsed; /// Next used contact.
    void* obj;
} objcontact_t;

typedef struct {
    objcontact_t* head[NUM_OBJ_TYPES];
} objcontactlist_t;

static void processSeg(HEdge* hedge, void* data);

static objlink_t* objlinks = NULL;
static objlink_t* objlinkFirst = NULL, *objlinkCursor = NULL;

// Each objlink type gets its own blockmap.
static objlinkblockmap_t blockmaps[NUM_OBJ_TYPES];

// List of unused and used contacts.
static objcontact_t* contFirst = NULL, *contCursor = NULL;

// List of contacts for each BSP leaf.
static objcontactlist_t* bspLeafContacts = NULL;

static __inline objlinkblockmap_t* chooseObjlinkBlockmap(objtype_t type)
{
    assert(VALID_OBJTYPE(type));
    return blockmaps + (int)type;
}

static __inline uint toObjlinkBlockmapX(objlinkblockmap_t* obm, float x)
{
    assert(obm && x >= obm->origin[0]);
    return (uint)((x - obm->origin[0]) / (float)BLOCK_WIDTH);
}

static __inline uint toObjlinkBlockmapY(objlinkblockmap_t* obm, float y)
{
    assert(obm && y >= obm->origin[1]);
    return (uint)((y - obm->origin[1]) / (float)BLOCK_HEIGHT);
}

/**
 * Given world coordinates @a x, @a y, determine the objlink blockmap block
 * [x, y] it resides in. If the coordinates are outside the blockmap they
 * are clipped within valid range.
 *
 * @return  @c true if the coordinates specified had to be adjusted.
 */
static boolean toObjlinkBlockmapCell(objlinkblockmap_t* obm, uint coords[2],
    float x, float y)
{
    boolean adjusted = false;
    float max[2];
    uint size[2];
    assert(obm);

    Gridmap_Size(obm->gridmap, size);
    max[0] = obm->origin[0] + size[0] * BLOCK_WIDTH;
    max[1] = obm->origin[1] + size[1] * BLOCK_HEIGHT;

    if(x < obm->origin[0])
    {
        coords[VX] = 0;
        adjusted = true;
    }
    else if(x >= max[0])
    {
        coords[VX] = size[0]-1;
        adjusted = true;
    }
    else
    {
        coords[VX] = toObjlinkBlockmapX(obm, x);
    }

    if(y < obm->origin[1])
    {
        coords[VY] = 0;
        adjusted = true;
    }
    else if(y >= max[1])
    {
        coords[VY] = size[1]-1;
        adjusted = true;
    }
    else
    {
        coords[VY] = toObjlinkBlockmapY(obm, y);
    }
    return adjusted;
}

static __inline void linkContact(objcontact_t* con, objcontact_t** list, uint index)
{
    con->next = list[index];
    list[index] = con;
}

static void linkContactToBspLeaf(objcontact_t* node, objtype_t type, uint index)
{
    linkContact(node, &bspLeafContacts[index].head[type], 0);
}

/**
 * Create a new objcontact. If there are none available in the list of
 * used objects a new one will be allocated and linked to the global list.
 */
static objcontact_t* allocObjContact(void)
{
    objcontact_t* con;
    if(!contCursor)
    {
        con = Z_Malloc(sizeof *con, PU_APPSTATIC, NULL);

        // Link to the list of objcontact nodes.
        con->nextUsed = contFirst;
        contFirst = con;
    }
    else
    {
        con = contCursor;
        contCursor = contCursor->nextUsed;
    }
    con->obj = NULL;
    return con;
}

static objlink_t* allocObjlink(void)
{
    objlink_t* link;
    if(!objlinkCursor)
    {
        link = Z_Malloc(sizeof *link, PU_APPSTATIC, NULL);

        // Link the link to the global list.
        link->nextUsed = objlinkFirst;
        objlinkFirst = link;
    }
    else
    {
        link = objlinkCursor;
        objlinkCursor = objlinkCursor->nextUsed;
    }
    link->nextInBlock = NULL;
    link->obj = NULL;

    // Link it to the list of in-use objlinks.
    link->next = objlinks;
    objlinks = link;

    return link;
}

void R_InitObjlinkBlockmapForMap(void)
{
    GameMap* map = theMap;
    float min[2], max[2];
    uint width, height;
    int i;

    // Determine the dimensions of the objlink blockmaps in blocks.
    GameMap_Bounds(map, &min[0], &max[0]);
    width  = (uint)ceil((max[0] - min[0]) / (float)BLOCK_WIDTH);
    height = (uint)ceil((max[1] - min[1]) / (float)BLOCK_HEIGHT);

    // Create the blockmaps.
    for(i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        objlinkblockmap_t* obm = chooseObjlinkBlockmap((objtype_t)i);
        obm->origin[0] = min[0];
        obm->origin[1] = min[1];
        obm->gridmap = Gridmap_New(width, height, sizeof(objlinkblock_t), PU_MAPSTATIC);
    }

    // Initialize obj => BspLeaf contact lists.
    bspLeafContacts = Z_Calloc(sizeof *bspLeafContacts * NUM_BSPLEAFS, PU_MAPSTATIC, 0);
}

void R_DestroyObjlinkBlockmap(void)
{
    int i;
    for(i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        objlinkblockmap_t* obm = chooseObjlinkBlockmap((objtype_t)i);
        if(!obm->gridmap) continue;
        Gridmap_Delete(obm->gridmap);
        obm->gridmap = NULL;
    }
    if(bspLeafContacts)
    {
        Z_Free(bspLeafContacts);
        bspLeafContacts = NULL;
    }
}

int clearObjlinkBlock(void* obj, void* paramaters)
{
    objlinkblock_t* block = (objlinkblock_t*)obj;
    block->head = NULL;
    block->doneSpread = false;
    return false; // Continue iteration.
}

void R_ClearObjlinkBlockmap(objtype_t type)
{
    if(!VALID_OBJTYPE(type))
    {
#if _DEBUG
        Con_Error("R_ClearObjlinkBlockmap: Attempted with invalid type %i.", (int)type);
#endif
        return;
    }
    // Clear all the contact list heads and spread flags.
    Gridmap_Iterate(chooseObjlinkBlockmap(type)->gridmap, clearObjlinkBlock);
}

void R_ClearObjlinksForFrame(void)
{
    int i;
    for(i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        objlinkblockmap_t* obm = chooseObjlinkBlockmap((objtype_t)i);
        if(!obm->gridmap) continue;
        R_ClearObjlinkBlockmap((objtype_t)i);
    }

    // Start reusing objlinks.
    objlinkCursor = objlinkFirst;
    objlinks = NULL;
}

void R_ObjlinkCreate(void* obj, objtype_t type)
{
    objlink_t* link = allocObjlink();
    link->obj = obj;
    link->type = type;
}

int RIT_LinkObjToBspLeaf(BspLeaf* bspLeaf, void* paramaters)
{
    const linkobjtobspleafparams_t* p = (linkobjtobspleafparams_t*) paramaters;
    objcontact_t* con = allocObjContact();

    con->obj = p->obj;
    // Link the contact list for this bspLeaf.
    linkContactToBspLeaf(con, p->type, GET_BSPLEAF_IDX(bspLeaf));

    return false; // Continue iteration.
}

/**
 * Attempt to spread the obj from the given contact from the source
 * BspLeaf and into the (relative) back BspLeaf.
 *
 * @param bspLeaf  BspLeaf to attempt to spread over to.
 * @param data  @see contactfinderparams_t
 *
 * @return  @c true (always - this function is also used as an iterator).
 */
static void spreadInBspLeaf(BspLeaf* bspLeaf, void* paramaters)
{
    HEdge* hedge;
    if(!bspLeaf || !bspLeaf->hedge) return;

    hedge = bspLeaf->hedge;
    do
    {
        processSeg(hedge, paramaters);
    } while((hedge = hedge->next) != bspLeaf->hedge);
}

static void processSeg(HEdge* hedge, void* paramaters)
{
    contactfinderparams_t* p = (contactfinderparams_t*) paramaters;
    linkobjtobspleafparams_t loParams;
    BspLeaf* source, *dest;
    float distance;
    Vertex* vtx;

    // HEdge must be between two different BspLeafs.
    if(hedge->lineDef && (!hedge->twin || hedge->bspLeaf == hedge->twin->bspLeaf))
        return;

    // Which way does the spread go?
    if(hedge->bspLeaf->validCount == validCount &&
       hedge->twin->bspLeaf->validCount != validCount)
    {
        source = hedge->bspLeaf;
        dest = hedge->twin->bspLeaf;
    }
    else
    {
        // Not eligible for spreading.
        return;
    }

    // Is the dest BspLeaf inside the objlink's AABB?
    if(dest->aaBox.maxX <= p->box[BOXLEFT] ||
       dest->aaBox.minX >= p->box[BOXRIGHT] ||
       dest->aaBox.maxY <= p->box[BOXBOTTOM] ||
       dest->aaBox.minY >= p->box[BOXTOP])
    {
        // The BspLeaf is not inside the params's bounds.
        return;
    }

    // Can the spread happen?
    if(hedge->lineDef)
    {
        if(dest->sector)
        {
            if(dest->sector->planes[PLN_CEILING]->height <= dest->sector->planes[PLN_FLOOR]->height ||
               dest->sector->planes[PLN_CEILING]->height <= source->sector->planes[PLN_FLOOR]->height ||
               dest->sector->planes[PLN_FLOOR]->height   >= source->sector->planes[PLN_CEILING]->height)
            {
                // No; destination sector is closed with no height.
                return;
            }
        }

        // Don't spread if the middle material completely fills the gap between
        // floor and ceiling (direction is from dest to source).
        if(LineDef_MiddleMaterialCoversOpening(hedge->lineDef,
            dest == hedge->twin->bspLeaf? false : true, false))
            return;
    }

    // Calculate 2D distance to hedge.
    {
    const float dx = hedge->HE_v2pos[VX] - hedge->HE_v1pos[VX];
    const float dy = hedge->HE_v2pos[VY] - hedge->HE_v1pos[VY];
    vtx = hedge->HE_v1;
    distance = ((vtx->pos[VY] - p->objPos[VY]) * dx -
                (vtx->pos[VX] - p->objPos[VX]) * dy) / hedge->length;
    }

    if(hedge->lineDef)
    {
        if((source == hedge->bspLeaf && distance < 0) ||
           (source == hedge->twin->bspLeaf && distance > 0))
        {
            // Can't spread in this direction.
            return;
        }
    }

    // Check distance against the obj radius.
    if(distance < 0)
        distance = -distance;
    if(distance >= p->objRadius)
    {   // The obj doesn't reach that far.
        return;
    }

    // During next step, obj will continue spreading from there.
    dest->validCount = validCount;

    // Add this obj to the destination BspLeaf.
    loParams.obj   = p->obj;
    loParams.type = p->objType;
    RIT_LinkObjToBspLeaf(dest, &loParams);

    spreadInBspLeaf(dest, paramaters);
}

/**
 * Create a contact for the objlink in all the BspLeafs the linked obj is
 * contacting (tests done on bounding boxes and the BSP leaf spread test).
 *
 * @param oLink Ptr to objlink to find BspLeaf contacts for.
 */
static void findContacts(objlink_t* link)
{
    contactfinderparams_t cfParams;
    linkobjtobspleafparams_t loParams;
    float radius;
    pvec3f_t pos;
    BspLeaf** ssecAdr;

    switch(link->type)
    {
    case OT_LUMOBJ: {
        lumobj_t* lum = (lumobj_t*) link->obj;
        // Only omni lights spread.
        if(lum->type != LT_OMNI) return;

        pos = lum->pos;
        radius = LUM_OMNI(lum)->radius;
        ssecAdr = &lum->bspLeaf;
        break;
      }
    case OT_MOBJ: {
        mobj_t* mo = (mobj_t*) link->obj;

        pos = mo->pos;
        radius = R_VisualRadius(mo);
        ssecAdr = &mo->bspLeaf;
        break;
      }
    default:
        Con_Error("findContacts: Invalid objtype %i.", (int) link->type);
        exit(1); // Unreachable.
    }

    // Do the BSP leaf spread. Begin from the obj's own BspLeaf.
    (*ssecAdr)->validCount = ++validCount;

    cfParams.obj = link->obj;
    cfParams.objType = link->type;
    V3f_Copy(cfParams.objPos, pos);
    // Use a slightly smaller radius than what the obj really is.
    cfParams.objRadius = radius * .98f;

    cfParams.box[BOXLEFT]   = cfParams.objPos[VX] - radius;
    cfParams.box[BOXRIGHT]  = cfParams.objPos[VX] + radius;
    cfParams.box[BOXBOTTOM] = cfParams.objPos[VY] - radius;
    cfParams.box[BOXTOP]    = cfParams.objPos[VY] + radius;

    // Always contact the obj's own BspLeaf.
    loParams.obj = link->obj;
    loParams.type = link->type;
    RIT_LinkObjToBspLeaf(*ssecAdr, &loParams);

    spreadInBspLeaf(*ssecAdr, &cfParams);
}

/**
 * Spread contacts in the object => BspLeaf objlink blockmap to all
 * other BspLeafs within the block.
 *
 * @param bspLeaf  BspLeaf to spread the contacts of.
 */
void R_ObjlinkBlockmapSpreadInBspLeaf(objlinkblockmap_t* obm,
    const BspLeaf* bspLeaf, float maxRadius)
{
    uint minBlock[2], maxBlock[2], x, y;
    objlink_t* iter;
    assert(obm);

    if(!bspLeaf) return; // Wha?

    toObjlinkBlockmapCell(obm, minBlock, bspLeaf->aaBox.minX - maxRadius,
                                         bspLeaf->aaBox.minY - maxRadius);

    toObjlinkBlockmapCell(obm, maxBlock, bspLeaf->aaBox.maxX + maxRadius,
                                         bspLeaf->aaBox.maxY + maxRadius);

    for(y = minBlock[1]; y <= maxBlock[1]; ++y)
        for(x = minBlock[0]; x <= maxBlock[0]; ++x)
        {
            objlinkblock_t* block = Gridmap_CellXY(obm->gridmap, x, y, true/*can allocate a block*/);
            if(block->doneSpread) continue;

            iter = block->head;
            while(iter)
            {
                findContacts(iter);
                iter = iter->nextInBlock;
            }
            block->doneSpread = true;
        }
}

static __inline const float maxRadius(objtype_t type)
{
    assert(VALID_OBJTYPE(type));
    if(type == OT_MOBJ) return DDMOBJ_RADIUS_MAX;
    // Must be OT_LUMOBJ
    return loMaxRadius;
}

void R_InitForBspLeaf(BspLeaf* bspLeaf)
{
    int i;
BEGIN_PROF( PROF_OBJLINK_SPREAD );

    for(i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        objlinkblockmap_t* obm = chooseObjlinkBlockmap((objtype_t)i);
        R_ObjlinkBlockmapSpreadInBspLeaf(obm, bspLeaf, maxRadius((objtype_t)i));
    }

END_PROF( PROF_OBJLINK_SPREAD );
}

/// @assume  Coordinates held by @a blockXY are within valid range.
static void linkObjlinkInBlockmap(objlinkblockmap_t* obm, objlink_t* link, uint blockXY[2])
{
    objlinkblock_t* block;
    if(!obm || !link || !blockXY) return; // Wha?
    block = Gridmap_CellXY(obm->gridmap, blockXY[0], blockXY[1], true/*can allocate a block*/);
    link->nextInBlock = block->head;
    block->head = link;
}

void R_LinkObjs(void)
{
    objlinkblockmap_t* obm;
    objlink_t* link;
    uint block[2];
    pvec3f_t pos;

BEGIN_PROF( PROF_OBJLINK_LINK );

    // Link objlinks into the objlink blockmap.
    link = objlinks;
    while(link)
    {
        switch(link->type)
        {
        case OT_LUMOBJ:     pos = ((lumobj_t*)link->obj)->pos; break;
        case OT_MOBJ:       pos = ((mobj_t*)link->obj)->pos; break;
        default:
            Con_Error("R_LinkObjs: Invalid objtype %i.", (int) link->type);
            exit(1); // Unreachable.
        }

        obm = chooseObjlinkBlockmap(link->type);
        if(!toObjlinkBlockmapCell(obm, block, pos[VX], pos[VY]))
        {
            linkObjlinkInBlockmap(obm, link, block);
        }
        link = link->next;
    }

END_PROF( PROF_OBJLINK_LINK );
}

void R_InitForNewFrame(void)
{
#ifdef DD_PROFILE
    static int i;

    if(++i > 40)
    {
        i = 0;
        PRINT_PROF(PROF_OBJLINK_SPREAD);
        PRINT_PROF(PROF_OBJLINK_LINK);
    }
#endif

    // Start reusing nodes from the first one in the list.
    contCursor = contFirst;
    if(bspLeafContacts)
        memset(bspLeafContacts, 0, NUM_BSPLEAFS * sizeof *bspLeafContacts);
}

int R_IterateBspLeafContacts2(BspLeaf* bspLeaf, objtype_t type,
    int (*callback) (void* object, void* paramaters), void* paramaters)
{
    objcontact_t* con = bspLeafContacts[GET_BSPLEAF_IDX(bspLeaf)].head[type];
    int result = false; // Continue iteration.
    while(con)
    {
        result = callback(con->obj, paramaters);
        if(result) break;
        con = con->next;
    }
    return result;
}

int R_IterateBspLeafContacts(BspLeaf* bspLeaf, objtype_t type,
    int (*callback) (void* object, void* paramaters))
{
    return R_IterateBspLeafContacts2(bspLeaf, type, callback, NULL/*no paramaters*/);
}
