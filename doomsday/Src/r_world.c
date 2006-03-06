/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Ker�nen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * r_world.c: World Setup and Refresh
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <assert.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define DOMINANT_SIZE 1000

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    R_PrepareSubsector(subsector_t *sub);
void    R_FindLineNeighbors(sector_t *sector, line_t *line,
                            struct line_s **neighbors, int alignment);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     rendSkyLight = 1;       // cvar

char    currentLevelId[64];

boolean firstFrameAfterLoad;
boolean levelSetup;

sectorinfo_t    *secinfo;
seginfo_t       *seginfo;
subsectorinfo_t *subsecinfo;
lineinfo_t      *lineinfo;
vertexowner_t   *vertexowners;
nodeindex_t     *linelinks;         // indices to roots

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean noSkyColorGiven;
static byte skyColorRGB[4], balancedRGB[4];
static float skyColorBalance;
static float mapBounds[4];

// CODE --------------------------------------------------------------------

#ifdef _MSC_VER
#  pragma optimize("g", off)
#endif

/*
 * We mustn't create links which form loops. This will start looking
 * from destlink, and if it finds startsec we're in trouble.
 */
boolean R_IsValidLink(sector_t *startsec, sector_t *destlink, boolean is_floor)
{
    sector_t *sec = destlink;
    sector_t *link;
    sectorinfo_t *sin;

    for(;;)
    {
        sin = SECT_INFO(sec);
        // Advance to the linked sector.
        if(is_floor)
        {
            if(!sin->linkedfloor)
                break;
            link = sin->linkedfloor;
        }
        else
        {
            if(!sin->linkedceil)
                break;
            link = sin->linkedceil;
        }
        // Is there an illegal linkage?
        if(sec == link || startsec == link)
            return false;
        sec = link;
    }
    // No problems encountered.
    return true;
}

/*
 * Called every frame. Sector heights may change at any time
 * without notice.
 */
void R_SetSectorLinks(sector_t *sec)
{
    int     i = GET_SECTOR_IDX(sec), k;
    sector_t *back;
    boolean hackfloor, hackceil;
    side_t *sid, *frontsid, *backsid;
    sector_t *floorlink_candidate = 0, *ceillink_candidate = 0;

    //return; //---DEBUG---

    // Must have a valid sector!
    if(!sec || !sec->linecount || secinfo[i].permanentlink)
        return;                 // Can't touch permanent links.

    hackfloor = (sec->floorpic != skyflatnum);
    hackceil = (sec->ceilingpic != skyflatnum);
    for(k = 0; k < sec->linecount; k++)
    {
        if(!hackfloor && !hackceil)
            break;
        // Most sectors will fail the test below.
        if(!sec->Lines[k]->frontsector || !sec->Lines[k]->backsector)
            return;
        // Check which way the line is facing.
        sid = SIDE_PTR(sec->Lines[k]->sidenum[0]);
        if(sid->sector == sec)
        {
            frontsid = sid;
            backsid = SIDE_PTR(sec->Lines[k]->sidenum[1]);
        }
        else
        {
            frontsid = SIDE_PTR(sec->Lines[k]->sidenum[1]);
            backsid = sid;
        }
        back = backsid->sector;
        if(back == sec)
            return;
        // Check that there is something on the other side.
        if(back->ceilingheight == back->floorheight)
            return;
        // Check the conditions that prevent the invis plane.
        if(back->floorheight == sec->floorheight)
            hackfloor = false;
        else
        {
            if(back->floorheight > sec->floorheight)
                sid = frontsid;
            else
                sid = backsid;
            if(sid->bottomtexture || sid->midtexture)
                hackfloor = false;
            else if(R_IsValidLink(sec, back, true))
                floorlink_candidate = back;
        }
        if(back->ceilingheight == sec->ceilingheight)
            hackceil = false;
        else
        {
            if(back->ceilingheight < sec->ceilingheight)
                sid = frontsid;
            else
                sid = backsid;
            if(sid->toptexture || sid->midtexture)
                hackceil = false;
            else if(R_IsValidLink(sec, back, false))
                ceillink_candidate = back;
        }
    }
    if(hackfloor)
    {
        secinfo[i].linkedfloor = floorlink_candidate;

        /*      if(floorlink_candidate)
           Con_Printf("LF:%i->%i\n",
           i, GET_SECTOR_IDX(floorlink_candidate)); */
    }
    if(hackceil)
    {
        secinfo[i].linkedceil = ceillink_candidate;

        /*      if(ceillink_candidate)
           Con_Printf("LC:%i->%i\n",
           i, GET_SECTOR_IDX(ceillink_candidate)); */
    }
}

#ifdef _MSC_VER
#  pragma optimize("", on)
#endif

/*
 * Returns a pointer to the list of points. It must be used.
 */
fvertex_t *edgeClipper(int *numpoints, fvertex_t * points, int numclippers,
                       fdivline_t * clippers)
{
    unsigned char sidelist[MAX_POLY_SIDES];
    int     i, k, num = *numpoints;

    // We'll clip the polygon with each of the divlines. The left side of
    // each divline is discarded.
    for(i = 0; i < numclippers; i++)
    {
        fdivline_t *curclip = clippers + i;

        // First we'll determine the side of each vertex. Points are allowed
        // to be on the line.
        for(k = 0; k < num; k++)
        {
            sidelist[k] = P_FloatPointOnLineSide(points + k, curclip);
        }

        for(k = 0; k < num; k++)
        {
            int     startIdx = k, endIdx = k + 1;

            // Check the end index.
            if(endIdx == num)
                endIdx = 0;     // Wrap-around.

            // Clipping will happen when the ends are on different sides.
            if(sidelist[startIdx] != sidelist[endIdx])
            {
                fvertex_t newvert;

                // Find the intersection point of intersecting lines.
                P_FloatInterceptVertex(points + startIdx, points + endIdx,
                                       curclip, &newvert);

                // Add the new vertex. Also modify the sidelist.
                points =
                    (fvertex_t *) realloc(points, (++num) * sizeof(fvertex_t));
                if(num >= MAX_POLY_SIDES)
                    Con_Error("Too many points in clipper.\n");

                // Make room for the new vertex.
                memmove(points + endIdx + 1, points + endIdx,
                        (num - endIdx - 1) * sizeof(fvertex_t));
                memcpy(points + endIdx, &newvert, sizeof(newvert));

                memmove(sidelist + endIdx + 1, sidelist + endIdx,
                        num - endIdx - 1);
                sidelist[endIdx] = 1;

                // Skip over the new vertex.
                k++;
            }
        }

        // Now we must discard the points that are on the wrong side.
        for(k = 0; k < num; k++)
            if(!sidelist[k])
            {
                memmove(points + k, points + k + 1,
                        (num - k - 1) * sizeof(fvertex_t));
                memmove(sidelist + k, sidelist + k + 1, num - k - 1);
                num--;
                k--;
            }
    }
    // Screen out consecutive identical points.
    for(i = 0; i < num; i++)
    {
        int     previdx = i - 1;

        if(previdx < 0)
            previdx = num - 1;
        if(points[i].x == points[previdx].x &&
           points[i].y == points[previdx].y)
        {
            // This point (i) must be removed.
            memmove(points + i, points + i + 1,
                    sizeof(fvertex_t) * (num - i - 1));
            num--;
            i--;
        }
    }
    *numpoints = num;
    return points;
}

void R_ConvexClipper(subsector_t *ssec, int num, divline_t * list)
{
    int     numclippers = num + ssec->linecount;
    int     i, numedgepoints;
    fvertex_t *edgepoints;
    fdivline_t *clippers = (fdivline_t *)
        Z_Malloc(numclippers * sizeof(fdivline_t), PU_STATIC, 0);

    // Convert the divlines to float, in reverse order.
    for(i = 0; i < numclippers; i++)
    {
        if(i < num)
        {
            clippers[i].x = FIX2FLT(list[num - i - 1].x);
            clippers[i].y = FIX2FLT(list[num - i - 1].y);
            clippers[i].dx = FIX2FLT(list[num - i - 1].dx);
            clippers[i].dy = FIX2FLT(list[num - i - 1].dy);
        }
        else
        {
            seg_t  *seg = SEG_PTR(ssec->firstline + i - num);

            clippers[i].x = FIX2FLT(seg->v1->x);
            clippers[i].y = FIX2FLT(seg->v1->y);
            clippers[i].dx = FIX2FLT(seg->v2->x - seg->v1->x);
            clippers[i].dy = FIX2FLT(seg->v2->y - seg->v1->y);
        }
    }

    // Setup the 'worldwide' polygon.
    numedgepoints = 4;
    edgepoints = (fvertex_t *) malloc(numedgepoints * sizeof(fvertex_t));

    edgepoints[0].x = -32768;
    edgepoints[0].y = 32768;

    edgepoints[1].x = 32768;
    edgepoints[1].y = 32768;

    edgepoints[2].x = 32768;
    edgepoints[2].y = -32768;

    edgepoints[3].x = -32768;
    edgepoints[3].y = -32768;

    // Do some clipping, <snip> <snip>
    edgepoints = edgeClipper(&numedgepoints, edgepoints, numclippers, clippers);

    if(!numedgepoints)
    {
        printf("All clipped away: subsector %i\n", GET_SUBSECTOR_IDX(ssec));
        ssec->numverts = 0;
        ssec->verts = 0;
        //ssec->origverts = 0;
        //ssec->diffverts = 0;
    }
    else
    {
        // We need these with dynamic lights.
        ssec->verts = Z_Malloc(sizeof(fvertex_t) * numedgepoints, PU_LEVEL, 0);
        memcpy(ssec->verts, edgepoints, sizeof(fvertex_t) * numedgepoints);
        ssec->numverts = numedgepoints;

        R_PrepareSubsector(ssec);
    }

    // We're done, free the edgepoints memory.
    free(edgepoints);
    Z_Free(clippers);
}

void R_PrepareSubsector(subsector_t *sub)
{
    int     j, num = sub->numverts;

    // Find the center point. First calculate the bounding box.
    sub->bbox[0].x = sub->bbox[1].x = sub->verts[0].x;
    sub->bbox[0].y = sub->bbox[1].y = sub->verts[0].y;
    sub->midpoint.x = sub->verts[0].x;
    sub->midpoint.y = sub->verts[0].y;
    for(j = 1; j < num; j++)
    {
        if(sub->verts[j].x < sub->bbox[0].x)
            sub->bbox[0].x = sub->verts[j].x;
        if(sub->verts[j].y < sub->bbox[0].y)
            sub->bbox[0].y = sub->verts[j].y;
        if(sub->verts[j].x > sub->bbox[1].x)
            sub->bbox[1].x = sub->verts[j].x;
        if(sub->verts[j].y > sub->bbox[1].y)
            sub->bbox[1].y = sub->verts[j].y;
        sub->midpoint.x += sub->verts[j].x;
        sub->midpoint.y += sub->verts[j].y;
    }
    sub->midpoint.x /= num;
    sub->midpoint.y /= num;
}

void R_PolygonizeWithoutCarving()
{
    int     i;
    int     j;
    subsector_t *sub;

    for(i = numsubsectors -1; i >= 0; --i)
    {
        sub = SUBSECTOR_PTR(i);
        sub->numverts = sub->linecount;
        sub->verts =
            (fvertex_t *) Z_Malloc(sizeof(fvertex_t) * sub->linecount,
                                   PU_LEVEL, 0);
        for(j = 0; j < sub->linecount; j++)
        {
            sub->verts[j].x = FIX2FLT(SEG_PTR(sub->firstline + j)->v1->x);
            sub->verts[j].y = FIX2FLT(SEG_PTR(sub->firstline + j)->v1->y);
        }

        R_PrepareSubsector(sub);
    }
}

/*
 * Recursively polygonizes all ceilings and floors.
 */
void R_CreateFloorsAndCeilings(int bspnode, int numdivlines,
                               divline_t * divlines)
{
    node_t *nod;
    divline_t *childlist, *dl;
    int     childlistsize = numdivlines + 1;

    // If this is a subsector we are dealing with, begin carving with the
    // given list.
    if(bspnode & NF_SUBSECTOR)
    {
        // We have arrived at a subsector. The divline list contains all
        // the partition lines that carve out the subsector.
        int     ssidx = bspnode & (~NF_SUBSECTOR);

        R_ConvexClipper(SUBSECTOR_PTR(ssidx), numdivlines, divlines);
        // This leaf is done.
        return;
    }

    // Get a pointer to the node.
    nod = NODE_PTR(bspnode);

    // Allocate a new list for each child.
    childlist = (divline_t *) malloc(childlistsize * sizeof(divline_t));

    // Copy the previous lines, from the parent nodes.
    if(divlines)
        memcpy(childlist, divlines, numdivlines * sizeof(divline_t));

    dl = childlist + numdivlines;
    dl->x = nod->x;
    dl->y = nod->y;
    // The right child gets the original line (LEFT side clipped).
    dl->dx = nod->dx;
    dl->dy = nod->dy;
    R_CreateFloorsAndCeilings(nod->children[0], childlistsize, childlist);

    // The left side. We must reverse the line, otherwise the wrong
    // side would get clipped.
    dl->dx = -nod->dx;
    dl->dy = -nod->dy;
    R_CreateFloorsAndCeilings(nod->children[1], childlistsize, childlist);

    // We are finishing with this node, free the allocated list.
    free(childlist);
}

/*
 * Fixing the sky means that for adjacent sky sectors the lower sky
 * ceiling is lifted to match the upper sky. The raising only affects
 * rendering, it has no bearing on gameplay.
 */
void R_SkyFix(void)
{
    boolean adjusted;
    int     i, f, b;

    // Check all things first.
    for(i = 0; i < numsectors; i++)
    {
        sector_t *sec = SECTOR_PTR(i);
        mobj_t *it;

        // Must have a sky ceiling.
        if(sec->ceilingpic != skyflatnum)
            continue;
        // Check that all the things in the sector fit in.
        for(it = sec->thinglist; it; it = it->snext)
        {
            b = it->height >> FRACBITS;
            f = (sec->ceilingheight >> FRACBITS) + sec->skyfix -
                (sec->floorheight >> FRACBITS);
            if(b > f)
            {
                // Must increase skyfix.
                sec->skyfix += b - f;
                if(verbose)
                {
                    Con_Printf("S%i: (mo)skyfix to %i (ceil=%i)\n",
                               GET_SECTOR_IDX(sec), sec->skyfix,
                               (sec->ceilingheight >> FRACBITS) + sec->skyfix);
                }
            }
        }
    }

    // We'll do this as long as we must to be sure all the sectors are fixed.
    do
    {
        adjusted = false;

        // We need to check all the linedefs.
        for(i = 0; i < numlines; i++)
        {
            line_t *line = LINE_PTR(i);
            sector_t *front = line->frontsector, *back = line->backsector;
            int     fix = 0;

            // The conditions: must have two sides.
            if(!front || !back)
                continue;

            // Both the front and back sectors must have the sky ceiling.
            if(front->ceilingpic != skyflatnum ||
               back->ceilingpic != skyflatnum)
                continue;

            f = (front->ceilingheight >> FRACBITS) + front->skyfix;
            b = (back->ceilingheight >> FRACBITS) + back->skyfix;
            if(f < b)
            {
                fix = b - (front->ceilingheight >> FRACBITS);
                if(fix > front->skyfix)
                {
                    front->skyfix = fix;
                    if(verbose)
                    {
                        Con_Printf("S%i: skyfix to %i (ceil=%i)\n",
                                   GET_SECTOR_IDX(front), front->skyfix,
                                   (front->ceilingheight >> FRACBITS) +
                                   front->skyfix);
                    }
                    adjusted = true;
                }
            }
            else if(f > b)
            {
                fix = f - (back->ceilingheight >> FRACBITS);
                if(fix > back->skyfix)
                {
                    back->skyfix = fix;
                    if(verbose)
                    {
                        Con_Printf("S%i: skyfix to %i (ceil=%i)\n",
                                   GET_SECTOR_IDX(back), back->skyfix,
                                   (back->ceilingheight >> FRACBITS) +
                                   back->skyfix);
                    }
                    adjusted = true;
                }
            }
        }
    }
    while(adjusted);
}

static float TriangleArea(fvertex_t * o, fvertex_t * s, fvertex_t * t)
{
    fvertex_t a = { s->x - o->x, s->y - o->y }, b =
    {
    t->x - o->x, t->y - o->y};
    float   area = (a.x * b.y - b.x * a.y) / 2;

    if(area < 0)
        area = -area;
    return area;
}

/*
 * Returns true if 'base' is a good tri-fan base.
 */
int R_TestTriFan(subsector_t *sub, int base)
{
#define TRIFAN_LIMIT    0.1
    int     i, a, b;

    if(sub->numverts == 3)
        return true;            // They're all valid.
    // Higher vertex counts need checking.
    for(i = 0; i < sub->numverts - 2; i++)
    {
        a = base + 1 + i;
        b = a + 1;
        if(a >= sub->numverts)
            a -= sub->numverts;
        if(b >= sub->numverts)
            b -= sub->numverts;
        if(TriangleArea(sub->verts + base, sub->verts + a, sub->verts + b) <=
           TRIFAN_LIMIT)
            return false;
    }
    // Whole triangle fan checked out OK, must be good.
    return true;
}

void R_SubsectorPlanes(void)
{
    int     i, k, num;
    subsector_t *sub;
    fvertex_t buf[RL_MAX_POLY_SIDES];

    for(i = 0; i < numsubsectors; i++)
    {
        sub = SUBSECTOR_PTR(i);
        num = sub->numverts;
        // We need to find a good tri-fan base vertex.
        // (One that doesn't generate zero-area triangles).
        // We'll test each one and pick the first good one.
        for(k = 0; k < num; k++)
        {
            if(R_TestTriFan(sub, k))
            {
                // Yes! This'll do nicely. Change the order of the
                // vertices so that k comes first.
                if(k)           // Need to change?
                {
                    memcpy(buf, sub->verts, sizeof(fvertex_t) * num);
                    memcpy(sub->verts, buf + k, sizeof(fvertex_t) * (num - k));
                    memcpy(sub->verts + (num - k), buf, sizeof(fvertex_t) * k);
                }
                goto ddSP_NextSubSctr;
            }
        }
        // There was no match. Bugger. We need to use the subsector
        // midpoint as the base. It's always valid.
        sub->flags |= DDSUBF_MIDPOINT;
        //Con_Message("Using midpoint for subsctr %i.\n", i);

      ddSP_NextSubSctr:;
    }
}

void R_SetVertexOwner(int idx, sector_t *secptr)
{
    int     i;
    vertexowner_t *own;
    unsigned short *list;
    int     sector;

    if(!secptr)
        return;
    sector = GET_SECTOR_IDX(secptr);
    own = vertexowners + idx;
    // Has this sector been already registered?
    for(i = 0; i < own->num; i++)
        if(own->list[i] == sector)
            return;             // Yes, we can exit.
    // Add a new owner.
    own->num++;
    // Allocate a new list.
    list =
        (unsigned short *) Z_Malloc(sizeof(unsigned short) * own->num,
                                    PU_LEVEL, 0);
    // If there are previous references, copy them.
    if(own->num > 1)
    {
        memcpy(list, own->list, sizeof(unsigned short) * (own->num - 1));
        // Free the old list.
        Z_Free(own->list);
    }
    own->list = list;
    own->list[own->num - 1] = sector;
}

void R_SetVertexLineOwner(int idx, line_t *lineptr)
{
    int     i;
    vertexowner_t *own;
    unsigned short *list;
    int     line;

    if(!lineptr)
        return;
    line = GET_LINE_IDX(lineptr);
    own = vertexowners + idx;
    // Has this line been already registered?
    for(i = 0; i < own->numlines; i++)
        if(own->linelist[i] == line)
            return;             // Yes, we can exit.
    // Add a new owner.
    own->numlines++;
    // Allocate a new list.
    list =
        (unsigned short *) Z_Malloc(sizeof(unsigned short) * own->numlines,
                                    PU_LEVEL, 0);
    // If there are previous references, copy them.
    if(own->numlines > 1)
    {
        memcpy(list, own->linelist, sizeof(unsigned short) * (own->numlines - 1));
        // Free the old list.
        Z_Free(own->linelist);
    }
    own->linelist = list;
    own->linelist[own->numlines - 1] = line;
}

/*
 * Generates an array of sector references for each vertex. The list
 * includes all the sectors the vertex belongs to.
 *
 * Generates an array of line references for each vertex. The list
 * includes all the lines the vertex belongs to.
 */
void R_InitVertexOwners(void)
{
    int     i, k, p, v[2];
    sector_t *sec;

    // Allocate enough memory.
    vertexowners =
        (vertexowner_t *) Z_Malloc(sizeof(vertexowner_t) * numvertexes,
                                   PU_LEVEL, 0);
    memset(vertexowners, 0, sizeof(vertexowner_t) * numvertexes);

    for(i = 0, sec = sectors; i < numsectors; i++, sec++)
    {
        // Traversing the line list will do fine.
        for(k = 0; k < sec->linecount; k++)
        {
            line_t* line = sec->Lines[k];
            v[0] = GET_VERTEX_IDX(line->v1);
            v[1] = GET_VERTEX_IDX(line->v2);
            for(p = 0; p < 2; p++)
            {
                R_SetVertexOwner(v[p], line->frontsector);
                R_SetVertexOwner(v[p], line->backsector);
                R_SetVertexLineOwner(v[p], line);
            }
        }
    }
}

/*boolean DD_SubContainTest(sector_t *innersec, sector_t *outersec)
   {
   int i, k;
   boolean contained;
   float in[4], out[4];
   subsector_t *isub, *osub;

   // Test each subsector of innersec against all subsectors of outersec.
   for(i=0; i<numsubsectors; i++)
   {
   isub = SUBSECTOR_PTR(i);
   contained = false;
   // Only accept innersec's subsectors.
   if(isub->sector != innersec) continue;
   for(k=0; k<numsubsectors && !contained; k++)
   {
   osub = SUBSECTOR_PTR(i);
   // Only accept outersec's subsectors.
   if(osub->sector != outersec) continue;
   // Test if isub is inside osub.
   if(isub->bbox[BLEFT] >= osub->bbox[BLEFT]
   && isub->bbox[BRIGHT] <= osub->bbox[BRIGHT]
   && isub->bbox[BTOP] >= osub->bbox[BTOP]
   && isub->bbox[BBOTTOM] <= osub->bbox[BBOTTOM])
   {
   // This is contained.
   contained = true;
   }
   }
   if(!contained) return false;
   }
   // All tested subsectors were contained!
   return true;
   } */

/*
 * The test is done on subsectors.
 */
sector_t *R_GetContainingSectorOf(sector_t *sec)
{
    int     i;
    float   cdiff = -1, diff;
    float   inner[4], outer[4];
    sector_t *other, *closest = NULL;

    memcpy(inner, secinfo[GET_SECTOR_IDX(sec)].bounds, sizeof(inner));

    // Try all sectors that fit in the bounding box.
    for(i = 0; i < numsectors; i++)
    {
        other = SECTOR_PTR(i);
        if(!other->linecount)
            continue;
        if(other == sec)
            continue;           // Don't try on self!
        memcpy(outer, secinfo[i].bounds, sizeof(outer));
        if(inner[BLEFT] >= outer[BLEFT] && inner[BRIGHT] <= outer[BRIGHT] &&
           inner[BTOP] >= outer[BTOP] && inner[BBOTTOM] <= outer[BBOTTOM])
        {
            // Inside! Now we must test each of the subsectors. Otherwise
            // we can't be sure...
            /*if(DD_SubContainTest(sec, other))
               { */
            // Sec is totally and completely inside other!
            diff = M_BoundingBoxDiff(inner, outer);
            if(cdiff < 0 || diff <= cdiff)
            {
                closest = other;
                cdiff = diff;
            }
            //}
        }
    }
    return closest;
}

void R_InitSectorInfo(void)
{
    int     i, k;
    sectorinfo_t *info;
    sector_t *sec, *other;
    line_t *lin;
    boolean dohack;

    secinfo = Z_Calloc(sizeof(sectorinfo_t) * numsectors, PU_LEVEL, 0);

    // Calculate bounding boxes for all sectors.
    for(i = 0; i < numsectors; i++)
    {
        P_SectorBoundingBox(SECTOR_PTR(i), secinfo[i].bounds);

        if(i == 0)
        {
            // The first sector is used as is.
            memcpy(mapBounds, secinfo[i].bounds, sizeof(mapBounds));
        }
        else
        {
            // Expand the bounding box.
            M_JoinBoxes(mapBounds, secinfo[i].bounds);
        }
    }

    for(i = 0, info = secinfo; i < numsectors; i++, info++)
    {
        sec = SECTOR_PTR(i);
        if(!sec->linecount)
            continue;
        dohack = true;
        for(k = 0; k < sec->linecount; k++)
        {
            lin = sec->Lines[k];

            if(!lin->frontsector || !lin->backsector ||
               lin->frontsector != lin->backsector)
            {
                dohack = false;
                break;
            }
        }

        if(dohack)
        {
            // Link permanently.
            info->permanentlink = true;
            info->linkedceil = info->linkedfloor =
                R_GetContainingSectorOf(sec);
            if(info->linkedceil)
            {
                Con_Printf("Linking S%i planes permanently to S%i\n", i,
                           GET_SECTOR_IDX(info->linkedceil));
            }
        }

        // Is this sector large enough to be a dominant light source?
        if(info->lightsource == NULL &&
           (sec->ceilingpic == skyflatnum || sec->floorpic == skyflatnum) &&
           info->bounds[BRIGHT] - info->bounds[BLEFT] > DOMINANT_SIZE &&
           info->bounds[BBOTTOM] - info->bounds[BTOP] > DOMINANT_SIZE)
        {
            // All sectors touching this one will be affected.
            for(k = 0; k < sec->linecount; k++)
            {
                other = sec->Lines[k]->frontsector;
                if(!other || other == sec)
                {
                    other = sec->Lines[k]->backsector;
                    if(!other || other == sec)
                        continue;
                }
                SECT_INFO(other)->lightsource = sec;
            }
        }
    }
}

void R_InitSegInfo(void)
{
    int i, k, j, n;
    seginfo_t *inf;

    seginfo = Z_Calloc(numsegs * sizeof(seginfo_t), PU_LEVEL, NULL);

    for(i = 0, inf = seginfo; i < numsegs; ++i, ++inf)
    {
        for(k = 0; k < 4; ++k)
        {
/*            inf->illum[0][k].front =
                inf->illum[1][k].front =
                inf->illum[2][k].front = SEG_PTR(i)->frontsector;*/

            for(j = 0; j < 3; ++j)
            {
                inf->illum[j][k].flags = VIF_STILL_UNSEEN;

                for(n = 0; n < MAX_BIAS_AFFECTED; ++n)
                {
                    inf->illum[j][k].casted[n].source = -1;
                }
            }
        }
    }
}

void R_InitPlanePoly(planeinfo_t *plane, boolean reverse,
                     subsector_t *subsector)
{
    int     numvrts, i, j;
    fvertex_t *vrts, *vtx, *pv;

    // Take the subsector's vertices.
    numvrts = subsector->numverts;
    vrts = subsector->verts;

    // Copy the vertices to the poly.
    if(subsector->flags & DDSUBF_MIDPOINT)
    {
        // Triangle fan base is the midpoint of the subsector.
        plane->numvertices = 2 + numvrts;
        plane->vertices =
            Z_Malloc(sizeof(fvertex_t) * plane->numvertices, PU_LEVEL, 0);

        memcpy(plane->vertices, &subsector->midpoint, sizeof(fvertex_t));

        vtx = vrts + (!reverse ? 0 : numvrts - 1);
        pv = plane->vertices + 1;
    }
    else
    {
        plane->numvertices = numvrts;
        plane->vertices =
            Z_Malloc(sizeof(fvertex_t) * plane->numvertices, PU_LEVEL, 0);

        // The first vertex is always the same: vertex zero.
        pv = plane->vertices;
        memcpy(pv, &vrts[0], sizeof(*pv));

        vtx = vrts + (!reverse ? 1 : numvrts - 1);
        pv++;
        numvrts--;
    }

    // Add the rest of the vertices.
    for(; numvrts > 0; numvrts--, (!reverse ? vtx++ : vtx--), pv++)
        memcpy(pv, vtx, sizeof(*vtx));

    if(subsector->flags & DDSUBF_MIDPOINT)
    {
        // Re-add the first vertex so the triangle fan wraps around.
        memcpy(pv, &plane->vertices[1], sizeof(*pv));
    }

    // Initialize the illumination for the subsector.
    plane->illumination = Z_Calloc(plane->numvertices * sizeof(vertexillum_t),
                                   PU_LEVEL, NULL);
    for(i = 0; i < plane->numvertices; ++i)
    {
        plane->illumination[i].flags |= VIF_STILL_UNSEEN;

        for(j = 0; j < MAX_BIAS_AFFECTED; ++j)
            plane->illumination[i].casted[j].source = -1;
    }
}

void R_InitSubsectorInfo(void)
{
    int     i;
    subsector_t *sub;
    subsectorinfo_t *info;
    sector_t *sector;

    i = sizeof(subsectorinfo_t) * numsubsectors;
#ifdef _DEBUG
    Con_Printf("R_InitSubsectorInfo: %i bytes.\n", i);
#endif
    subsecinfo = Z_Calloc(i, PU_LEVEL, NULL);

    Z_CheckHeap();

    for(i = 0, info = subsecinfo; i < numsubsectors; i++, info++)
    {
        sub = SUBSECTOR_PTR(i);
        sector = sub->sector;

        // Init floor plane.
        info->floor.isfloor = true;
        R_InitPlanePoly(&info->floor, false, sub);

        // Init ceiling plane.
        info->ceil.isfloor = false;
        R_InitPlanePoly(&info->ceil, true, sub);
    }

    Z_CheckHeap();
}

/*
 *  Mapinfo must be set.
 */
void R_SetupFog(void)
{
    int     flags;

    if(!mapinfo)
    {
        // Go with the defaults.
        Con_Execute(CMDS_DDAY,"fog off", true);
        return;
    }

    // Check the flags.
    flags = mapinfo->flags;
    if(flags & MIF_FOG)
    {
        // Setup fog.
        Con_Execute(CMDS_DDAY, "fog on", true);
        Con_Executef(CMDS_DDAY, true, "fog start %f", mapinfo->fog_start);
        Con_Executef(CMDS_DDAY, true, "fog end %f", mapinfo->fog_end);
        Con_Executef(CMDS_DDAY, true, "fog density %f", mapinfo->fog_density);
        Con_Executef(CMDS_DDAY, true, "fog color %.0f %.0f %.0f",
                     mapinfo->fog_color[0] * 255, mapinfo->fog_color[1] * 255,
                     mapinfo->fog_color[2] * 255);
    }
    else
    {
        Con_Execute(CMDS_DDAY, "fog off", true);
    }
}

/*
 * Scans all sectors for any supported DOOM.exe renderer hacks.
 * Updates sectorinfo accordingly.
 */
void R_RationalizeSectors(void)
{
    int     i, k;
    sectorinfo_t *info;
    sector_t *sec;
    line_t *lin;
    lineinfo_t *linfo;
    boolean selfRefHack;
    boolean unclosed;

    for(i = 0, info = secinfo; i < numsectors; ++i, info++)
    {
        sec = SECTOR_PTR(i);
        if(!sec->linecount)
            continue;

        // Detect self-referencing sectors.
        // NOTE: We need to find ALL the self-referencing "root" lines.
        selfRefHack = false;
        for(k = 0; k < sec->linecount && !selfRefHack; ++k)
        {
            lin = sec->Lines[k];
            linfo = LINE_INFO(lin);

            if(lin->frontsector && lin->backsector &&
               lin->frontsector == lin->backsector &&
               lin->backsector == sec)
            {
                vertexowner_t *ownerA, *ownerB;
                // The line properties indicate that this might be a
                // self-referencing, hack sector.

                // Make sure this line isn't isolated
                // (ie both vertexes arn't endpoints).
                ownerA = &vertexowners[GET_VERTEX_IDX(lin->v1)];
                ownerB = &vertexowners[GET_VERTEX_IDX(lin->v2)];
                if(!(ownerA->numlines == 1 && ownerB->numlines == 1))
                {
                    // Also, this line could split a sector and both ends
                    // COULD be vertexes that make up the sector outline.
                    // So, check all line owners of each vertex.

                    // Test simple case - single line dividing a sector
                    if(!(ownerA->num == 1 && ownerB->num == 1))
                    {
                        int j;
                        int count;
                        line_t *owner;
                        boolean ok = true;
                        boolean ok2 = true;

                        // Ok, need to check for neighbors.
                        // Test all the line owners to see that they arn't
                        // "real" twosided lines.
                        if(ownerA->num > 1)
                        {
                            count = 0;
                            for(j = 0; j < ownerA->numlines && ok; ++j)
                            {
                                owner = LINE_PTR(ownerA->linelist[j]);
                                if(owner != lin)
                                if((owner->frontsector == sec ||
                                    (owner->backsector && owner->backsector == sec)))
                                {
                                    ++count;
                                    if(count > 1)
                                        ok = false;
                                }
                            }
                        }

                        if(ok && ownerB->num > 1)
                        {
                            count = 0;
                            for(j = 0; j < ownerB->numlines && ok2; ++j)
                            {
                                owner = LINE_PTR(ownerB->linelist[j]);
                                if(owner != lin)
                                if((owner->frontsector == sec ||
                                    (owner->backsector && owner->backsector == sec)))
                                {
                                    ++count;
                                    if(count > 1)
                                        ok2 = false;
                                }
                            }
                        }

                        if(ok && ok2)
                        {
                            selfRefHack = true;
                            linfo->selfRefHackRoot = true;
                        }
                    }
                }
            }
        }

        if(selfRefHack)
            info->selfRefHack = true;

        // Detect unclosed sectors.
        unclosed = false;
        if(!(sec->linecount >= 3))
            unclosed = true;
        else
        {
            // TODO:
            // Add algorithm to check for unclosed sectors here.
            // Perhaps have a look at glBSP.
        }

        if(unclosed)
            info->unclosed = true;
    }
}

/*
 * Mapinfo must be set.
 */
void R_SetupSky(void)
{
    int     i, k;

    if(!mapinfo)
    {
        // Use the defaults.
        Rend_SkyParams(DD_SKY, DD_HEIGHT, .666667f);
        Rend_SkyParams(DD_SKY, DD_HORIZON, 0);
        Rend_SkyParams(0, DD_ENABLE, 0);
        Rend_SkyParams(0, DD_TEXTURE, R_TextureNumForName("SKY1"));
        Rend_SkyParams(0, DD_MASK, DD_NO);
        Rend_SkyParams(0, DD_OFFSET, 0);
        Rend_SkyParams(1, DD_DISABLE, 0);

        // There is no sky color.
        noSkyColorGiven = true;
        return;
    }

    Rend_SkyParams(DD_SKY, DD_HEIGHT, mapinfo->sky_height);
    Rend_SkyParams(DD_SKY, DD_HORIZON, mapinfo->horizon_offset);
    for(i = 0; i < 2; i++)
    {
        k = mapinfo->sky_layers[i].flags;
        if(k & SLF_ENABLED)
        {
            Rend_SkyParams(i, DD_ENABLE, 0);
            Rend_SkyParams(i, DD_TEXTURE,
                           R_TextureNumForName(mapinfo->sky_layers[i].
                                               texture));
            Rend_SkyParams(i, DD_MASK, k & SLF_MASKED ? DD_YES : DD_NO);
            Rend_SkyParams(i, DD_OFFSET, mapinfo->sky_layers[i].offset);
            Rend_SkyParams(i, DD_COLOR_LIMIT,
                           mapinfo->sky_layers[i].color_limit);
        }
        else
        {
            Rend_SkyParams(i, DD_DISABLE, 0);
        }
    }

    // Any sky models to setup? Models will override the normal
    // sphere.
    R_SetupSkyModels(mapinfo);

    // How about the sky color?
    noSkyColorGiven = true;
    for(i = 0; i < 3; i++)
    {
        skyColorRGB[i] = (byte) (255 * mapinfo->sky_color[i]);
        if(mapinfo->sky_color[i] > 0)
            noSkyColorGiven = false;
    }

    // Calculate a balancing factor, so the light in the non-skylit
    // sectors won't appear too bright.
    if(false &&
       (mapinfo->sky_color[0] > 0 || mapinfo->sky_color[1] > 0 ||
        mapinfo->sky_color[2] > 0))
    {
        skyColorBalance =
            (0 +
             (mapinfo->sky_color[0] * 2 + mapinfo->sky_color[1] * 3 +
              mapinfo->sky_color[2] * 2) / 7) / 1;
    }
    else
    {
        skyColorBalance = 1;
    }
}

/*
 * Returns pointers to the line's vertices in such a fashion that
 * verts[0] is the leftmost vertex and verts[1] is the rightmost
 * vertex, when the line lies at the edge of `sector.'
 */
void R_OrderVertices(line_t *line, const sector_t *sector, vertex_t *verts[2])
{
    if(sector == line->frontsector)
    {
        verts[0] = line->v1;
        verts[1] = line->v2;
    }
    else
    {
        verts[0] = line->v2;
        verts[1] = line->v1;
    }
}

/*
 * A neighbour is a line that shares a vertex with 'line', and faces
 * the specified sector.  Finds both the left and right neighbours.
 */
void R_FindLineNeighbors(sector_t *sector, line_t *line,
                         struct line_s **neighbors, int alignment)
{
    struct line_s *other;
    vertex_t *vtx[2];
    int     j;

    // We want to know which vertex is the leftmost/rightmost one.
    R_OrderVertices(line, sector, vtx);

    // Find the real neighbours, which are in the same sector
    // as this line.
    for(j = 0; j < sector->linecount; j++)
    {
        other = sector->Lines[j];
        if(other == line)
            continue;

        // Is this a valid neighbour?
        if(other->frontsector == sector && other->backsector == sector)
            continue;

        // Do we need to test the line alignment?
        if(alignment)
        {
#define SEP 10
            binangle_t diff = LINE_INFO(line)->angle - LINE_INFO(other)->angle;

            /*if(!(diff < SEP && diff > BANG_MAX - SEP) &&
               !(diff < BANG_180 + SEP && diff > BANG_180 - SEP))
               continue; // Misaligned. */

            if(alignment < 0)
                diff -= BANG_180;
            if(other->frontsector != sector)
                diff -= BANG_180;
            if(!(diff < SEP || diff > BANG_MAX - SEP))
                continue;       // Misaligned.
        }

        // It's our 'left' neighbour if it shares v1.
        if(other->v1 == vtx[0] || other->v2 == vtx[0])
            neighbors[0] = other;

        // It's our 'right' neighbour if it shares v2.
        if(other->v1 == vtx[1] || other->v2 == vtx[1])
            neighbors[1] = other;

        // Do we have everything we want?
        if(neighbors[0] && neighbors[1])
            break;
    }
}

static boolean R_IsEquivalent(line_t *a, line_t *b)
{
    return ((a->v1 == b->v1 && a->v2 == b->v2) ||
            (a->v1 == b->v2 && a->v2 == b->v1));
}

/*
 * Browse through the lines in backSector.  The backNeighbor is the
 * line that 1) isn't realNeighbor and 2) connects to commonVertex.
 */
static void R_FindBackNeighbor(sector_t *backSector, line_t *self,
                               line_t *realNeighbor, vertex_t *commonVertex,
                               line_t **backNeighbor)
{
    int     i;
    line_t *line;

    for(i = 0; i < backSector->linecount; i++)
    {
        line = backSector->Lines[i];
        if(R_IsEquivalent(line, realNeighbor) || R_IsEquivalent(line, self))
            continue;
        if(line->frontsector == line->backsector)
            continue;
        if(line->v1 == commonVertex || line->v2 == commonVertex)
        {
            *backNeighbor = line;
            return;
        }
    }
}

/*
 * Calculate accurate lengths for all lines.  Find line neighbours,
 * which will be used in the FakeRadio calculations.
 */
void R_InitLineInfo(void)
{
    line_t *line;
    sector_t *sector;
    int     i, k, j, m;
    lineinfo_t *info;
    lineinfo_side_t *side;
    vertexowner_t *owner;
    vertex_t *vertices[2];

    // Allocate memory for the line info.
    lineinfo = Z_Calloc(sizeof(lineinfo_t) * numlines, PU_LEVEL, NULL);

    // Calculate the accurate length of each line.
    for(i = 0, info = lineinfo; i < numlines; i++, info++)
    {
        line = LINE_PTR(i);
        info->length = P_AccurateDistance(line->dx, line->dy);
        info->angle = bamsAtan2(-(line->dx >> 13), line->dy >> 13);
    }

    // Find neighbours. We'll do this sector by sector.
    for(k = 0; k < numsectors; k++)
    {
        sector = SECTOR_PTR(k);
        for(i = 0; i < sector->linecount; i++)
        {
            line = sector->Lines[i];
            info = LINE_INFO(line);

            // Which side is this?
            side =
                (line->frontsector ==
                 sector ? &info->side[0] : &info->side[1]);

            R_FindLineNeighbors(sector, line, side->neighbor, 0);

            R_OrderVertices(line, sector, vertices);

            // Figure out the sectors in the proximity.
            for(j = 0; j < 2; j++)
            {
                // Neighbour must be two-sided.
                if(side->neighbor[j] && side->neighbor[j]->frontsector &&
                   side->neighbor[j]->backsector)
                {
                    side->proxsector[j] =
                        (side->neighbor[j]->frontsector ==
                         sector ? side->neighbor[j]->backsector : side->
                         neighbor[j]->frontsector);

                    // Find the backneighbour.  They are the
                    // neighbouring lines in the backsectors of the
                    // neighbour lines.
                    R_FindBackNeighbor(side->proxsector[j], line,
                                       side->neighbor[j], vertices[j],
                                       &side->backneighbor[j]);

                    /*assert(side->backneighbor[j] != line); */
                }
                else
                {
                    side->proxsector[j] = NULL;
                }
            }

            // Look for aligned neighbours.  They are side-specific.
            for(j = 0; j < 2; j++)
            {
                owner = vertexowners + GET_VERTEX_IDX(vertices[j]);
                for(m = 0; m < owner->num; m++)
                {
                    //if(owner->list[m] == k) continue;
                    R_FindLineNeighbors(SECTOR_PTR(owner->list[m]), line,
                                        side->alignneighbor,
                                        side == &info->side[0] ? 1 : -1);
                }
            }

            /*          // How about the other sector?
               if(!line->backsector || !line->frontsector)
               continue; // Single-sided.

               R_FindLineNeighbors(line->frontsector == sector?
               line->backsector : line->frontsector,
               line, info->backneighbor); */
        }
    }

#ifdef _DEBUG
    if(verbose >= 1)
    {
        for(i = 0; i < numlines; i++)
        {
            for(k = 0; k < 2; k++)
            {
                line = LINE_PTR(i);
                side = LINE_INFO(line)->side + k;
                if(side->alignneighbor[0] || side->alignneighbor[1])
                    Con_Printf("Line %i/%i: l=%i r=%i\n", i, k,
                               side->alignneighbor[0] ? GET_LINE_IDX(side->
                                                                     alignneighbor
                                                                     [0]) : -1,
                               side->alignneighbor[1] ? GET_LINE_IDX(side->
                                                                     alignneighbor
                                                                     [1]) :
                               -1);
            }
        }
    }
#endif
}

/*
 * This routine is called from the Game to polygonize the current
 * level.  Creates floors and ceilings and fixes the adjacent sky
 * sector heights.  Creates a big enough dlBlockLinks.  Reads mapinfo
 * and does the necessary setup.
 */
void R_SetupLevel(char *level_id, int flags)
{
    int     i;

    if(flags & DDSLF_INITIALIZE)
    {
        // A new level is about to be setup.
        levelSetup = true;

        // This is called before anything is actually done.
        if(loadInStartupMode)
            Con_StartupInit();
        return;
    }

    // First check for some special tasks.
    if(flags & DDSLF_INIT_LINKS)
    {
        // Init polyobj blockmap.
        P_InitPolyBlockMap();

        // Initialize node piles and line rings.
        NP_Init(&thingnodes, 256);  // Allocate a small pile.
        NP_Init(&linenodes, numlines + 1000);

        // Allocate the rings.
        linelinks = Z_Malloc(sizeof(*linelinks) * numlines, PU_LEVEL, 0);
        for(i = 0; i < numlines; i++)
            linelinks[i] = NP_New(&linenodes, NP_ROOT_NODE);
        return;                 // Do nothing else.
    }
    if(flags & DDSLF_SERVER_ONLY)
    {
        // Only init server data.
        Sv_InitPools();
        return;
    }
    if(flags & DDSLF_AFTER_LOADING)
    {
        // Loading a game usually destroys all thinkers. Until a proper
        // savegame system handled by the engine is introduced we'll have
        // to resort to re-initializing the most important stuff.
        P_SpawnTypeParticleGens();
        return;
    }
    if(flags & DDSLF_FINALIZE)
    {
        if(loadInStartupMode)
            Con_StartupDone();

        // Recalculate the light range mod matrix.
        Rend_CalcLightRangeModMatrix(NULL);

        // Run any commands specified in Map Info.
        if(mapinfo && mapinfo->execute)
            Con_Execute(CMDS_DED, mapinfo->execute, true);

        // The level setup has been completed.  Run the special level
        // setup command, which the user may alias to do something
        // useful.
        if(level_id && level_id[0])
        {
            char    cmd[80];

            sprintf(cmd, "init-%s", level_id);
            if(Con_IsValidCommand(cmd))
            {
                Con_Executef(CMDS_DED, false, cmd);
            }
        }

        // Clear any input events that might have accumulated during the
        // setup period.
        DD_ClearEvents();

        // Now that the setup is done, let's reset the tictimer so it'll
        // appear that no time has passed during the setup.
        DD_ResetTimer();

        // Kill all local commands.
        for(i = 0; i < MAXPLAYERS; i++)
        {
            clients[i].numTics = 0;
        }

        // Reset the level tick timer.
        levelTime = 0;

        // We've finished setting up the level
        levelSetup = false;

        // Inform the timing system to suspend the starting of the clock.
        firstFrameAfterLoad = true;
        return;
    }

    if(isServer)
    {
        // Whenever the map changes, remote players must tell us when
        // they're ready to begin receiving frames.
        for(i = 0; i < MAXPLAYERS; i++)
        {
            if(!(players[i].flags & DDPF_LOCAL) && clients[i].connected)
            {
#ifdef _DEBUG
                Con_Printf("Cl%i NOT READY ANY MORE!\n", i);
#endif
                clients[i].ready = false;
            }
        }
    }

    Con_InitProgress("Setting up level...", 100);
    strcpy(currentLevelId, level_id);

    // First compose the vertex owners array.
    R_InitVertexOwners();
    Con_Progress(10, 0);

    // Polygonize.
    if(flags & DDSLF_POLYGONIZE)
    {
        if(flags & DDSLF_DONT_CLIP)
            R_PolygonizeWithoutCarving();
        else
            R_CreateFloorsAndCeilings(numnodes - 1, 0, NULL);
    }
    Con_Progress(10, 0);

    // Init Particle Generator links.
    PG_InitForLevel();

    // Make sure subsector floors and ceilings will be rendered
    // correctly.
    R_SubsectorPlanes();

    // The map bounding box will be updated during sector info
    // initialization.
    memset(mapBounds, 0, sizeof(mapBounds));
    R_InitSectorInfo();

    R_InitSegInfo();
    R_InitSubsectorInfo();
    R_InitLineInfo();

    // Init blockmap for searching subsectors.
    P_InitSubsectorBlockMap();
    R_InitSectorShadows();

    R_RationalizeSectors();

    Con_Progress(10, 0);

    if(flags & DDSLF_FIX_SKY)
        R_SkyFix();
    if(flags & DDSLF_REVERB)
        S_CalcSectorReverbs();

    DL_InitLinks();

    Cl_Reset();
    RL_DeleteLists();
    GL_DeleteRawImages();
    Con_Progress(10, 0);

    // See what mapinfo says about this level.
    mapinfo = Def_GetMapInfo(level_id);
    if(!mapinfo)
        mapinfo = Def_GetMapInfo("*");
    // Setup accordingly.
    R_SetupFog();
    R_SetupSky();
    if(mapinfo)
    {
        mapgravity = mapinfo->gravity * FRACUNIT;
        r_ambient = mapinfo->ambient * 255;
    }
    else
    {
        // No map info found, set some basic stuff.
        mapgravity = FRACUNIT;
        r_ambient = 0;
    }

    // Invalidate old cmds.
    if(isServer)
    {
        for(i = 0; i < MAXPLAYERS; i++)
            if(players[i].ingame)
                clients[i].runTime = SECONDS_TO_TICKS(gameTime);
    }

    // Set target heights of all planes.
    for(i = 0; i < numsectors; i++)
    {
        sector_t *sec = SECTOR_PTR(i);

        sec->planes[PLN_FLOOR].target = sec->floorheight;
        sec->planes[PLN_CEILING].target = sec->ceilingheight;
    }

    // Spawn all type-triggered particle generators.
    // Let's hope there aren't too many...
    P_SpawnTypeParticleGens();
    P_SpawnMapParticleGens(level_id);

    // Make sure that the next frame doesn't use a filtered viewer.
    R_ResetViewer();

    // Texture animations should begin from their first step.
    R_ResetAnimGroups();

    // Do network init: Initialize Delta Pools.
    if(!(flags & DDSLF_NO_SERVER))
    {
        Sv_InitPools();
    }

    // Tell shadow bias to initialize the bias light sources.
    SB_InitForLevel(R_GetUniqueLevelID());

    // Initialize the lighting grid.
    LG_Init();

    Con_Progress(10, 0);        // 50%.
}

void R_ClearSectorFlags(void)
{
    int     i;
    sectorinfo_t *sinf;

    for(i = 0, sinf = secinfo; i < numsectors; i++, sinf++)
    {
        // Clear all flags that can be cleared before each frame.
        sinf->flags &= ~SIF_FRAME_CLEAR;
    }
}

sector_t *R_GetLinkedSector(sector_t *startsec, boolean getfloor)
{
    sector_t *sec = startsec;
    sector_t *link;
    sectorinfo_t *sin;

    for(;;)
    {
        sin = SECT_INFO(sec);
        if(getfloor)
        {
            if(!sin->linkedfloor)
                return sec;
            link = sin->linkedfloor;
        }
        else
        {
            if(!sin->linkedceil)
                return sec;
            link = sin->linkedceil;
        }
#if _DEBUG
        if(sec == link || startsec == link)
        {
            Con_Error("R_GetLinkedSector: linked to self! (%s)\n",
                      getfloor ? "flr" : "ceil");
            return startsec;
        }
#endif
        sec = link;
    }
}

/*
 * All links will be updated every frame (sectorheights may change at
 * any time without notice).
 */
void R_UpdatePlanes(void)
{
    int     i, j;
    int setFloorGlow, setCeilingGlow;
    sector_t *sec;
    sectorinfo_t *sin;

    // Clear all non-permanent sector links.
    for(i = 0, sin = secinfo; i < numsectors; i++, sin++)
    {
        if(sin->permanentlink)
            continue;
        sin->linkedfloor = sin->linkedceil = NULL;

        // Check if there are any lightlevel or color changes.
        sec = SECTOR_PTR(i);
        if(sec->lightlevel != sin->oldlightlevel ||
           sec->rgb[0] != sin->oldrgb[0] ||
           sec->rgb[1] != sin->oldrgb[1] ||
           sec->rgb[2] != sin->oldrgb[2])
        {
            sin->flags |= SIF_LIGHT_CHANGED;
            sin->oldlightlevel = sec->lightlevel;
            memcpy(sin->oldrgb, sec->rgb, 3);

            LG_SectorChanged(sec, sin);
        }
        else
        {
            sin->flags &= ~SIF_LIGHT_CHANGED;
        }

        // Any changes to surface colours?
        // TODO: when surface colours are intergrated with the
        // bias lighting model we will need to recalculate the
        // vertex colours when they are changed.
        if(sec->floorrgb[0] != sin->oldfloorrgb[0] ||
           sec->floorrgb[1] != sin->oldfloorrgb[1] ||
           sec->floorrgb[2] != sin->oldfloorrgb[2] ||
           sec->ceilingrgb[0] != sin->oldceilingrgb[0] ||
           sec->ceilingrgb[1] != sin->oldceilingrgb[1] ||
           sec->ceilingrgb[2] != sin->oldceilingrgb[2] )
        {
            sin->flags |= SIF_PLANE_COLOR_CHANGED;
            memcpy(sin->oldfloorrgb, sec->floorrgb, 3);
            memcpy(sin->oldceilingrgb, sec->ceilingrgb, 3);
        }
        else
        {
            sin->flags &= ~SIF_PLANE_COLOR_CHANGED;
        }

        // Any change to the floor texture or glow properties?
        // TODO: Implement Decoration{ Glow{}} definitions.
        setFloorGlow = 0;
        // The order of these tests is important.
        if(sec->floorpic != sin->oldfloorpic)
        {
            // Check if the new texture is declared as glowing.
            // NOTE: Currently, we always discard the glow settings of the
            //       previous flat after a texture change.

            // Now that glows are properties of the sector this does not
            // need to be the case. If we expose these properties via DMU
            // they could be changed at any time. However in order to support
            // flats that are declared as glowing we would need some method
            // of telling Doomsday IF we want to inherit these properties when
            // the plane flat changes...
            if(R_FlatFlags(sec->floorpic) & TXF_GLOW)
            {
                // The new texture is glowing.

                // Default height factor is 4
                sec->floorglow = 4;

                // Always use the average colour.
                GL_GetFlatColor(sec->floorpic, sec->floorglowrgb);

                // Do we need to update the plane glow flags?
                if(sin->oldfloorpic)
                {
                    if(!(R_FlatFlags(sin->oldfloorpic) & TXF_GLOW))
                        setFloorGlow = 1; // Turn the subsector plane glow flags on
                }
                else
                    setFloorGlow = 1;
            }
            else if(sin->oldfloorpic && (R_FlatFlags(sin->oldfloorpic) & TXF_GLOW))
            {
                // The old texture was glowing but the new one is not.
                // Clear the glow properties for this plane.
                sec->floorglow = 0;
                memset(sec->floorglowrgb, 0, 3);

                setFloorGlow = -1; // Turn the subsector plane glow flags off
            }

            sin->oldfloorpic = sec->floorpic;
        }
        else if((R_FlatFlags(sec->floorpic) & TXF_GLOW) != (sec->floorglow != 0))
        {
            // The glow property of the current flat has been changed
            // since last update.

            // NOTE:
            // This approach is hardly optimal but since flats will
            // rarely/if ever have this property changed during normal
            // gameplay (the RENDER_GLOWFLATS text string is depreciated and
            // the only time this property might change is after a console
            // RESET) so it doesn't matter.
            if(!(R_FlatFlags(sec->floorpic) & TXF_GLOW) && sec->floorglow != 0)
            {
                // The current flat is no longer glowing
                sec->floorglow = 0;
                memset(sec->floorglowrgb, 0, 3);
                setFloorGlow = -1; // Turn the subsector plane glow flags off
            }
            else if((R_FlatFlags(sec->floorpic) & TXF_GLOW) && sec->floorglow == 0)
            {
                // The current flat is now glowing
                sec->floorglow = 4;
                GL_GetFlatColor(sec->floorpic, sec->floorglowrgb);
                setFloorGlow = 1; // Turn the subsector plane glow flags on
            }
        }

        // Same as above but for ceilings
        setCeilingGlow = 0;
        if(sec->ceilingpic != sin->oldceilingpic)
        {
            if(R_FlatFlags(sec->ceilingpic) & TXF_GLOW)
            {
                sec->ceilingglow = 4;
                GL_GetFlatColor(sec->ceilingpic, sec->ceilingglowrgb);

                if(sin->oldceilingpic)
                {
                    if(!(R_FlatFlags(sin->oldceilingpic) & TXF_GLOW))
                        setCeilingGlow = 1;
                }
                else
                    setCeilingGlow = 1;
            }
            else if(sin->oldceilingpic && (R_FlatFlags(sin->oldceilingpic) & TXF_GLOW))
            {
                sec->ceilingglow = 0;
                memset(sec->ceilingglowrgb, 0, 3);
                setCeilingGlow = -1;
            }
            sin->oldceilingpic = sec->ceilingpic;
        }
        else if((R_FlatFlags(sec->ceilingpic) & TXF_GLOW) != (sec->ceilingglow != 0))
        {
            if(!(R_FlatFlags(sec->ceilingpic) & TXF_GLOW) && sec->ceilingglow != 0)
            {
                sec->ceilingglow = 0;
                memset(sec->ceilingglowrgb, 0, 3);
                setCeilingGlow = -1;
            }
            else if((R_FlatFlags(sec->ceilingpic) & TXF_GLOW) && sec->ceilingglow == 0)
            {
                sec->ceilingglow = 4;
                GL_GetFlatColor(sec->ceilingpic, sec->ceilingglowrgb);
                setCeilingGlow = 1;
            }
        }

        // Do we need to update the subsector plane glow flags?
        if(setFloorGlow != 0 || setCeilingGlow != 0)
        {
            // FIXME: Find a better way to find the subsectors of a sector.
            for(j = 0; j < numsubsectors; ++j)
            {
                subsector_t *sub = SUBSECTOR_PTR(j);
                subsectorinfo_t *subInfo = SUBSECT_INFO(sub);

                // Only the subsectors of the changed sector.
                if(sub->sector != sec)
                    continue;

                // Update the floor glow flag?
                if(setFloorGlow == 1)
                    subInfo->floor.flags |= RPF_GLOW;
                else if(setFloorGlow == -1)
                    subInfo->floor.flags &= ~RPF_GLOW;

                // Update the ceiling glow flag?
                if(setCeilingGlow == 1)
                    subInfo->ceil.flags |= RPF_GLOW;
                else if(setCeilingGlow == -1)
                    subInfo->ceil.flags &= ~RPF_GLOW;
            }
        }
    }

    // Assign new links.
    for(i = 0, sin = secinfo; i < numsectors; i++, sin++)
    {
        sec = SECTOR_PTR(i);
        R_SetSectorLinks(sec);

        // Floor height.
        if(!sin->linkedfloor)
        {
            sin->visfloor = FIX2FLT(sec->floorheight) + sin->visflooroffset;
        }
        else
        {
            sin->visfloor =
                FIX2FLT(R_GetLinkedSector(sin->linkedfloor, true)->
                        floorheight);
        }

        // Ceiling height.
        if(!sin->linkedceil)
        {
            sin->visceil = FIX2FLT(sec->ceilingheight) + sin->visceiloffset;
        }
        else
        {
            sin->visceil =
                FIX2FLT(R_GetLinkedSector(sin->linkedceil, false)->
                        ceilingheight);
        }
    }
}

/*
 * This ID is the name of the lump tag that marks the beginning of map
 * data, e.g. "MAP03" or "E2M8".
 */
const char *R_GetCurrentLevelID(void)
{
    return currentLevelId;
}

/*
 * Return the 'unique' identifier of the map.  This identifier
 * contains information about the map tag (E3M3), the WAD that
 * contains the map (DOOM.IWAD), and the game mode (doom-ultimate).
 *
 * The entire ID string will be in lowercase letters.
 */
const char *R_GetUniqueLevelID(void)
{
    static char uid[256];
    filename_t base;
    int lump = W_GetNumForName((char*)R_GetCurrentLevelID());

    M_ExtractFileBase(W_LumpSourceFile(lump), base);

    sprintf(uid, "%s|%s|%s|%s",
            R_GetCurrentLevelID(),
            base, (W_IsFromIWAD(lump) ? "iwad" : "pwad"),
            gx.Get(DD_GAME_MODE));

    strlwr(uid);
    return uid;
}

/*
 * Sector light color may be affected by the sky light color.
 */
const byte *R_GetSectorLightColor(sector_t *sector)
{
    sector_t *src;
    int     i;

    if(!rendSkyLight || noSkyColorGiven)
        return sector->rgb;     // The sector's real color.

    if(sector->ceilingpic != skyflatnum && sector->floorpic != skyflatnum)
    {
        // A dominant light source affects this sector?
        src = SECT_INFO(sector)->lightsource;
        if(src && src->lightlevel >= sector->lightlevel)
        {
            // The color shines here, too.
            return R_GetSectorLightColor(src);
        }

        // Return the sector's real color (balanced against sky's).
        if(skyColorBalance >= 1)
        {
            return sector->rgb;
        }
        else
        {
            for(i = 0; i < 3; i++)
                balancedRGB[i] = (byte) (sector->rgb[i] * skyColorBalance);
            return balancedRGB;
        }
    }
    // Return the sky color.
    return skyColorRGB;
}

/*
 * Calculate the size of the entire map.
 */
void R_GetMapSize(vertex_t *min, vertex_t *max)
{
/*    byte *ptr;
    int i;
    fixed_t x;
    fixed_t y;

    memcpy(min, vertexes, sizeof(min));
    memcpy(max, vertexes, sizeof(max));

    for(i = 1, ptr = vertexes + VTXSIZE; i < numvertexes; i++, ptr += VTXSIZE)
    {
        x = ((vertex_t *) ptr)->x;
        y = ((vertex_t *) ptr)->y;

        if(x < min->x)
            min->x = x;
        if(x > max->x)
            max->x = x;
        if(y < min->y)
            min->y = y;
        if(y > max->y)
            max->y = y;
            }*/

    min->x = FRACUNIT * mapBounds[BLEFT];
    min->y = FRACUNIT * mapBounds[BTOP];

    max->x = FRACUNIT * mapBounds[BRIGHT];
    max->y = FRACUNIT * mapBounds[BBOTTOM];
}
