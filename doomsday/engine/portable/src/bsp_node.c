/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *\author Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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
 * bsp_node.c: BSP node builder. Recursive node creation and sorting.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * \notes
 * Split a list of half-edges into two using the method described at the
 * bottom of the file, this was taken from OBJECTS.C in the DEU5beta source.
 *
 * This is done by scanning all of the half-edges and finding the one that
 * does the least splitting and has the least difference in numbers of
 * half-edges on either side.
 *
 * If the ones on the left side make a SSector, then create another SSector
 * else put the half-edges into the left list.
 * If the ones on the right side make a SSector, then create another SSector
 * else put the half-edges into the right list.
 *
 * Rewritten by Andrew Apted (-AJA-), 1999-2000.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_bsp.h"
#include "de_play.h"
#include "de_misc.h"

#include <math.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Used when sorting subsector hEdges by angle around midpoint.
static size_t nodeSortBufSize;
static hedge_node_t** nodeSortBuf;

// CODE --------------------------------------------------------------------

static __inline int pointOnHEdgeSide(double x, double y,
                                     const hedge_t* part)
{
    bsp_hedgeinfo_t*         data = (bsp_hedgeinfo_t*) part->data;

    return P_PointOnLinedefSide2(x, y, data->pDX, data->pDY, data->pPerp,
                                 data->pLength, DIST_EPSILON);
}

/**
 * Add the given half-edge to the specified list.
 */
void BSP_AddHEdgeToSuperBlock(superblock_t* block, hedge_t* hEdge)
{
#define SUPER_IS_LEAF(s)  \
    ((s)->bbox[BOXRIGHT] - (s)->bbox[BOXLEFT] <= 256 && \
     (s)->bbox[BOXTOP] - (s)->bbox[BOXBOTTOM] <= 256)

    for(;;)
    {
        int         p1, p2;
        int         child;
        int         midPoint[2];
        superblock_t *sub;

        midPoint[VX] = (block->bbox[BOXLEFT]   + block->bbox[BOXRIGHT]) / 2;
        midPoint[VY] = (block->bbox[BOXBOTTOM] + block->bbox[BOXTOP])   / 2;

        // Update half-edge counts.
        if(((bsp_hedgeinfo_t*) hEdge->data)->lineDef)
            block->realNum++;
        else
            block->miniNum++;

        if(SUPER_IS_LEAF(block))
        {   // Block is a leaf -- no subdivision possible.
            SuperBlock_LinkHEdge(block, hEdge);
            return;
        }

        if(block->bbox[BOXRIGHT] - block->bbox[BOXLEFT] >=
           block->bbox[BOXTOP]   - block->bbox[BOXBOTTOM])
        {   // Block is wider than it is high, or square.
            p1 = hEdge->v[0]->buildData.pos[VX] >= midPoint[VX];
            p2 = hEdge->v[1]->buildData.pos[VX] >= midPoint[VX];
        }
        else
        {   // Block is higher than it is wide.
            p1 = hEdge->v[0]->buildData.pos[VY] >= midPoint[VY];
            p2 = hEdge->v[1]->buildData.pos[VY] >= midPoint[VY];
        }

        if(p1 && p2)
            child = 1;
        else if(!p1 && !p2)
            child = 0;
        else
        {   // Line crosses midpoint -- link it in and return.
            SuperBlock_LinkHEdge(block, hEdge);
            return;
        }

        // The seg lies in one half of this block. Create the block if it
        // doesn't already exist, and loop back to add the seg.
        if(!block->subs[child])
        {
            block->subs[child] = sub = BSP_SuperBlockCreate();
            sub->parent = block;

            if(block->bbox[BOXRIGHT] - block->bbox[BOXLEFT] >=
               block->bbox[BOXTOP]   - block->bbox[BOXBOTTOM])
            {
                sub->bbox[BOXLEFT] =
                    (child? midPoint[VX] : block->bbox[BOXLEFT]);
                sub->bbox[BOXBOTTOM] = block->bbox[BOXBOTTOM];

                sub->bbox[BOXRIGHT] =
                    (child? block->bbox[BOXRIGHT] : midPoint[VX]);
                sub->bbox[BOXTOP] = block->bbox[BOXTOP];
            }
            else
            {
                sub->bbox[BOXLEFT] = block->bbox[BOXLEFT];
                sub->bbox[BOXBOTTOM] =
                    (child? midPoint[VY] : block->bbox[BOXBOTTOM]);

                sub->bbox[BOXRIGHT] = block->bbox[BOXRIGHT];
                sub->bbox[BOXTOP] =
                    (child? block->bbox[BOXTOP] : midPoint[VY]);
            }
        }

        block = block->subs[child];
    }

#undef SUPER_IS_LEAF
}

static boolean getAveragedCoords(const hedge_node_t* headPtr, double* x,
                                 double* y)
{
    size_t              total = 0;
    double              avg[2];
    const hedge_node_t* n;

    if(!x || !y)
        return false;

    avg[VX] = avg[VY] = 0;

    for(n = headPtr; n; n = n->next)
    {
        const hedge_t*      hEdge = n->hEdge;

        avg[VX] += hEdge->v[0]->buildData.pos[VX];
        avg[VY] += hEdge->v[0]->buildData.pos[VY];

        avg[VX] += hEdge->v[1]->buildData.pos[VX];
        avg[VY] += hEdge->v[1]->buildData.pos[VY];

        total += 2;
    }

    if(total > 0)
    {
        *x = avg[VX] / total;
        *y = avg[VY] / total;
        return true;
    }

    return false;
}

/**
 * Sort half-edges by angle (from the middle point to the start vertex).
 * The desired order (clockwise) means descending angles.
 *
 * \note Algorithm:
 * Uses the now famous "double bubble" sorter :).
 */
static void sortHEdgesByAngleAroundPoint(hedge_node_t** nodes, size_t total,
                                         double x, double y)
{
    size_t              i;

    i = 0;
    while(i + 1 < total)
    {
        hedge_node_t*       a = nodes[i];
        hedge_node_t*       b = nodes[i+1];
        angle_g             angle1, angle2;

        {
        const hedge_t*      hEdgeA = a->hEdge;
        const hedge_t*      hEdgeB = b->hEdge;

        angle1 = M_SlopeToAngle(hEdgeA->v[0]->buildData.pos[VX] - x,
                                hEdgeA->v[0]->buildData.pos[VY] - y);
        angle2 = M_SlopeToAngle(hEdgeB->v[0]->buildData.pos[VX] - x,
                                hEdgeB->v[0]->buildData.pos[VY] - y);
        }

        if(angle1 + ANG_EPSILON < angle2)
        {
            // Swap them.
            nodes[i] = b;
            nodes[i + 1] = a;

            // Bubble down.
            if(i > 0)
                i--;
        }
        else
        {
            // Bubble up.
            i++;
        }
    }
}

/**
 * Sort the given list of half-edges into clockwise order based on their
 * position/orientation compared to the specified point.
 *
 * @param headPtr       Ptr to the address of the headPtr to the list
 *                      of hedges to be sorted.
 * @param num           Number of half edges in the list.
 * @param x             X coordinate of the point to order around.
 * @param y             Y coordinate of the point to order around.
 */
static void clockwiseOrder(hedge_node_t** headPtr, size_t num, double x,
                           double y)
{
    size_t              i, idx;
    hedge_node_t*       n;

    // Insert ptrs to the into the sort buffer.
    idx = 0;
    for(n = *headPtr; n; n = n->next)
        nodeSortBuf[idx++] = n;
    nodeSortBuf[idx] = NULL; // Terminate.

    if(idx != num)
        Con_Error("clockwiseOrder: Miscounted?");

    sortHEdgesByAngleAroundPoint(nodeSortBuf, num, x, y);

    // Re-link the list in the order of the sorted array.
    *headPtr = NULL;
    for(i = 0; i < num; ++i)
    {
        size_t              idx = (num - 1) - i;
        size_t              j = idx % num;

        nodeSortBuf[j]->next = *headPtr;
        *headPtr = nodeSortBuf[j];
    }

/*#if _DEBUG
Con_Message("Sorted half-edges around (%1.1f,%1.1f)\n", x, y);

for(n = sub->hEdges; n; n = n->next)
{
    const hedge_t*      hEdge = n->hEdge;
    angle_g             angle =
        M_SlopeToAngle(hEdge->v[0]->V_pos[VX] - x,
                       hEdge->v[0]->V_pos[VY] - y);

    Con_Message("  half-edge %p: Angle %1.6f  (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                hEdge, angle, hEdge->v[0]->V_pos[VX], hEdge->v[0]->V_pos[VY],
                hEdge->v[1]->V_pos[VX], hEdge->v[1]->V_pos[VY]);
}
#endif*/
}

static void sanityCheckClosed(const bspleafdata_t* leaf)
{
    int                 total = 0, gaps = 0;
    const hedge_node_t* n;

    for(n = leaf->hEdges; n; n = n->next)
    {
        const hedge_t*      a = n->hEdge;
        const hedge_t*      b = (n->next? n->next : leaf->hEdges)->hEdge;

        if(a->v[1]->buildData.pos[VX] != b->v[0]->buildData.pos[VX] ||
           a->v[1]->buildData.pos[VY] != b->v[0]->buildData.pos[VY])
            gaps++;

        total++;
    }

    if(gaps > 0)
    {
        Con_Message("HEdge list for leaf #%p is not closed "
                    "(%d gaps, %d half-edges)\n", leaf, gaps, total);

/*#if _DEBUG
for(n = leaf->hEdges; n; n = n->next)
{
    const hedge_t*      hEdge = n->hEdge;

    Con_Message("  half-edge %p  (%1.1f,%1.1f) --> (%1.1f,%1.1f)\n", hEdge,
                hEdge->v[0]->pos[VX], hEdge->v[0]->pos[VY],
                hEdge->v[1]->pos[VX], hEdge->v[1]->pos[VY]);
}
#endif*/
    }
}

static void sanityCheckSameSector(const bspleafdata_t* leaf)
{
    const hedge_node_t* cur;
    const bsp_hedgeinfo_t* data;

    {
    const hedge_node_t* n;

    // Find a suitable half-edge for comparison.
    for(n = leaf->hEdges; n; n = n->next)
    {
        const hedge_t*      hEdge = n->hEdge;

        if(!((bsp_hedgeinfo_t*) hEdge->data)->sector)
            continue;

        break;
    }

    if(!n)
        return;

    cur = n->next;
    data = (bsp_hedgeinfo_t*) n->hEdge->data;
    }

    for(; cur; cur = cur->next)
    {
        const hedge_t*      hEdge = cur->hEdge;
        const bsp_hedgeinfo_t* curData = (bsp_hedgeinfo_t*) hEdge->data;

        if(!curData->sector)
            continue;

        if(curData->sector == data->sector)
            continue;

        // Prevent excessive number of warnings.
        if(data->sector->buildData.warnedFacing ==
           curData->sector->buildData.index)
            continue;

        data->sector->buildData.warnedFacing =
            curData->sector->buildData.index;

        if(verbose >= 1)
        {
            if(curData->lineDef)
                Con_Message("Sector #%d has sidedef facing #%d (line #%d).\n",
                            data->sector->buildData.index,
                            curData->sector->buildData.index,
                            curData->lineDef->buildData.index);
            else
                Con_Message("Sector #%d has sidedef facing #%d.\n",
                            data->sector->buildData.index,
                            curData->sector->buildData.index);
        }
    }
}

static boolean sanityCheckHasRealHEdge(const bspleafdata_t* leaf)
{
    const hedge_node_t* n;

    for(n = leaf->hEdges; n; n = n->next)
    {
        const hedge_t*      hEdge = n->hEdge;

        if(((const bsp_hedgeinfo_t*) hEdge->data)->lineDef)
            return true;
    }

    return false;
}

static void renumberLeafHEdges(bspleafdata_t* leaf, uint* curIndex)
{
    const hedge_node_t* n;

    for(n = leaf->hEdges; n; n = n->next)
    {
        const hedge_t*      hEdge = n->hEdge;

        ((bsp_hedgeinfo_t*) hEdge->data)->index = *curIndex;
        (*curIndex)++;
    }
}

static void prepareNodeSortBuffer(size_t num)
{
    // Do we need to enlarge our sort buffer?
    if(num + 1 > nodeSortBufSize)
    {
        nodeSortBufSize = num + 1;
        nodeSortBuf =
            M_Realloc(nodeSortBuf, nodeSortBufSize * sizeof(hedge_node_t*));
    }
}

static boolean C_DECL clockwiseLeaf(binarytree_t* tree, void* data)
{
    if(BinaryTree_IsLeaf(tree))
    {   // obj is a leaf.
        size_t              total;
        const hedge_node_t* n;
        bspleafdata_t*      leaf = (bspleafdata_t*) BinaryTree_GetData(tree);
        double              midPoint[2];

        getAveragedCoords(leaf->hEdges, &midPoint[VX], &midPoint[VY]);

        // Count half-edges.
        total = 0;
        for(n = leaf->hEdges; n; n = n->next)
            total++;

        // Ensure the sort buffer is large enough.
        prepareNodeSortBuffer(total);

        clockwiseOrder(&leaf->hEdges, total, midPoint[VX], midPoint[VY]);
        renumberLeafHEdges(leaf, data);

        // Do some sanity checks.
        sanityCheckClosed(leaf);
        sanityCheckSameSector(leaf);
        if(!sanityCheckHasRealHEdge(leaf))
        {
            Con_Error("BSP Leaf #%p has no linedef-linked half-edge!", leaf);
        }
    }

    return true; // Continue traversal.
}

/**
 * Traverse the BSP tree and put all the half-edges in each subsector into
 * clockwise order, and renumber their indices.
 *
 * \important This cannot be done during BuildNodes() since splitting a
 * half-edge with a twin may insert another half-edge into that twin's list,
 * usually in the wrong place order-wise.
 */
void ClockwiseBspTree(binarytree_t* rootNode)
{
    uint                curIndex;

    nodeSortBufSize = 0;
    nodeSortBuf = NULL;

    curIndex = 0;
    BinaryTree_PostOrder(rootNode, clockwiseLeaf, &curIndex);

    // Free temporary storage.
    if(nodeSortBuf)
    {
        M_Free(nodeSortBuf);
        nodeSortBuf = NULL;
    }
}

static void createBSPLeafWorker(bspleafdata_t* leaf, superblock_t* block)
{
    uint                num;

    while(block->hEdges)
    {
        hedge_node_t*       node = block->hEdges;
        hedge_t*            hEdge = node->hEdge;

        SuperBlock_UnLinkHEdge(block, hEdge);
        ((bsp_hedgeinfo_t*) hEdge->data)->block = NULL;

        // Link it into head of the BSP leaf's list.
        BSPLeaf_LinkHEdge(leaf, hEdge);
        ((bsp_hedgeinfo_t*) hEdge->data)->leaf = leaf;
    }

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        superblock_t*       a = block->subs[num];

        if(a)
        {
            createBSPLeafWorker(leaf, a);

            if(a->realNum + a->miniNum > 0)
                Con_Error("createSubSectorWorker: child %d not empty!", num);

            BSP_SuperBlockDestroy(a);
            block->subs[num] = NULL;
        }
    }

    block->realNum = block->miniNum = 0;
}

static __inline bspleafdata_t* allocBSPLeaf(void)
{
    return M_Malloc(sizeof(bspleafdata_t));
}

static __inline void freeBSPLeaf(bspleafdata_t* leaf)
{
    M_Free(leaf);
}

bspleafdata_t* BSPLeaf_Create(void)
{
    bspleafdata_t*      leaf = allocBSPLeaf();

    leaf->hEdges = NULL;

    return leaf;
}

void BSPLeaf_Destroy(bspleafdata_t* leaf)
{
    if(!leaf)
        return;

    while(leaf->hEdges)
    {
        hedge_t*            hEdge = leaf->hEdges->hEdge;

        BSPLeaf_UnLinkHEdge(leaf, hEdge);

        if(hEdge->data)
            Z_Free(hEdge->data);
        HEdge_Destroy(hEdge);
    }

    freeBSPLeaf(leaf);
}

/**
 * Create a new leaf from a list of half-edges.
 */
static bspleafdata_t* createBSPLeaf(superblock_t* hEdgeList)
{
    bspleafdata_t*      leaf = BSPLeaf_Create();

    // Link the half-edges into the new leaf.
    createBSPLeafWorker(leaf, hEdgeList);

    return leaf;
}

/**
 * Takes the half-edge list and determines if it is convex, possibly
 * converting it into a subsector. Otherwise, the list is divided into two
 * halves and recursion will continue on the new sub list.
 *
 * @param hEdgeList     Ptr to the list of half edges at the current node.
 * @param parent        Ptr to write back the address of any newly created
 *                      subtree.
 * @param depth         Current tree depth.
 * @param cutList       Ptr to the cutlist to use for storing new segment
 *                      intersections (cuts).
 * @return              @c true, if successfull.
 */
boolean BuildNodes(superblock_t* hEdgeList, binarytree_t** parent,
                   size_t depth, cutlist_t* cutList)
{
    binarytree_t*       subTree;
    bspnodedata_t*      node;
    superblock_t*       hEdgeSet[2];
    bspleafdata_t*      leaf;
    boolean             builtOK = false;
    bspartition_t       partition;

    *parent = NULL;

/*#if _DEBUG
Con_Message("Build: Begun @ %lu\n", (unsigned long) depth);
BSP_PrintSuperblockHEdges(hEdgeList);
#endif*/

    // Pick the next partition to use.
    if(!SuperBlock_PickPartition(hEdgeList, depth, &partition))
    {   // No partition required, already convex.
/*#if _DEBUG
Con_Message("BuildNodes: Convex.\n");
#endif*/
        leaf = createBSPLeaf(hEdgeList);
        *parent = BinaryTree_Create(leaf);
        return true;
    }

/*#if _DEBUG
Con_Message("BuildNodes: Partition %p (%1.0f,%1.0f) -> (%1.0f,%1.0f).\n",
            best, best->v[0]->V_pos[VX], best->v[0]->V_pos[VY],
            best->v[1]->V_pos[VX], best->v[1]->V_pos[VY]);
#endif*/

    // Create left and right super blocks.
    hEdgeSet[RIGHT] = (superblock_t *) BSP_SuperBlockCreate();
    hEdgeSet[LEFT]  = (superblock_t *) BSP_SuperBlockCreate();

    // Copy the bounding box of the edge list to the superblocks.
    M_CopyBox(hEdgeSet[LEFT]->bbox, hEdgeList->bbox);
    M_CopyBox(hEdgeSet[RIGHT]->bbox, hEdgeList->bbox);

    // Divide the half-edges into two lists: left & right.
    BSP_PartitionHEdges(hEdgeList, &partition, hEdgeSet[RIGHT],
                        hEdgeSet[LEFT], cutList);
    BSP_CutListEmpty(cutList);

    node = M_Calloc(sizeof(bspnodedata_t));
    *parent = BinaryTree_Create(node);

    BSP_FindNodeBounds(node, hEdgeSet[RIGHT], hEdgeSet[LEFT]);

    node->partition.x = partition.x;
    node->partition.y = partition.y;
    node->partition.dX = partition.dX;
    node->partition.dY = partition.dY;

    builtOK = BuildNodes(hEdgeSet[RIGHT], &subTree, depth + 1, cutList);
    BinaryTree_SetChild(*parent, RIGHT, subTree);
    BSP_SuperBlockDestroy(hEdgeSet[RIGHT]);

    if(builtOK)
    {
        builtOK = BuildNodes(hEdgeSet[LEFT], &subTree, depth + 1, cutList);
        BinaryTree_SetChild(*parent, LEFT, subTree);
    }

    BSP_SuperBlockDestroy(hEdgeSet[LEFT]);

    return builtOK;
}

//---------------------------------------------------------------------------
//
//    This message has been taken, complete, from OBJECTS.C in DEU5beta
//    source.  It outlines the method used here to pick the nodelines.
//
// IF YOU ARE WRITING A DOOM EDITOR, PLEASE READ THIS:
//
// I spent a lot of time writing the Nodes builder.  There are some bugs in
// it, but most of the code is OK.  If you steal any ideas from this program,
// put a prominent message in your own editor to make it CLEAR that some
// original ideas were taken from DEU.  Thanks.
//
// While everyone was talking about LineDefs, I had the idea of taking only
// the Segs into account, and creating the Segs directly from the SideDefs.
// Also, dividing the list of Segs in two after each call to CreateNodes makes
// the algorithm faster.  I use several other tricks, such as looking at the
// two ends of a Seg to see on which side of the nodeline it lies or if it
// should be split in two.  I took me a lot of time and efforts to do this.
//
// I give this algorithm to whoever wants to use it, but with this condition:
// if your program uses some of the ideas from DEU or the whole algorithm, you
// MUST tell it to the user.  And if you post a message with all or parts of
// this algorithm in it, please post this notice also.  I don't want to speak
// legalese; I hope that you understand me...  I kindly give the sources of my
// program to you: please be kind with me...
//
// If you need more information about this, here is my E-mail address:
// Raphael.Quinet@eed.ericsson.se (Raphael Quinet).
//
// Short description of the algorithm:
//   1 - Create one Seg for each SideDef: pick each LineDef in turn.  If it
//       has a "first" SideDef, then create a normal Seg.  If it has a
//       "second" SideDef, then create a flipped Seg.
//   2 - Call CreateNodes with the current list of Segs.  The list of Segs is
//       the only argument to CreateNodes.
//   3 - Save the Nodes, Segs and SSectors to disk.  Start with the leaves of
//       the Nodes tree and continue up to the root (last Node).
//
// CreateNodes does the following:
//   1 - Pick a nodeline amongst the Segs (minimize the number of splits and
//       keep the tree as balanced as possible).
//   2 - Move all Segs on the right of the nodeline in a list (segs1) and do
//       the same for all Segs on the left of the nodeline (in segs2).
//   3 - If the first list (segs1) contains references to more than one
//       Sector or if the angle between two adjacent Segs is greater than
//       180 degrees, then call CreateNodes with this (smaller) list.
//       Else, create a SubSector with all these Segs.
//   4 - Do the same for the second list (segs2).
//   5 - Return the new node (its two children are already OK).
//
// Each time CreateSSector is called, the Segs are put in a global list.
// When there is no more Seg in CreateNodes' list, then they are all in the
// global list and ready to be saved to disk.
//
