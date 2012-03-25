/**
 * @file superblockmap.cpp
 * BSP Builder Superblock. @ingroup map
 *
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_platform.h"
#include "p_mapdata.h"

#include "bspbuilder/hedges.hh"
#include "bspbuilder/superblockmap.hh"

using namespace de;

void SuperBlock::clear()
{
    hedges.clear();
    KdTreeNode_SetUserData(tree, NULL);
}

//static int deleteSuperBlockIterator(SuperBlock* block, void* /*parameters*/);

static void initAABoxFromHEdgeVertexes(AABoxf* aaBox, const bsp_hedge_t* hedge)
{
    assert(aaBox && hedge);
    const double* from = hedge->v[0]->buildData.pos;
    const double* to   = hedge->v[1]->buildData.pos;
    aaBox->minX = (float)MIN_OF(from[0], to[0]);
    aaBox->minY = (float)MIN_OF(from[1], to[1]);
    aaBox->maxX = (float)MAX_OF(from[0], to[0]);
    aaBox->maxY = (float)MAX_OF(from[1], to[1]);
}

/// @todo Optimize: Cache this result.
void SuperBlock::findHEdgeBounds(AABoxf* bounds)
{
    if(!bounds) return;

    bool initialized = false;
    AABoxf hedgeAABox;

    for(HEdges::iterator it = hedges.begin(); it != hedges.end(); ++it)
    {
        bsp_hedge_t* hedge = *it;
        initAABoxFromHEdgeVertexes(&hedgeAABox, hedge);
        if(initialized)
        {
            V2_AddToBox(bounds->arvec2, hedgeAABox.min);
        }
        else
        {
            V2_InitBox(bounds->arvec2, hedgeAABox.min);
            initialized = true;
        }
        V2_AddToBox(bounds->arvec2, hedgeAABox.max);
    }
}

SuperBlock* SuperBlock::hedgePush(bsp_hedge_t* hedge)
{
    if(!hedge) return this;

    SuperBlock* sb = this;
    for(;;)
    {
        int p1, p2, half, midPoint[2];
        const AABox* bounds;

        // Update half-edge counts.
        sb->incrementHEdgeCount(hedge);

        if(sb->isLeaf())
        {
            // No further subdivision possible.
            sb->linkHEdge(hedge);
            break;
        }

        bounds = sb->bounds();
        midPoint[0] = (bounds->minX + bounds->maxX) / 2;
        midPoint[1] = (bounds->minY + bounds->maxY) / 2;

        if(bounds->maxX - bounds->minX >=
           bounds->maxY - bounds->minY)
        {
            // Wider than tall.
            p1 = hedge->v[0]->buildData.pos[0] >= midPoint[0];
            p2 = hedge->v[1]->buildData.pos[0] >= midPoint[0];
        }
        else
        {
            // Taller than wide.
            p1 = hedge->v[0]->buildData.pos[1] >= midPoint[1];
            p2 = hedge->v[1]->buildData.pos[1] >= midPoint[1];
        }

        if(p1 && p2)
        {
            half = 1;
        }
        else if(!p1 && !p2)
        {
            half = 0;
        }
        else
        {
            // Line crosses midpoint -- link it in and return.
            sb->linkHEdge(hedge);
            break;
        }

        // The hedge lies in one half of this block. Create the sub-block
        // if it doesn't already exist, and loop back to add the hedge.
        if(!KdTreeNode_Child(sb->tree, half))
        {
            SuperBlock* child;
            AABox sub;

            bounds = sb->bounds();
            if(bounds->maxX - bounds->minX >= bounds->maxY - bounds->minY)
            {
                sub.minX = (half? midPoint[0] : bounds->minX);
                sub.minY = bounds->minY;

                sub.maxX = (half? bounds->maxX : midPoint[0]);
                sub.maxY = bounds->maxY;
            }
            else
            {
                sub.minX = bounds->minX;
                sub.minY = (half? midPoint[1] : bounds->minY);

                sub.maxX = bounds->maxX;
                sub.maxY = (half? bounds->maxY : midPoint[1]);
            }

            child = new SuperBlock(sb->blockmap()/*, &sub*/);
            child->tree = KdTreeNode_AddChild(sb->tree, &sub, half, child);
        }

        sb = (SuperBlock*)KdTreeNode_UserData(KdTreeNode_Child(sb->tree, half));
    }

    return this;
}

bsp_hedge_t* SuperBlock::hedgePop()
{
    if(hedges.empty()) return NULL;

    bsp_hedge_t* hedge = hedges.front();
    hedges.pop_front();

    // Update half-edge counts.
    if(hedge->info.lineDef)
        realNum--;
    else
        miniNum--;

    // Disassociate ourself.
    hedge->block = NULL;
    return hedge;
}

typedef struct {
    int (*callback)(SuperBlock*, void*);
    void* parameters;
} treetraverserparams_t;

static int SuperBlock_TreeTraverser(KdTreeNode* kd, void* parameters)
{
    treetraverserparams_t* p = (treetraverserparams_t*)parameters;
    return p->callback((SuperBlock*)KdTreeNode_UserData(kd), p->parameters);
}

int SuperBlock_Traverse(SuperBlock* sb, int (*callback)(SuperBlock*, void*),
    void* parameters)
{
    treetraverserparams_t parm;
    assert(sb);
    if(!callback) return false; // Continue iteration.

    parm.callback = callback;
    parm.parameters = parameters;
    return KdTreeNode_Traverse2(sb->tree, SuperBlock_TreeTraverser, (void*)&parm);
}

void SuperBlockmap::init(const AABox* bounds)
{
    kdTree = KdTree_New(bounds);

    SuperBlock* block = new SuperBlock(this);
    block->tree = KdTreeNode_SetUserData(KdTree_Root(kdTree), block);
}

SuperBlock* SuperBlockmap::root()
{
    return (SuperBlock*)KdTreeNode_UserData(KdTree_Root(kdTree));
}

typedef struct {
    AABoxf bounds;
    boolean initialized;
} findhedgelistboundsparams_t;

static int findHEdgeBoundsWorker(SuperBlock* block, void* parameters)
{
    findhedgelistboundsparams_t* p = (findhedgelistboundsparams_t*)parameters;
    if(block->hedgeCount(true, true))
    {
        AABoxf blockHEdgeAABox;
        block->findHEdgeBounds(&blockHEdgeAABox);
        if(p->initialized)
        {
            V2_AddToBox(p->bounds.arvec2, blockHEdgeAABox.min);
        }
        else
        {
            V2_InitBox(p->bounds.arvec2, blockHEdgeAABox.min);
            p->initialized = true;
        }
        V2_AddToBox(p->bounds.arvec2, blockHEdgeAABox.max);
    }
    return false; // Continue iteration.
}

static int deleteSuperBlockWorker(SuperBlock* block, void* /*parameters*/)
{
    delete block;
    return false; // Continue iteration.
}

void SuperBlockmap::clear()
{
    SuperBlockmap_PostTraverse(this, deleteSuperBlockWorker);
    KdTree_Delete(kdTree);
}

void SuperBlockmap::findHEdgeBounds(AABoxf* aaBox)
{
    if(!aaBox) return;

    findhedgelistboundsparams_t parm;
    parm.initialized = false;
    SuperBlock_Traverse(root(), findHEdgeBoundsWorker, (void*)&parm);
    if(parm.initialized)
    {
        V2_CopyBox(aaBox->arvec2, parm.bounds.arvec2);
        return;
    }

    // Clear.
    V2_Set(aaBox->min, DDMAXFLOAT, DDMAXFLOAT);
    V2_Set(aaBox->max, DDMINFLOAT, DDMINFLOAT);
}

int SuperBlockmap_PostTraverse(SuperBlockmap* bmap, int(*callback)(SuperBlock*, void*),
    void* parameters)
{
    treetraverserparams_t parm;
    assert(bmap);
    if(!callback) return false; // Continue iteration.

    parm.callback = callback;
    parm.parameters = parameters;
    return KdTree_PostTraverse2(bmap->kdTree, SuperBlock_TreeTraverser, (void*)&parm);
}
