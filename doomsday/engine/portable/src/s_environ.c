/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * s_environ.c: Environmental Sound Effects
 *
 * Calculation of the aural properties of sectors.
 */

// HEADER FILES ------------------------------------------------------------


#include <ctype.h>
#include <string.h>

#include "de_base.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_audio.h"

#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    const char  name[9];    // Material type name.
    int         volumeMul;
    int         decayMul;
    int         dampingMul;
} materialenvinfo_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static materialenvinfo_t matInfo[NUM_MATERIAL_ENV_CLASSES] = {
    {"Metal",     255,     255,    25},
    {"Rock",      200,     160,    100},
    {"Wood",      80,      50,     200},
    {"Cloth",     5,       5,      255}
};

static ownernode_t *unusedNodeList = NULL;

// CODE --------------------------------------------------------------------

/**
 * Given a texture/flat name, look up the associated material type.
 *
 * @param name          Name of the texture/flat to look up.
 * @param mnamespace    Material namespace (MG_* e.g. MN_FLATS).
 *
 * @return              If found; material type associated to the texture,
 *                      else @c MEC_UNKNOWN.
 */
material_env_class_t S_MaterialClassForName(const char* name,
                                            material_namespace_t mnamespace)
{
    int                 i;
    ded_tenviron_t*     env;

    for(i = 0, env = defs.textureEnv; i < defs.count.textureEnv.num; ++i, env++)
    {
        int                 j;

        for(j = 0; j < env->count.num; ++j)
        {
            ded_materialid_t*   mid = &env->materials[j];

            if(mid->mnamespace == mnamespace && !stricmp(mid->name, name))
            {   // A match!
                material_env_class_t     k;

                // See if we recognise the material name.
                for(k = 0; k < NUM_MATERIAL_ENV_CLASSES; ++k)
                    if(!stricmp(env->id, matInfo[k].name))
                        return k;

                return MEC_UNKNOWN;
            }
        }
    }

    return MEC_UNKNOWN;
}

static ownernode_t* newOwnerNode(void)
{
    ownernode_t*        node;

    if(unusedNodeList)
    {   // An existing node is available for re-use.
        node = unusedNodeList;
        unusedNodeList = unusedNodeList->next;

        node->next = NULL;
        node->data = NULL;
    }
    else
    {   // Need to allocate another.
        node = M_Malloc(sizeof(ownernode_t));
    }

    return node;
}

static void setSectorOwner(ownerlist_t* ownerList, face_t* face)
{
    ownernode_t*        node;

    if(!face)
        return;

    // Add a new owner.
    // NOTE: No need to check for duplicates.
    ownerList->count++;

    node = newOwnerNode();
    node->data = face;
    node->next = ownerList->head;
    ownerList->head = node;
}

static void findSSecsAffectingSector(gamemap_t* map, uint secIDX)
{
    uint                i;
    ownernode_t*        node, *p;
    float               bbox[4];
    ownerlist_t         faceOwnerList;
    sector_t*           sec = &map->sectors[secIDX];

    memset(&faceOwnerList, 0, sizeof(faceOwnerList));

    memcpy(bbox, sec->bBox, sizeof(bbox));
    bbox[BOXLEFT]   -= 128;
    bbox[BOXRIGHT]  += 128;
    bbox[BOXTOP]    += 128;
    bbox[BOXBOTTOM] -= 128;
/*
#if _DEBUG
Con_Message("sector %i: (%f,%f) - (%f,%f)\n", c,
            bbox[BOXLEFT], bbox[BOXTOP], bbox[BOXRIGHT], bbox[BOXBOTTOM]);
#endif
*/
    for(i = 0; i < map->numFaces; ++i)
    {
        face_t*             face = &map->faces[i];
        subsector_t*        ssec = (subsector_t*) face->data;

        // Is this subsector close enough?
        if(ssec->sector == sec || // subsector is IN this sector
           (ssec->midPoint.pos[VX] > bbox[BOXLEFT] &&
            ssec->midPoint.pos[VX] < bbox[BOXRIGHT] &&
            ssec->midPoint.pos[VY] < bbox[BOXTOP] &&
            ssec->midPoint.pos[VY] > bbox[BOXBOTTOM]))
        {
            // It will contribute to the reverb settings of this sector.
            setSectorOwner(&faceOwnerList, face);
        }
    }

    // Now harden the list.
    sec->numReverbFaceAttributors = faceOwnerList.count;
    if(sec->numReverbFaceAttributors)
    {
        face_t**            ptr;

        sec->reverbFaces =
            Z_Malloc((sec->numReverbFaceAttributors + 1) * sizeof(face_t*),
                     PU_MAPSTATIC, 0);

        for(i = 0, ptr = sec->reverbFaces, node = faceOwnerList.head;
            i < sec->numReverbFaceAttributors; ++i, ptr++)
        {
            p = node->next;
            *ptr = (face_t*) node->data;

            if(i < numSectors - 1)
            {   // Move this node to the unused list for re-use.
                node->next = unusedNodeList;
                unusedNodeList = node;
            }
            else
            {   // No further use for the nodes.
                M_Free(node);
            }
            node = p;
        }
        *ptr = NULL; // terminate.
    }
}

/**
 * Called during map init to determine which subsectors affect the reverb
 * properties of all sectors. Given that subsectors do not change shape (in
 * two dimensions at least), they do not move and are not created/destroyed
 * once the map has been loaded; this step can be pre-processed.
 */
void S_DetermineSubSecsAffectingSectorReverb(gamemap_t* map)
{
    uint                startTime = Sys_GetRealTime();

    uint                i;
    ownernode_t*        node, *p;

    for(i = 0; i < map->numSectors; ++i)
    {
        findSSecsAffectingSector(map, i);
    }

    // Free any nodes left in the unused list.
    node = unusedNodeList;
    while(node)
    {
        p = node->next;
        M_Free(node);
        node = p;
    }
    unusedNodeList = NULL;

    // How much time did we spend?
    VERBOSE(Con_Message
            ("S_DetermineSubSecsAffectingSectorReverb: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}

static boolean calcSSecReverb(face_t* face)
{
    uint                i, v;
    hedge_t*            ptr;
    subsector_t*        ssec = (subsector_t*) face->data;
    float               total = 0;
    material_env_class_t mclass;
    float               materials[NUM_MATERIAL_ENV_CLASSES];

    if(!ssec->sector)
    {
        ssec->reverb[SRD_SPACE] = ssec->reverb[SRD_VOLUME] =
            ssec->reverb[SRD_DECAY] = ssec->reverb[SRD_DAMPING] = 0;
        return false;
    }

    memset(&materials, 0, sizeof(materials));

    // Space is the rough volume of the subsector (bounding box).
    ssec->reverb[SRD_SPACE] =
        (int) (ssec->sector->SP_ceilheight - ssec->sector->SP_floorheight) *
        (ssec->bBox[1].pos[VX] - ssec->bBox[0].pos[VX]) *
        (ssec->bBox[1].pos[VY] - ssec->bBox[0].pos[VY]);

    // The other reverb properties can be found out by taking a look at the
    // materials of all surfaces in the subsector.
    if((ptr = face->hEdge))
    {
        do
        {
            hedge_t*            hEdge = ptr;
            seg_t*              seg = (seg_t*) (ptr)->data;

            if(seg->lineDef && seg->sideDef &&
               seg->sideDef->SW_middlematerial)
            {
                material_t*         mat = seg->sideDef->SW_middlematerial;

                // The material determines its type.
                mclass = Material_GetEnvClass(mat);
                total += seg->length;
                if(!(mclass >= 0 && mclass < NUM_MATERIAL_ENV_CLASSES))
                    mclass = MEC_WOOD; // Assume it's wood if unknown.
                materials[mclass] += seg->length;
            }
        } while((ptr = ptr->next) != face->hEdge);
    }

    if(!total)
    {   // Huh?
        ssec->reverb[SRD_VOLUME] = ssec->reverb[SRD_DECAY] =
            ssec->reverb[SRD_DAMPING] = 0;
        return false;
    }

    // Average the results.
    for(i = 0; i < NUM_MATERIAL_ENV_CLASSES; ++i)
        materials[i] /= total;

    // Volume.
    for(i = 0, v = 0; i < NUM_MATERIAL_ENV_CLASSES; ++i)
        v += materials[i] * matInfo[i].volumeMul;
    if(v > 255)
        v = 255;
    ssec->reverb[SRD_VOLUME] = v;

    // Decay time.
    for(i = 0, v = 0; i < NUM_MATERIAL_ENV_CLASSES; ++i)
        v += materials[i] * matInfo[i].decayMul;
    if(v > 255)
        v = 255;
    ssec->reverb[SRD_DECAY] = v;

    // High frequency damping.
    for(i = 0, v = 0; i < NUM_MATERIAL_ENV_CLASSES; ++i)
        v += materials[i] * matInfo[i].dampingMul;
    if(v > 255)
        v = 255;
    ssec->reverb[SRD_DAMPING] = v;

/*
#if _DEBUG
Con_Message("ssec %04i: vol:%3i sp:%3i dec:%3i dam:%3i\n",
            GET_SUBSECTOR_IDX(ssec), ssec->reverb[SRD_VOLUME],
            ssec->reverb[SRD_SPACE], ssec->reverb[SRD_DECAY],
            ssec->reverb[SRD_DAMPING]);
#endif
*/
    return true;
}

/**
 * Re-calculate the reverb properties of the given sector. Should be called
 * whenever any of the properties governing reverb properties have changed
 * (i.e. seg/plane texture or plane height changes).
 *
 * PRE: Subsector attributors must have been determined first.
 *
 * @param sec           Ptr to the sector to calculate reverb properties of.
 */
void S_CalcSectorReverb(sector_t* sec)
{
    uint                i, sectorSpace;
    float               spaceScatter;

    if(!sec)
        return; // Wha?

    sectorSpace = (int) (sec->SP_ceilheight - sec->SP_floorheight) *
        (sec->bBox[BOXRIGHT] - sec->bBox[BOXLEFT]) *
        (sec->bBox[BOXTOP] - sec->bBox[BOXBOTTOM]);
/*
#if _DEBUG
Con_Message("sector %i: secsp:%i\n", c, sectorSpace);
#endif
*/
    for(i = 0; i < sec->numReverbFaceAttributors; ++i)
    {
        face_t*             face = sec->reverbFaces[i];

        if(calcSSecReverb(face))
        {
            const subsector_t*  ssec = (subsector_t*) face->data;

            sec->reverb[SRD_SPACE]   += ssec->reverb[SRD_SPACE];

            sec->reverb[SRD_VOLUME]  +=
                ssec->reverb[SRD_VOLUME]  / 255.0f * ssec->reverb[SRD_SPACE];
            sec->reverb[SRD_DECAY]   +=
                ssec->reverb[SRD_DECAY]   / 255.0f * ssec->reverb[SRD_SPACE];
            sec->reverb[SRD_DAMPING] +=
                ssec->reverb[SRD_DAMPING] / 255.0f * ssec->reverb[SRD_SPACE];
        }
    }

    if(sec->reverb[SRD_SPACE])
    {
        spaceScatter = sectorSpace / sec->reverb[SRD_SPACE];
        // These three are weighted by the space.
        sec->reverb[SRD_VOLUME]  /= sec->reverb[SRD_SPACE];
        sec->reverb[SRD_DECAY]   /= sec->reverb[SRD_SPACE];
        sec->reverb[SRD_DAMPING] /= sec->reverb[SRD_SPACE];
    }
    else
    {
        spaceScatter = 0;
        sec->reverb[SRD_VOLUME]  = .2f;
        sec->reverb[SRD_DECAY]   = .4f;
        sec->reverb[SRD_DAMPING] = 1;
    }

    // If the space is scattered, the reverb effect lessens.
    sec->reverb[SRD_SPACE] /=
        (spaceScatter > .8 ? 10 : spaceScatter > .6 ? 4 : 1);

    // Normalize the reverb space [0...1].
    // 0= very small
    // .99= very large
    // 1.0= only for open areas (special case).
    sec->reverb[SRD_SPACE] /= 120e6;
    if(sec->reverb[SRD_SPACE] > .99)
        sec->reverb[SRD_SPACE] = .99f;

    if(R_IsSkySurface(&sec->SP_ceilsurface) ||
       R_IsSkySurface(&sec->SP_floorsurface))
    {   // An "open" sector.
        // It can still be small, in which case; reverb is diminished a bit.
        if(sec->reverb[SRD_SPACE] > .5)
            sec->reverb[SRD_VOLUME] = 1; // Full volume.
        else
            sec->reverb[SRD_VOLUME] = .5f; // Small, but still open.

        sec->reverb[SRD_SPACE] = 1;
    }
    else
    {   // A "closed" sector.
        // Large spaces have automatically a bit more audible reverb.
        sec->reverb[SRD_VOLUME] += sec->reverb[SRD_SPACE] / 4;
    }

    if(sec->reverb[SRD_VOLUME] > 1)
        sec->reverb[SRD_VOLUME] = 1;
}
