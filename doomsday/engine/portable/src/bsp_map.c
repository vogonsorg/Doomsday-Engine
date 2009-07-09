/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * bsp_map.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_bsp.h"
#include "de_misc.h"
#include "de_play.h"
#include "de_edit.h"
#include "de_refresh.h"

#include <stdlib.h>
#include <math.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void hardenSideSegList(gamemap_t* map, sidedef_t* side, hedge_t* seg,
                              const hedge_t* hEdge)
{
    uint                count;
    const hedge_t*      first, *other;

    if(!side)
        return;

    // Have we already processed this side?
    if(side->hEdges)
        return;

    // Find the first hedge for this side.
    first = hEdge;
    while(first->prev)
    {
        if(((bsp_hedgeinfo_t*) first->data)->sourceLine !=
           ((bsp_hedgeinfo_t*) hEdge->data)->sourceLine)
            break;

        first = first->prev;
    }

    // Count the hedges for this side.
    count = 0;
    other = first;
    while(other)
    {
        if(((bsp_hedgeinfo_t*) other->data)->sourceLine !=
           ((bsp_hedgeinfo_t*) hEdge->data)->sourceLine)
            break;

        other = other->next;
        count++;
    }

    if(count)
    {
        // Allocate the final side hedge table.
        side->hEdgeCount = count;
        side->hEdges = Z_Malloc(sizeof(hedge_t*) * (side->hEdgeCount+1),
            PU_MAPSTATIC, 0);

        count = 0;
        other = first;
        while(other)
        {
            if(((bsp_hedgeinfo_t*) other->data)->sourceLine !=
               ((bsp_hedgeinfo_t*) hEdge->data)->sourceLine)
                break;

            side->hEdges[count++] =
                &map->hEdges[((bsp_hedgeinfo_t*) other->data)->index];
            other = other->next;
        }
        side->hEdges[count] = NULL; // Terminate.
    }
    else
    {   // Should never happen.
        side->hEdgeCount = 0;
        side->hEdges = NULL;
    }
}

static int C_DECL hEdgeCompare(const void* p1, const void* p2)
{
    const bsp_hedgeinfo_t* a = (bsp_hedgeinfo_t*) (((const hedge_t**) p1)[0])->data;
    const bsp_hedgeinfo_t* b = (bsp_hedgeinfo_t*) (((const hedge_t**) p2)[0])->data;

    if(a->index == b->index)
        return 0;
    if(a->index < b->index)
        return -1;
    return 1;
}

typedef struct {
    size_t              curIdx;
    hedge_t***          indexPtr;
    boolean             write;
} hedgecollectorparams_t;

static boolean hEdgeCollector(binarytree_t* tree, void* data)
{
    if(BinaryTree_IsLeaf(tree))
    {
        hedgecollectorparams_t* params = (hedgecollectorparams_t*) data;
        bspleafdata_t*      leaf = (bspleafdata_t*) BinaryTree_GetData(tree);
        hedge_node_t*       n;

        for(n = leaf->hEdges; n; n = n->next)
        {
            hedge_t*            hEdge = n->hEdge;

            if(params->indexPtr)
            {   // Write mode.
                (*params->indexPtr)[params->curIdx++] = hEdge;
            }
            else
            {   // Count mode.
                if(((bsp_hedgeinfo_t*) hEdge->data)->index == -1)
                    Con_Error("HEdge %p never reached a subsector!", hEdge);

                params->curIdx++;
            }
        }
    }

    return true; // Continue traversal.
}

static void buildSegsFromHEdges(gamemap_t* map, binarytree_t* rootNode)
{
    uint                i;
    hedge_t**           index;
    seg_t*              storage;
    hedgecollectorparams_t params;

    //
    // First we need to build a sorted index of the used hedges.
    //

    // Pass 1: Count the number of used hedges.
    params.curIdx = 0;
    params.indexPtr = NULL;
    BinaryTree_InOrder(rootNode, hEdgeCollector, &params);

    if(!(params.curIdx > 0))
        Con_Error("buildSegsFromHEdges: No halfedges?");

    // Allocate the sort buffer.
    index = M_Malloc(sizeof(hedge_t*) * params.curIdx);

    // Pass 2: Collect ptrs the hedges and insert into the index.
    params.curIdx = 0;
    params.indexPtr = &index;
    BinaryTree_InOrder(rootNode, hEdgeCollector, &params);

    // Sort the half-edges into ascending index order.
    qsort(index, params.curIdx, sizeof(hedge_t*), hEdgeCompare);

    map->numHEdges = (uint) params.curIdx;
    map->hEdges =
        Z_Calloc(map->numHEdges * (sizeof(hedge_t)+sizeof(seg_t)),
        PU_MAPSTATIC, 0);
    storage = (seg_t*) (((byte*) map->hEdges) + (sizeof(hedge_t) * map->numHEdges));

    for(i = 0; i < map->numHEdges; ++i)
    {
        const hedge_t*      hEdge = index[i];
        hedge_t*            dst = &map->hEdges[i];

        dst->header.type = DMU_HEDGE;
        dst->data = storage++;

        dst->HE_v1 = &map->vertexes[hEdge->v[0]->buildData.index - 1];
        dst->HE_v2 = &map->vertexes[hEdge->v[1]->buildData.index - 1];

        if(hEdge->twin)
            dst->twin =
                &map->hEdges[((bsp_hedgeinfo_t*) hEdge->twin->data)->index];
        else
            dst->twin = NULL;
    }

    // Generate seg data from (BSP) line segments.
    for(i = 0; i < map->numHEdges; ++i)
    {
        hedge_t*            dst = &map->hEdges[i];
        seg_t*              seg = (seg_t* ) dst->data;
        const hedge_t*      hEdge = index[i];
        const bsp_hedgeinfo_t*   data = (bsp_hedgeinfo_t*) hEdge->data;

        seg->side  = data->side;
        if(data->lineDef)
            seg->lineDef = &map->lineDefs[data->lineDef->buildData.index - 1];

        seg->flags = 0;
        if(seg->lineDef)
        {
            linedef_t*          ldef = seg->lineDef;
            vertex_t*           vtx = seg->lineDef->L_v(seg->side);

            if(ldef->L_side(seg->side))
                seg->SG_frontsector = ldef->L_side(seg->side)->sector;

            if(ldef->L_frontside && ldef->L_backside)
            {
                seg->SG_backsector = ldef->L_side(seg->side ^ 1)->sector;
            }
            else
            {
                seg->SG_backsector = 0;
            }

            seg->offset = P_AccurateDistance(dst->HE_v1pos[VX] - vtx->V_pos[VX],
                                             dst->HE_v1pos[VY] - vtx->V_pos[VY]);
        }
        else
        {
            seg->lineDef = NULL;
            seg->SG_frontsector = NULL;
            seg->SG_backsector = NULL;
        }

        if(seg->lineDef)
            hardenSideSegList(map, HEDGE_SIDEDEF(dst), dst, hEdge);

        seg->angle =
            bamsAtan2((int) (dst->HE_v2pos[VY] - dst->HE_v1pos[VY]),
                      (int) (dst->HE_v2pos[VX] - dst->HE_v1pos[VX])) << FRACBITS;

        // Calculate the length of the segment. We need this for
        // the texture coordinates. -jk
        seg->length = P_AccurateDistance(dst->HE_v2pos[VX] - dst->HE_v1pos[VX],
                                         dst->HE_v2pos[VY] - dst->HE_v1pos[VY]);

        if(seg->length == 0)
            seg->length = 0.01f; // Hmm...

        // Calculate the surface normals
        // Front first
        if(seg->lineDef && HEDGE_SIDEDEF(dst))
        {
            sidedef_t*          side = HEDGE_SIDEDEF(dst);
            surface_t*          surface = &side->SW_topsurface;

            surface->normal[VY] = (dst->HE_v1pos[VX] - dst->HE_v2pos[VX]) / seg->length;
            surface->normal[VX] = (dst->HE_v2pos[VY] - dst->HE_v1pos[VY]) / seg->length;
            surface->normal[VZ] = 0;

            // All surfaces of a sidedef have the same normal.
            memcpy(side->SW_middlenormal, surface->normal, sizeof(surface->normal));
            memcpy(side->SW_bottomnormal, surface->normal, sizeof(surface->normal));
        }
    }

    // Free temporary storage
    M_Free(index);
}

static void hardenLeaf(gamemap_t* map, subsector_t* dest,
                       const bspleafdata_t* src)
{
    boolean             found;
    size_t              hEdgeCount;
    hedge_t*            hEdge;
    hedge_node_t*       n;

    dest->hEdge =
        &map->hEdges[((bsp_hedgeinfo_t*) src->hEdges->hEdge->data)->index];

    hEdgeCount = 0;
    for(n = src->hEdges; ; n = n->next)
    {
        const hedge_t*      hEdge = n->hEdge;

        hEdgeCount++;

        if(!n->next)
        {
            map->hEdges[((bsp_hedgeinfo_t*) hEdge->data)->index].next =
                dest->hEdge;
            break;
        }

        map->hEdges[((bsp_hedgeinfo_t*) hEdge->data)->index].next =
            &map->hEdges[((bsp_hedgeinfo_t*) n->next->hEdge->data)->index];
    }

    dest->header.type = DMU_SUBSECTOR;
    dest->hEdgeCount = (uint) hEdgeCount;
    dest->shadows = NULL;
    dest->vertices = NULL;

    // Determine which sector this subsector belongs to.
    found = false;
    hEdge = dest->hEdge;
    do
    {
        sidedef_t*          side;

        if(!found && (side = HEDGE_SIDEDEF(hEdge)))
        {
            dest->sector = side->sector;
            found = true;
        }

       ((seg_t*) hEdge->data)->subsector = dest;
    } while((hEdge = hEdge->next) != dest->hEdge);

    if(!dest->sector)
    {
        Con_Message("hardenLeaf: Warning orphan subsector %p.\n", dest);
    }
}

typedef struct {
    gamemap_t*      dest;
    uint            ssecCurIndex;
    uint            nodeCurIndex;
} hardenbspparams_t;

static boolean C_DECL hardenNode(binarytree_t* tree, void* data)
{
    binarytree_t*       right, *left;
    bspnodedata_t*      nodeData;
    hardenbspparams_t*  params;
    node_t*             node;

    if(BinaryTree_IsLeaf(tree))
        return true; // Continue iteration.

    nodeData = BinaryTree_GetData(tree);
    params = (hardenbspparams_t*) data;

    node = &params->dest->nodes[nodeData->index = params->nodeCurIndex++];
    node->header.type = DMU_NODE;

    node->partition.x = nodeData->partition.x;
    node->partition.y = nodeData->partition.y;
    node->partition.dX = nodeData->partition.dX;
    node->partition.dY = nodeData->partition.dY;

    node->bBox[RIGHT][BOXTOP]    = nodeData->bBox[RIGHT][BOXTOP];
    node->bBox[RIGHT][BOXBOTTOM] = nodeData->bBox[RIGHT][BOXBOTTOM];
    node->bBox[RIGHT][BOXLEFT]   = nodeData->bBox[RIGHT][BOXLEFT];
    node->bBox[RIGHT][BOXRIGHT]  = nodeData->bBox[RIGHT][BOXRIGHT];

    node->bBox[LEFT][BOXTOP]     = nodeData->bBox[LEFT][BOXTOP];
    node->bBox[LEFT][BOXBOTTOM]  = nodeData->bBox[LEFT][BOXBOTTOM];
    node->bBox[LEFT][BOXLEFT]    = nodeData->bBox[LEFT][BOXLEFT];
    node->bBox[LEFT][BOXRIGHT]   = nodeData->bBox[LEFT][BOXRIGHT];

    right = BinaryTree_GetChild(tree, RIGHT);
    if(right)
    {
        if(BinaryTree_IsLeaf(right))
        {
            bspleafdata_t*  leaf = (bspleafdata_t*) BinaryTree_GetData(right);
            uint            idx = params->ssecCurIndex++;

            node->children[RIGHT] = idx | NF_SUBSECTOR;
            hardenLeaf(params->dest, &params->dest->ssectors[idx], leaf);
        }
        else
        {
            bspnodedata_t* data = (bspnodedata_t*) BinaryTree_GetData(right);

            node->children[RIGHT] = data->index;
        }
    }

    left = BinaryTree_GetChild(tree, LEFT);
    if(left)
    {
        if(BinaryTree_IsLeaf(left))
        {
            bspleafdata_t*  leaf = (bspleafdata_t*) BinaryTree_GetData(left);
            uint            idx = params->ssecCurIndex++;

            node->children[LEFT] = idx | NF_SUBSECTOR;
            hardenLeaf(params->dest, &params->dest->ssectors[idx], leaf);
        }
        else
        {
            bspnodedata_t*  data = (bspnodedata_t*) BinaryTree_GetData(left);

            node->children[LEFT]  = data->index;
        }
    }

    return true; // Continue iteration.
}

static boolean C_DECL countNode(binarytree_t* tree, void* data)
{
    if(!BinaryTree_IsLeaf(tree))
        (*((uint*) data))++;

    return true; // Continue iteration.
}

static boolean C_DECL countSSec(binarytree_t* tree, void* data)
{
    if(BinaryTree_IsLeaf(tree))
        (*((uint*) data))++;

    return true; // Continue iteration.
}

static void hardenBSP(gamemap_t* dest, binarytree_t* rootNode)
{
    dest->numNodes = 0;
    BinaryTree_PostOrder(rootNode, countNode, &dest->numNodes);
    dest->nodes =
        Z_Calloc(dest->numNodes * sizeof(node_t), PU_MAPSTATIC, 0);

    dest->numSSectors = 0;
    BinaryTree_PostOrder(rootNode, countSSec, &dest->numSSectors);
    dest->ssectors =
        Z_Calloc(dest->numSSectors * sizeof(subsector_t), PU_MAPSTATIC, 0);

    if(rootNode)
    {
        hardenbspparams_t params;

        params.dest = dest;
        params.ssecCurIndex = 0;
        params.nodeCurIndex = 0;

        BinaryTree_PostOrder(rootNode, hardenNode, &params);
    }
}

void BSP_InitForNodeBuild(gamemap_t* map)
{
    uint                i;

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t*          l = &map->lineDefs[i];
        vertex_t*           start = l->v[0];
        vertex_t*           end   = l->v[1];

        start->buildData.refCount++;
        end->buildData.refCount++;

        l->buildData.mlFlags = 0;

        // Check for zero-length line.
        if((fabs(start->buildData.pos[VX] - end->buildData.pos[VX]) < DIST_EPSILON) &&
           (fabs(start->buildData.pos[VY] - end->buildData.pos[VY]) < DIST_EPSILON))
            l->buildData.mlFlags |= MLF_ZEROLENGTH;

        if(l->inFlags & LF_POLYOBJ)
            l->buildData.mlFlags |= MLF_POLYOBJ;

        if(l->sideDefs[BACK] && l->sideDefs[FRONT])
        {
            l->buildData.mlFlags |= MLF_TWOSIDED;

            if(l->sideDefs[BACK]->sector == l->sideDefs[FRONT]->sector)
                l->buildData.mlFlags |= MLF_SELFREF;
        }
    }
}

static void hardenVertexes(gamemap_t* dest, vertex_t*** vertexes,
                           uint* numVertexes)
{
    uint                i;

    dest->numVertexes = *numVertexes;
    dest->vertexes =
        Z_Calloc(dest->numVertexes * sizeof(vertex_t), PU_MAPSTATIC, 0);

    for(i = 0; i < dest->numVertexes; ++i)
    {
        vertex_t*           destV = &dest->vertexes[i];
        vertex_t*           srcV = (*vertexes)[i];

        destV->header.type = DMU_VERTEX;
        destV->numLineOwners = srcV->numLineOwners;
        destV->lineOwners = srcV->lineOwners;

        //// \fixme Add some rounding.
        destV->V_pos[VX] = (float) srcV->buildData.pos[VX];
        destV->V_pos[VY] = (float) srcV->buildData.pos[VY];
    }
}

static void updateVertexLinks(gamemap_t* dest)
{
    uint                i;

    for(i = 0; i < dest->numLineDefs; ++i)
    {
        linedef_t*          line = &dest->lineDefs[i];

        line->L_v1 = &dest->vertexes[line->L_v1->buildData.index - 1];
        line->L_v2 = &dest->vertexes[line->L_v2->buildData.index - 1];
    }
}

void SaveMap(gamemap_t* dest, void* rootNode, vertex_t*** vertexes,
             uint* numVertexes)
{
    uint                startTime = Sys_GetRealTime();
    binarytree_t*       rn = (binarytree_t*) rootNode;

    hardenVertexes(dest, vertexes, numVertexes);
    updateVertexLinks(dest);
    buildSegsFromHEdges(dest, rn);
    hardenBSP(dest, rn);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("SaveMap: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}
