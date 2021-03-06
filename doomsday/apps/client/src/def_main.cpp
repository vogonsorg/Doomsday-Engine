/** @file def_main.cpp  Definition subsystem.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#define DE_NO_API_MACROS_DEFINITIONS

#include "def_main.h"
#include "dd_main.h"
#include "dd_def.h"
#include "api_def.h"
#include "api_sound.h"

#ifdef __CLIENT__
#  include "world/generator.h"
#  include "render/rend_particle.h"
#  include "resource/lightmaterialdecoration.h"
#endif

#include <doomsday/console/cmd.h>
#include <doomsday/defs/decoration.h>
#include <doomsday/defs/dedfile.h>
#include <doomsday/defs/dedparser.h>
#include <doomsday/defs/material.h>
#include <doomsday/defs/sky.h>
#include <doomsday/defs/state.h>
#include <doomsday/doomsdayapp.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>
#include <doomsday/manifest.h>
#include <doomsday/res/bundles.h>
#include <doomsday/res/doomsdaypackage.h>
#include <doomsday/res/textures.h>
#include <doomsday/res/animgroups.h>
#include <doomsday/world/materialmanifest.h>
#include <doomsday/world/materials.h>
#include <doomsday/world/detailtexturemateriallayer.h>
#include <doomsday/world/shinetexturemateriallayer.h>
#include <doomsday/world/texturemateriallayer.h>
#include <doomsday/world/xg.h>
#include <de/keymap.h>

#include <de/legacy/findfile.h>
#include <de/c_wrapper.h>
#include <de/app.h>
#include <de/packageloader.h>
#include <de/dscript.h>
#include <de/nativepath.h>

#include <cwctype>
#include <cstring>

using namespace de;
using namespace res;

#define LOOPi(n)    for (i = 0; i < (n); ++i)
#define LOOPk(n)    for (k = 0; k < (n); ++k)

static bool defsInited;
static mobjinfo_t *gettingFor;
static Binder *defsBinder;

static inline FS1 &fileSys()
{
    return App_FileSystem();
}

static Value *Function_Defs_GetSoundNum(Context &, const Function::ArgumentValues &args)
{
    return new NumberValue(DED_Definitions()->getSoundNum(args.at(0)->asText()));
}

void Def_Init()
{
    ::runtimeDefs.clear();
    DED_Definitions()->clear();

    auto &defs = *DED_Definitions();

    // Make the definitions visible in the global namespace.
    if (!defsBinder)
    {
        auto &scr = ScriptSystem::get();
        scr.addNativeModule("Defs", defs.names);

        /// TODO: Add a DEDRegister for sounds so this lookup is not needed and can be converted
        /// to a utility script function.
        defsBinder = new Binder;
        defsBinder->init(defs.names)
            << DE_FUNC(Defs_GetSoundNum, "getSoundNum", "name");
    }

    // Constants for definitions.
    DE_ADD_NUMBER_CONSTANT(defs.names, SN_SPAWN);
    DE_ADD_NUMBER_CONSTANT(defs.names, SN_SEE);
    DE_ADD_NUMBER_CONSTANT(defs.names, SN_PAIN);
    DE_ADD_NUMBER_CONSTANT(defs.names, SN_MELEE);
    DE_ADD_NUMBER_CONSTANT(defs.names, SN_MISSILE);
    DE_ADD_NUMBER_CONSTANT(defs.names, SN_CRASH);
    DE_ADD_NUMBER_CONSTANT(defs.names, SN_DEATH);
    DE_ADD_NUMBER_CONSTANT(defs.names, SN_XDEATH);
    DE_ADD_NUMBER_CONSTANT(defs.names, SN_RAISE);

    DE_ADD_NUMBER_CONSTANT(defs.names, SDN_ACTIVE);
    DE_ADD_NUMBER_CONSTANT(defs.names, SDN_ATTACK);
    DE_ADD_NUMBER_CONSTANT(defs.names, SDN_DEATH);
    DE_ADD_NUMBER_CONSTANT(defs.names, SDN_PAIN);
    DE_ADD_NUMBER_CONSTANT(defs.names, SDN_SEE);
}

void Def_Destroy()
{
    delete defsBinder;
    defsBinder = nullptr;

    App::app().scriptSystem().removeNativeModule("Defs");

    DED_Definitions()->clear();

    // Destroy the databases.
    ::runtimeDefs.clear();
    DED_DestroyDefinitions();

    ::defsInited = false;
}

state_t *Def_GetState(int num)
{
    if (num >= 0 && num < DED_Definitions()->states.size())
    {
        return &::runtimeDefs.states[num];
    }
    return nullptr;  // Not found.
}

sfxinfo_t *Def_GetSoundInfo(int soundId, float *freq, float *volume)
{
    if (soundId <= 0 || soundId >= DED_Definitions()->sounds.size())
        return nullptr;

    float dummy = 0;
    if (!freq)   freq   = &dummy;
    if (!volume) volume = &dummy;

    // Traverse all links when getting the definition. (But only up to 10, which is
    // certainly enough and prevents endless recursion.) Update the sound id at the
    // same time. The links were checked in Def_Read() so there cannot be any bogus
    // ones.
    sfxinfo_t *info = &::runtimeDefs.sounds[soundId];

    for (int i = 0; info->link && i < 10;
        info     = info->link,
        *freq    = (info->linkPitch > 0    ? info->linkPitch  / 128.0f : *freq),
        *volume += (info->linkVolume != -1 ? info->linkVolume / 127.0f : 0),
        soundId  = ::runtimeDefs.sounds.indexOf(info),
       ++i)
    {}

    DE_ASSERT(soundId < DED_Definitions()->sounds.size());

    return info;
}

bool Def_SoundIsRepeating(int soundId)
{
    if (sfxinfo_t *info = Def_GetSoundInfo(soundId, nullptr, nullptr))
    {
        return (info->flags & SF_REPEAT) != 0;
    }
    return false;
}

ded_compositefont_t *Def_GetCompositeFont(const char *uri)
{
    return DED_Definitions()->getCompositeFont(uri);
}

/// @todo $revise-texture-animation
static ded_reflection_t *tryFindReflection(const res::Uri &uri, /* bool hasExternal,*/ bool isCustom)
{
    for (int i = DED_Definitions()->reflections.size() - 1; i >= 0; i--)
    {
        ded_reflection_t &def = DED_Definitions()->reflections[i];
        if (!def.material || *def.material !=  uri) continue;

        /*
        if (hasExternal)
        {
            if (!(def.flags & REFF_EXTERNAL)) continue;
        }
        else */
        if (!isCustom)
        {
            if (def.flags & REFF_NO_IWAD) continue;
        }
        else
        {
            if (!(def.flags & REFF_PWAD)) continue;
        }

        return &def;
    }
    return nullptr;  // Not found.
}

/// @todo $revise-texture-animation
static ded_detailtexture_t *tryFindDetailTexture(const res::Uri &uri, /*bool hasExternal,*/ bool isCustom)
{
    for (int i = DED_Definitions()->details.size() - 1; i >= 0; i--)
    {
        ded_detailtexture_t &def = DED_Definitions()->details[i];
        for (int k = 0; k < 2; ++k)
        {
            const res::Uri *matUri = (k == 0 ? def.material1 : def.material2);
            if (!matUri || *matUri != uri) continue;

            /*if (hasExternal)
            {
                if (!(def.flags & DTLF_EXTERNAL)) continue;
            }
            else*/
            if (!isCustom)
            {
                if (def.flags & DTLF_NO_IWAD) continue;
            }
            else
            {
                if (!(def.flags & DTLF_PWAD)) continue;
            }

            return &def;
        }
    }
    return nullptr;  // Not found.
}

ded_ptcgen_t *Def_GetGenerator(const res::Uri &uri)
{
    if (uri.isEmpty()) return nullptr;

    for (int i = 0; i < DED_Definitions()->ptcGens.size(); ++i)
    {
        ded_ptcgen_t *def = &DED_Definitions()->ptcGens[i];
        if (!def->material) continue;

        // Is this suitable?
        if (*def->material == uri)
            return def;

#if 0  /// @todo $revise-texture-animation
        if (def->flags & PGF_GROUP)
        {
            // Generator triggered by all materials in the (animation) group.
            // A search is necessary only if we know both the used material and
            // the specified material in this definition are in *a* group.
            if (Material_IsGroupAnimated(defMat) && Material_IsGroupAnimated(mat) &&
               &Material_AnimGroup(defMat) == &Material_AnimGroup(mat))
            {
                // Both are in this group! This def will do.
                return def;
            }
        }
#endif
    }

    return nullptr;  // None found.
}

ded_ptcgen_t *Def_GetGenerator(const uri_s *uri)
{
    if (!uri) return nullptr;
    return Def_GetGenerator(reinterpret_cast<const res::Uri &>(*uri));
}

ded_ptcgen_t *Def_GetDamageGenerator(int mobjType)
{
    // Search for a suitable definition.
    for (int i = 0; i < DED_Definitions()->ptcGens.size(); ++i)
    {
        ded_ptcgen_t *def = &DED_Definitions()->ptcGens[i];

        // It must be for this type of mobj.
        if (def->damageNum == mobjType)
            return def;
    }
    return nullptr;
}

/**
 * The following escape sequences are un-escaped:
 * <pre>
 *     \\n   Newline
 *     \\r   Carriage return
 *     \\t   Tab
 *     \\\_   Space
 *     \\s   Space
 * </pre>
 */
static void Def_InitTextDef(ddtext_t *txt, const char *str)
{
    DE_ASSERT(txt);

    // Handle null pointers with "".
    if (!str) str = "";

    txt->text = (char *) M_Calloc(strlen(str) + 1);

    const char *in = str;
    char *out = txt->text;
    for (; *in; out++, in++)
    {
        if (*in == '\\')
        {
            in++;

            if (*in == 'n')      *out = '\n'; // Newline.
            else if (*in == 'r') *out = '\r'; // Carriage return.
            else if (*in == 't') *out = '\t'; // Tab.
            else if (*in == '_'
                 || *in == 's') *out = ' '; // Space.
            else
            {
                *out = *in;
            }
        }
        else
        {
            *out = *in;
        }
    }

    // Adjust buffer to fix exactly.
    txt->text = (char *) M_Realloc(txt->text, strlen(txt->text) + 1);
}

/**
 * Prints a count with a 2-space indentation.
 */
static String defCountMsg(int count, const String &label)
{
    if (!DoomsdayApp::verbose && !count)
        return "";  // Don't print zeros if not verbose.

    return Stringf(_E(Ta) "  %i " _E(Tb) "%s\n", count, label.c_str());
}

/**
 * Read all DD_DEFNS lumps in the primary lump index.
 */
static void Def_ReadLumpDefs()
{
    LOG_AS("Def_ReadLumpDefs");

    const LumpIndex &lumpIndex = fileSys().nameIndex();
    LumpIndex::FoundIndices foundDefns;
    lumpIndex.findAll("DD_DEFNS.lmp", foundDefns);
    for (const auto i : foundDefns)
    {
        if (!DED_ReadLump(DED_Definitions(), i))
        {
            LOG_RES_ERROR("Parse error reading \"%s:DD_DEFNS\": %s")
                << NativePath(lumpIndex[i].container().composePath()).pretty() << DED_Error();
        }
    }

    const auto numProcessedLumps = foundDefns.size();
    if (DoomsdayApp::verbose && numProcessedLumps > 0)
    {
        LOG_RES_NOTE("Processed %i %s")
                << numProcessedLumps << (numProcessedLumps != 1 ? "lumps" : "lump");
    }
}

/**
 * Uses gettingFor. Initializes the state-owners information.
 */
int Def_StateForMobj(const String &state)
{
    int num = DED_Definitions()->getStateNum(state);
    if (num < 0) num = 0;

    // State zero is the NULL state.
    if (num > 0)
    {
        ::runtimeDefs.stateInfo[num].owner = ::gettingFor;
        // Scan forward at most 'count' states, or until we hit a state with
        // an owner, or the NULL state.
        int st, count = 16;
        for (st = ::runtimeDefs.states[num].nextState;
            st > 0 && count-- && !::runtimeDefs.stateInfo[st].owner;
            st = ::runtimeDefs.states[st].nextState)
        {
            ::runtimeDefs.stateInfo[st].owner = ::gettingFor;
        }
    }

    return num;
}

static void readDefinitionFile(const String& path)
{
    if (path.isEmpty()) return;

    LOG_RES_VERBOSE("Reading \"%s\"") << NativePath(path).pretty();
    Def_ReadProcessDED(DED_Definitions(), path);
}

#if 0
/**
 * Attempt to prepend the current work path. If @a src is already absolute do nothing.
 *
 * @param dst  Absolute path written here.
 * @param src  Original path.
 */
static void prependWorkPath(ddstring_t *dst, const ddstring_t *src)
{
    DE_ASSERT(dst && src);

    if (!F_IsAbsolute(src))
    {
        NativePath prepended = NativePath::workPath() / Str_Text(src);

        //char *curPath = Dir_CurrentPath();
        //Str_Prepend(dst, curPath.);
        //Dir_CleanPathStr(dst);
        //M_Free(curPath);

        return;
    }

    // Do we need to copy anyway?
    if (dst != src)
    {
        Str_Set(dst, Str_Text(src));
    }
}
#endif

/**
 * Returns a URN list (in load order) for all lumps whose name matches the pattern "MAPINFO.lmp".
 */
static StringList allMapInfoUrns()
{
    StringList foundPaths;

    // The game's main MAPINFO definitions should be processed first.
    bool ignoreNonCustom = false;
    try
    {
        String mainMapInfo = fileSys().findPath(res::Uri(App_CurrentGame().mainMapInfo()), RLF_MATCH_EXTENSION);
        if (!mainMapInfo.isEmpty())
        {
            foundPaths << mainMapInfo;
            ignoreNonCustom = true;
        }
    }
    catch (const FS1::NotFoundError &)
    {}  // Ignore this error.

    // Process all other lumps named MAPINFO.lmp
    const LumpIndex &lumpIndex = fileSys().nameIndex();
    LumpIndex::FoundIndices foundLumps;
    lumpIndex.findAll("MAPINFO.lmp", foundLumps);
    for (const auto &lumpNumber : foundLumps)
    {
        // Ignore MAPINFO definition data in IWADs?
        if (ignoreNonCustom)
        {
            const File1 &file = lumpIndex[lumpNumber];
            /// @todo Custom status for contained files is not inherited from the container?
            if (file.isContained())
            {
                if (!file.container().hasCustom())
                    continue;
            }
            else if (!file.hasCustom())
                continue;
        }

        foundPaths << Stringf("LumpIndex:%i", lumpNumber);
    }

    return foundPaths;
}

/**
 * @param mapInfoUrns  MAPINFO definitions to translate, in load order.
 */
static void translateMapInfos(const StringList &mapInfoUrns, String &xlat, String &xlatCustom)
{
    xlat.clear();
    xlatCustom.clear();

    String delimitedPaths = String::join(mapInfoUrns, ";");
    if (delimitedPaths.isEmpty()) return;

    ddhook_mapinfo_convert_t parm;
    Str_InitStd(&parm.paths);
    Str_InitStd(&parm.translated);
    Str_InitStd(&parm.translatedCustom);
    try
    {
        Str_Set(&parm.paths, delimitedPaths);
        if (DoomsdayApp::plugins().callAllHooks(HOOK_MAPINFO_CONVERT, 0, &parm))
        {
            xlat       = Str_Text(&parm.translated);
            xlatCustom = Str_Text(&parm.translatedCustom);
        }
    }
    catch (...)
    {}
    Str_Free(&parm.translatedCustom);
    Str_Free(&parm.translated);
    Str_Free(&parm.paths);
}

static void readAllDefinitions()
{
    Time begunAt;

    // Start with engine's own top-level definition file.
    readDefinitionFile(App::packageLoader().package("net.dengine.base").root()
                       .locate<File const>("defs/doomsday.ded").path());

    if (App_GameLoaded())
    {
        const Game &game = App_CurrentGame();

        // Some games use definitions (MAPINFO lumps) that are translated to DED.
        StringList mapInfoUrns = allMapInfoUrns();
        if (!mapInfoUrns.isEmpty())
        {
            String xlat, xlatCustom;
            translateMapInfos(mapInfoUrns, xlat, xlatCustom);

            if (!xlat.isEmpty())
            {
                LOG_AS("Non-custom translated");
                LOGDEV_MAP_VERBOSE("MAPINFO definitions:\n") << xlat;

                if (!DED_ReadData(DED_Definitions(), xlat,
                                 "[TranslatedMapInfos]", false /*not custom*/))
                {
                    LOG_RES_ERROR("DED parse error: %s") << DED_Error();
                }
            }

            if (!xlatCustom.isEmpty())
            {
                LOG_AS("Custom translated");
                LOGDEV_MAP_VERBOSE("MAPINFO definitions:\n") << xlatCustom;

                if (!DED_ReadData(DED_Definitions(), xlatCustom,
                                 "[TranslatedMapInfos]", true /*custom*/))
                {
                    LOG_RES_ERROR("DED parse error: %s") << DED_Error();
                }
            }
        }

        // Now any startup definition files required by the game.
        const Game::Manifests &gameResources = game.manifests();
        int packageIdx = 0;
        const auto foundRes = gameResources.equal_range(RC_DEFINITION);
        for (auto i = foundRes.first; i != foundRes.second; ++i, ++packageIdx)
        {
            ResourceManifest &record = *i->second;
            /// Try to locate this resource now.
            const String &path = record.resolvedPath(true/*try to locate*/);
            if (path.isEmpty())
            {
                const auto names = String::join(record.names(), ";");
                LOG_RES_ERROR("Failed to locate required game definition \"%s\"") << names;
            }
            readDefinitionFile(path);
        }

        // Next are definition files in the games' /auto directory.
        if (!CommandLine_Exists("-noauto"))
        {
            FS1::PathList foundPaths;
            if (fileSys().findAllPaths(res::makeUri("$(App.DefsPath)/$(GamePlugin.Name)/auto/*.ded").resolved(), 0, foundPaths))
            {
                for (const FS1::PathListItem &found : foundPaths)
                {
                    // Ignore directories.
                    if (found.attrib & A_SUBDIR) continue;

                    readDefinitionFile(found.path);
                }
            }
        }
    }

    // Definitions from loaded data bundles.
    for (const DataBundle *bundle : DataBundle::loadedBundles())
    {
        if (bundle->format() == DataBundle::Ded)
        {
            const String bundleRoot = bundle->rootPath();
            for (const Value *path : bundle->packageMetadata().geta("dataFiles").elements())
            {
                readDefinitionFile(bundleRoot / path->asText());
            }
        }
    }

    // Definitions from loaded packages.
    for (Package *pkg : App::packageLoader().loadedPackagesInOrder())
    {
        res::DoomsdayPackage ddPkg(*pkg);
        if (ddPkg.hasDefinitions())
        {
            // Relative to package root.
            const Folder &defsFolder = pkg->root().locate<Folder const>(ddPkg.defsPath());

            // Read all the DED files found in this folder, in alphabetical order.
            // Subfolders are not checked -- the DED files need to manually `Include`
            // any files from subfolders.
            defsFolder.forContents([] (String name, File &file)
            {
                if (!name.fileNameExtension().compare(".ded", CaseInsensitive))
                {
                    readDefinitionFile(file.path());
                }
                return LoopContinue;
            });
        }
    }

    // Last are DD_DEFNS definition lumps from loaded add-ons.
    /// @todo Shouldn't these be processed before definitions on the command line?
    Def_ReadLumpDefs();

    LOG_RES_VERBOSE("readAllDefinitions: Completed in %.2f seconds") << begunAt.since();
}

static void defineFlaremap(const res::Uri &resourceUri)
{
    if (resourceUri.isEmpty()) return;

    // Reference to none?
    if (!resourceUri.path().toString().compareWithoutCase("-")) return;

    // Reference to a "built-in" flaremap?
    const String resourcePathStr = resourceUri.path();
    if (resourcePathStr.length() == 1 &&
        resourcePathStr.first() >= '0' && resourcePathStr.first() <= '4')
        return;

    res::Textures::get().defineTexture("Flaremaps", resourceUri);
}

static void defineLightmap(const res::Uri &resourceUri)
{
    if (resourceUri.isEmpty()) return;

    // Reference to none?
    if (!resourceUri.path().toString().compareWithoutCase("-")) return;

    res::Textures::get().defineTexture("Lightmaps", resourceUri);
}

static void generateMaterialDefForTexture(const res::TextureManifest &manifest)
{
    LOG_AS("generateMaterialDefForTexture");

    Record &mat = DED_Definitions()->materials[DED_Definitions()->addMaterial()];
    mat.set("autoGenerated", true);

    const res::Uri texUri = manifest.composeUri();
    mat.set("id", res::Uri(DD_MaterialSchemeNameForTextureScheme(texUri.scheme()), texUri.path()).compose());

    if (manifest.hasTexture())
    {
        res::Texture &tex = manifest.texture();
        mat.set("dimensions", new ArrayValue(tex.dimensions()));
        mat.set("flags", int(tex.isFlagged(res::Texture::NoDraw) ? MATF_NO_DRAW : 0));
    }
    else
    {
        LOGDEV_RES_MSG("Texture \"%s\" not yet defined, resultant Material will inherit dimensions") << texUri;
    }

    // The first layer and stage is implicit.
    defn::Material matDef(mat);
    defn::MaterialLayer layerDef(matDef.addLayer());

    Record &st0 = layerDef.addStage();
    st0.set("texture", texUri.compose());

    // Is there an animation for this?
    const res::AnimGroup *anim = res::AnimGroups::get().animGroupForTexture(manifest);
    if (anim && anim->frameCount() > 1)
    {
        // Determine the start frame.
        int startFrame = 0;
        while (&anim->frame(startFrame).textureManifest() != &manifest)
        {
            startFrame++;
        }

        // Just animate the first in the sequence?
        if (startFrame && (anim->flags() & AGF_FIRST_ONLY))
            return;

        // Complete configuration of the first stage.
        const res::AnimGroupFrame &animFrame0 = anim->frame(startFrame);
        st0.set("tics", int( animFrame0.tics() + animFrame0.randomTics()) );
        if (animFrame0.randomTics())
        {
            st0.set("variance", animFrame0.randomTics() / st0.getf("tics"));
        }

        // Add further stages according to the animation group.
        startFrame++;
        for (int i = 0; i < anim->frameCount() - 1; ++i)
        {
            const res::AnimGroupFrame &animFrame      = anim->frame(de::wrap(startFrame + i, 0, anim->frameCount()));
            const res::TextureManifest &frameManifest = animFrame.textureManifest();

            Record &st = layerDef.addStage();
            st.set("texture", frameManifest.composeUrn().compose());
            st.set("tics", int( animFrame.tics() + animFrame.randomTics() ));
            if (animFrame.randomTics())
            {
                st.set("variance", animFrame.randomTics() / st.getf("tics"));
            }
        }
    }
}

static void generateMaterialDefsForAllTexturesInScheme(res::TextureScheme &scheme)
{
    PathTreeIterator<res::TextureScheme::Index> iter(scheme.index().leafNodes());
    while (iter.hasNext()) generateMaterialDefForTexture(iter.next());
}

static inline void generateMaterialDefsForAllTexturesInScheme(const String &schemeName)
{
    generateMaterialDefsForAllTexturesInScheme(res::Textures::get().textureScheme(schemeName));
}

static void generateMaterialDefs()
{
    generateMaterialDefsForAllTexturesInScheme("Textures");
    generateMaterialDefsForAllTexturesInScheme("Flats");
    generateMaterialDefsForAllTexturesInScheme("Sprites");
}

#ifdef __CLIENT__

/**
 * Returns @c true iff @a decorDef is compatible with the specified context.
 */
static bool decorationIsCompatible(const Record &decorDef, const res::Uri &textureUri,
                                   bool materialIsCustom)
{
    if (res::makeUri(decorDef.gets("texture")) != textureUri)
        return false;

    if (materialIsCustom)
    {
        return (decorDef.geti("flags") & DCRF_PWAD) != 0;
    }
    return (decorDef.geti("flags") & DCRF_NO_IWAD) == 0;
}

/**
 * (Re)Decorate the given @a material according to definition @a def. Any existing
 * decorations will be cleared in the process.
 *
 * @param material  The material being (re)decorated.
 * @param def       Definition to apply.
 */
static void redecorateMaterial(ClientMaterial &material, const Record &def)
{
    defn::Material matDef(def);

    material.clearAllDecorations();

    // Prefer decorations defined within the material.
    for (int i = 0; i < matDef.decorationCount(); ++i)
    {
        defn::MaterialDecoration decorDef(matDef.decoration(i));

        for (int k = 0; k < decorDef.stageCount(); ++k)
        {
            const Record &st = decorDef.stage(k);

            defineLightmap(res::makeUri(st.gets("lightmapUp")));
            defineLightmap(res::makeUri(st.gets("lightmapDown")));
            defineLightmap(res::makeUri(st.gets("lightmapSide")));
            defineFlaremap(res::makeUri(st.gets("haloTexture")));
        }

        material.addDecoration(LightMaterialDecoration::fromDef(decorDef.def()));
    }

    if (material.hasDecorations())
        return;

    // Perhaps old style linked decoration definitions?
    if (material.layerCount())
    {
        // The animation configuration of layer0 determines decoration animation.
        const auto &decorationsByTexture = DED_Definitions()->decorations.lookup("texture").elements();
        const auto &layer0               = material.layer(0).as<world::TextureMaterialLayer>();

        bool haveDecorations = false;
        List<const Record *> stageDecorations(layer0.stageCount());
        for (int i = 0; i < layer0.stageCount(); ++i)
        {
            const world::TextureMaterialLayer::AnimationStage &stage = layer0.stage(i);
            try
            {
                res::TextureManifest &texManifest = res::Textures::get().textureManifest(stage.texture);
                const res::Uri texUri = texManifest.composeUri();
                for (const auto &pair : decorationsByTexture)
                {
                    const Record &rec = *pair.second->as<RecordValue>().record();
                    if (decorationIsCompatible(rec, texUri, material.manifest().isCustom()))
                    {
                        stageDecorations[i] = &rec;
                        haveDecorations = true;
                        break;
                    }
                }
            }
            catch (const Resources::MissingResourceManifestError &)
            {}  // Ignore this error
        }

        if (!haveDecorations) return;

        for (int i = 0; i < layer0.stageCount(); ++i)
        {
            if (!stageDecorations[i]) continue;

            defn::Decoration mainDef(*stageDecorations[i]);
            for (int k = 0; k < mainDef.lightCount(); ++k)
            {
                defn::MaterialDecoration decorDef(mainDef.light(k));
                DE_ASSERT(decorDef.stageCount() == 1); // sanity check.

                std::unique_ptr<LightMaterialDecoration> decor(
                        new LightMaterialDecoration(Vec2i(decorDef.geta("patternSkip")),
                                                    Vec2i(decorDef.geta("patternOffset")),
                                                    false /*don't use interpolation*/));

                std::unique_ptr<LightMaterialDecoration::AnimationStage> definedDecorStage(
                        LightMaterialDecoration::AnimationStage::fromDef(decorDef.stage(0)));

                definedDecorStage->tics = layer0.stage(i).tics;

                for (int m = 0; m < i; ++m)
                {
                    LightMaterialDecoration::AnimationStage preStage(*definedDecorStage);
                    preStage.tics  = layer0.stage(m).tics;
                    preStage.color = Vec3f();
                    decor->addStage(preStage);  // makes a copy.
                }

                decor->addStage(*definedDecorStage);

                for (int m = i + 1; m < layer0.stageCount(); ++m)
                {
                    LightMaterialDecoration::AnimationStage postStage(*definedDecorStage);
                    postStage.tics  = layer0.stage(m).tics;
                    postStage.color = Vec3f();
                    decor->addStage(postStage);
                }

                material.addDecoration(decor.release());  // takes ownership.
            }
        }
    }
}

#endif // __CLIENT__

static ded_group_t *findGroupForMaterialLayerAnimation(const res::Uri &uri)
{
    if (uri.isEmpty()) return nullptr;

    // Reverse iteration (later defs override earlier ones).
    for (int i = DED_Definitions()->groups.size(); i--> 0; )
    {
        ded_group_t &grp = DED_Definitions()->groups[i];

        // We aren't interested in precache groups.
        if (grp.flags & AGF_PRECACHE) continue;

        // Or empty/single-frame groups.
        if (grp.members.size() < 2) continue;

        // The referenced material must be a member.
        if (!grp.tryFindFirstMemberWithMaterial(uri)) continue;

        // Only consider groups where each frame has a valid duration.
        int k;
        for (k = 0; k < grp.members.size(); ++k)
        {
            if (grp.members[k].tics < 0) break;
        }
        if (k < grp.members.size()) continue;

        // Found a suitable Group.
        return &grp;
    }

    return nullptr;  // Not found.
}

static void configureMaterial(world::Material &mat, const Record &definition)
{
    defn::Material matDef(definition);
    res::Uri const materialUri(matDef.gets("id"), RC_NULL);

    // Reconfigure basic properties.
    mat.setDimensions(Vec2ui(matDef.geta("dimensions")));
    mat.markDontDraw((matDef.geti("flags") & MATF_NO_DRAW) != 0);
    mat.markSkyMasked((matDef.geti("flags") & MATF_SKYMASK) != 0);
    mat.setAudioEnvironment(S_AudioEnvironmentId(&materialUri));

    // Reconfigure the layers.
    mat.clearAllLayers();
    for (int i = 0; i < matDef.layerCount(); ++i)
    {
        mat.addLayerAt(world::TextureMaterialLayer::fromDef(matDef.layer(i)), mat.layerCount());
    }

    if (mat.layerCount() && mat.layer(0).stageCount())
    {
        auto &layer0 = mat.layer(0).as<world::TextureMaterialLayer>();
        world::TextureMaterialLayer::AnimationStage &stage0 = layer0.stage(0);

        if (!stage0.texture.isEmpty())
        {
            // We may need to interpret the layer animation from the now
            // deprecated Group definitions.

            if (matDef.getb(DE_STR("autoGenerated")) && layer0.stageCount() == 1)
            {
                const res::Uri &textureUri = stage0.texture;

                // Possibly; see if there is a compatible definition with
                // a member named similarly to the texture for layer #0.

                if (const ded_group_t *grp = findGroupForMaterialLayerAnimation(textureUri))
                {
                    // Determine the start frame.
                    int startFrame = 0;
                    while (!grp->members[startFrame].material ||
                           *grp->members[startFrame].material != textureUri)
                    {
                        startFrame++;
                    }

                    // Configure the first stage.
                    const ded_group_member_t &gm0 = grp->members[startFrame];
                    stage0.tics     = gm0.tics;
                    stage0.variance = de::max(gm0.randomTics, 0) / float( gm0.tics );

                    // Add further stages for each frame in the group.
                    startFrame++;
                    for (int i = 0; i < grp->members.size() - 1; ++i)
                    {
                        const int frame              = de::wrap(startFrame + i, 0, grp->members.size());
                        const ded_group_member_t &gm = grp->members[frame];

                        if (gm.material)
                        {
                            const int tics       = gm.tics;
                            const float variance = de::max(gm.randomTics, 0) / float( gm.tics );

                            layer0.addStage(world::TextureMaterialLayer::AnimationStage(*gm.material, tics, variance));
                        }
                    }
                }
            }

            // Are there Detail definitions we need to produce a layer for?
            world::DetailTextureMaterialLayer *dlayer = nullptr;
            for (int i = 0; i < layer0.stageCount(); ++i)
            {
                world::TextureMaterialLayer::AnimationStage &stage = layer0.stage(i);
                const ded_detailtexture_t *detailDef =
                    tryFindDetailTexture(stage.texture, /*UNKNOWN VALUE,*/ mat.manifest().isCustom());

                if (!detailDef || !detailDef->stage.texture)
                    continue;

                if (!dlayer)
                {
                    // Add a new detail layer.
                    mat.addLayerAt(dlayer = world::DetailTextureMaterialLayer::fromDef(*detailDef), 0);
                }
                else
                {
                    // Add a new stage.
                    try
                    {
                        res::TextureManifest &texture = res::Textures::get().textureScheme("Details")
                                .findByResourceUri(*detailDef->stage.texture);
                        dlayer->addStage(world::DetailTextureMaterialLayer::AnimationStage
                                         (texture.composeUri(), stage.tics, stage.variance,
                                          detailDef->stage.scale, detailDef->stage.strength,
                                          detailDef->stage.maxDistance));

                        if (dlayer->stageCount() == 2)
                        {
                            // Update the first stage with timing info.
                            const world::TextureMaterialLayer::AnimationStage &stage0 = layer0.stage(0);
                            world::TextureMaterialLayer::AnimationStage &dstage0      = dlayer->stage(0);

                            dstage0.tics     = stage0.tics;
                            dstage0.variance = stage0.variance;
                        }
                    }
                    catch (const Resources::MissingResourceManifestError &)
                    {}  // Ignore this error.
                }
            }

            // Are there Reflection definition we need to produce a layer for?
            world::ShineTextureMaterialLayer *slayer = nullptr;
            for (int i = 0; i < layer0.stageCount(); ++i)
            {
                world::TextureMaterialLayer::AnimationStage &stage = layer0.stage(i);
                const ded_reflection_t *shineDef =
                    tryFindReflection(stage.texture, /*UNKNOWN VALUE,*/ mat.manifest().isCustom());

                if (!shineDef || !shineDef->stage.texture)
                    continue;

                if (!slayer)
                {
                    // Add a new shine layer.
                    mat.addLayerAt(slayer = world::ShineTextureMaterialLayer::fromDef(*shineDef), mat.layerCount());
                }
                else
                {
                    // Add a new stage.
                    try
                    {
                        res::TextureManifest &texture = res::Textures::get().textureScheme("Reflections")
                                                               .findByResourceUri(*shineDef->stage.texture);

                        if (shineDef->stage.maskTexture)
                        {
                            try
                            {
                                res::TextureManifest *maskTexture = &res::Textures::get().textureScheme("Masks")
                                                   .findByResourceUri(*shineDef->stage.maskTexture);

                                slayer->addStage(world::ShineTextureMaterialLayer::AnimationStage
                                                 (texture.composeUri(), stage.tics, stage.variance,
                                                  maskTexture->composeUri(), shineDef->stage.blendMode,
                                                  shineDef->stage.shininess,
                                                  Vec3f(shineDef->stage.minColor),
                                                  Vec2f(shineDef->stage.maskWidth, shineDef->stage.maskHeight)));
                            }
                            catch (const Resources::MissingResourceManifestError &)
                            {}  // Ignore this error.
                        }

                        if (slayer->stageCount() == 2)
                        {
                            // Update the first stage with timing info.
                            const world::TextureMaterialLayer::AnimationStage &stage0 = layer0.stage(0);
                            world::TextureMaterialLayer::AnimationStage &sstage0      = slayer->stage(0);

                            sstage0.tics     = stage0.tics;
                            sstage0.variance = stage0.variance;
                        }
                    }
                    catch (const Resources::MissingResourceManifestError &)
                    {}  // Ignore this error.
                }
            }
        }
    }

#ifdef __CLIENT__
    if (is<ClientMaterial>(mat))
    {
        redecorateMaterial(static_cast<ClientMaterial &>(mat), definition);
    }
#endif

    // At this point we know the material is usable.
    mat.markValid();
}

static void interpretMaterialDef(const Record &definition)
{
    LOG_AS("interpretMaterialDef");
    defn::Material matDef(definition);
    res::Uri const materialUri(matDef.gets("id"), RC_NULL);
    try
    {
        // Create/retrieve a manifest for the would-be material.
        world::MaterialManifest *manifest = &world::Materials::get().declareMaterial(materialUri);

        // Update manifest classification:
        manifest->setFlags(world::MaterialManifest::AutoGenerated, matDef.getb("autoGenerated")? SetFlags : UnsetFlags);
        manifest->setFlags(world::MaterialManifest::Custom, UnsetFlags);
        if (matDef.layerCount())
        {
            defn::MaterialLayer layerDef(matDef.layer(0));
            if (layerDef.stageCount() > 0)
            {
                res::Uri const textureUri(layerDef.stage(0).gets("texture"), RC_NULL);
                try
                {
                    res::TextureManifest &texManifest = res::Textures::get().textureManifest(textureUri);
                    if (texManifest.hasTexture() && texManifest.texture().isFlagged(res::Texture::Custom))
                    {
                        manifest->setFlags(world::MaterialManifest::Custom);
                    }
                }
                catch (const Resources::MissingResourceManifestError &er)
                {
                    // Log but otherwise ignore this error.
                    LOG_RES_MSG("Ignoring unknown texture \"%s\" in Material \"%s\" (layer 0 stage 0): %s")
                        << textureUri
                        << materialUri
                        << er.asText();
                }
            }
        }

        // (Re)configure the material.
        /// @todo Defer until necessary.
        configureMaterial(*manifest->derive(), definition);
    }
    catch (const Resources::UnknownSchemeError &er)
    {
        LOG_RES_WARNING("Failed to declare material \"%s\": %s")
            << materialUri << er.asText();
    }
    catch (const world::MaterialScheme::InvalidPathError &er)
    {
        LOG_RES_WARNING("Failed to declare material \"%s\": %s")
            << materialUri << er.asText();
    }
}

static void invalidateAllMaterials()
{
    world::Materials::get().forAllMaterials([] (world::Material &material)
    {
        material.markValid(false);
        return LoopContinue;
    });
}

#ifdef __CLIENT__
static void clearFontDefinitionLinks()
{
    for (AbstractFont *font : ClientResources::get().allFonts())
    {
        if (CompositeBitmapFont *compFont = maybeAs<CompositeBitmapFont>(font))
        {
            compFont->setDefinition(nullptr);
        }
    }
}
#endif // __CLIENT__

void Def_Read()
{
    LOG_AS("Def_Read");

    if (defsInited)
    {
        // We've already initialized the definitions once.
        // Get rid of everything.
        FS1::Scheme &scheme = fileSys().scheme(App_ResourceClass("RC_MODEL").defaultScheme());
        scheme.reset();

        invalidateAllMaterials();
#ifdef __CLIENT__
        clearFontDefinitionLinks();
#endif
        defsInited = false;
    }

    auto &defs = *DED_Definitions();

    // Now we can clear all existing definitions and re-init.
    defs.clear();
    runtimeDefs.clear();

    // Generate definitions.
    generateMaterialDefs();

    // Read all definitions files and lumps.
    LOG_RES_MSG("Parsing definition files...");
    readAllDefinitions();

    // Any definition hooks?
    DoomsdayApp::plugins().callAllHooks(HOOK_DEFS, 0, &defs);

#ifdef __CLIENT__
    // Composite fonts.
    for (int i = 0; i < defs.compositeFonts.size(); ++i)
    {
        ClientResources::get().newFontFromDef(defs.compositeFonts[i]);
    }
#endif

    // States.
    ::runtimeDefs.states.append(defs.states.size());
    for (int i = 0; i < ::runtimeDefs.states.size(); ++i)
    {
        Record &dst = defs.states[i];

        // Make sure duplicate IDs overwrite the earliest.
        int stateNum = defs.getStateNum(dst.gets("id"));
        if (stateNum == -1) continue;

        Record &dstNew = defs.states[stateNum];
        state_t *st = &::runtimeDefs.states[stateNum];

        st->sprite    = defs.getSpriteNum(dst.gets("sprite"));
        st->flags     = dst.geti("flags");
        st->frame     = dst.geti("frame");
        st->tics      = dst.geti("tics");
        st->action    = P_GetAction(dst.gets("action"));
        st->nextState = defs.getStateNum(dst.gets("nextState"));

        if (st->nextState == -1)
        {
            LOG_WARNING("State \"%s\": next state \"%s\" is not defined") << dst.gets("id")
                                                                          << dst.gets("nextState");
        }

        const auto &misc = dst.geta("misc");
        for (int k = 0; k < NUM_STATE_MISC; ++k)
        {
            st->misc[k] = misc[k].asInt();
        }

        // Replace the older execute string.
        if (&dst != &dstNew)
        {
            dstNew.set("execute", dst.gets("execute"));
        }
    }

    ::runtimeDefs.stateInfo.append(defs.states.size());

    // Mobj info.
    ::runtimeDefs.mobjInfo.append(defs.things.size());
    for (int i = 0; i < runtimeDefs.mobjInfo.size(); ++i)
    {
        Record *dmo = &defs.things[i];

        // Make sure duplicate defs overwrite the earliest.
        mobjinfo_t *mo = &runtimeDefs.mobjInfo[defs.getMobjNum(dmo->gets("id"))];

        gettingFor       = mo;
        mo->doomEdNum    = dmo->geti("doomEdNum");
        mo->spawnHealth  = dmo->geti("spawnHealth");
        mo->reactionTime = dmo->geti("reactionTime");
        mo->painChance   = dmo->geti("painChance");
        mo->speed        = dmo->getf("speed");
        mo->radius       = dmo->getf("radius");
        mo->height       = dmo->getf("height");
        mo->mass         = dmo->geti("mass");
        mo->damage       = dmo->geti("damage");
        mo->flags        = dmo->geta("flags")[0].asInt();
        mo->flags2       = dmo->geta("flags")[1].asInt();
        mo->flags3       = dmo->geta("flags")[2].asInt();

        const auto &states = dmo->geta("states");
        const auto &sounds = dmo->geta("sounds");

        for (int k = 0; k < STATENAMES_COUNT; ++k)
        {
            mo->states[k] = Def_StateForMobj(states[k].asText());
        }

        mo->seeSound     = defs.getSoundNum(sounds[SDN_SEE].asText());
        mo->attackSound  = defs.getSoundNum(sounds[SDN_ATTACK].asText());
        mo->painSound    = defs.getSoundNum(sounds[SDN_PAIN].asText());
        mo->deathSound   = defs.getSoundNum(sounds[SDN_DEATH].asText());
        mo->activeSound  = defs.getSoundNum(sounds[SDN_ACTIVE].asText());

        for (int k = 0; k < NUM_MOBJ_MISC; ++k)
        {
            mo->misc[k] = dmo->geta("misc")[k].asInt();
        }
    }

    // Decorations. (Define textures).
    for (int i = 0; i < defs.decorations.size(); ++i)
    {
        defn::Decoration decorDef(defs.decorations[i]);
        for (int k = 0; k < decorDef.lightCount(); ++k)
        {
            const Record &st = defn::MaterialDecoration(decorDef.light(k)).stage(0);
            if (Vec3f(st.geta("color")) != Vec3f(0.0f))
            {
                defineLightmap(res::makeUri(st["lightmapUp"]));
                defineLightmap(res::makeUri(st["lightmapDown"]));
                defineLightmap(res::makeUri(st["lightmapSide"]));
                defineFlaremap(res::makeUri(st["haloTexture"]));
            }
        }
    }

    // Detail textures (Define textures).
    res::Textures::get().textureScheme("Details").clear();
    for (int i = 0; i < defs.details.size(); ++i)
    {
        ded_detailtexture_t *dtl = &defs.details[i];

        // Ignore definitions which do not specify a material.
        if ((!dtl->material1 || dtl->material1->isEmpty()) &&
           (!dtl->material2 || dtl->material2->isEmpty())) continue;

        if (!dtl->stage.texture) continue;

        res::Textures::get().defineTexture("Details", *dtl->stage.texture);
    }

    // Surface reflections (Define textures).
    res::Textures::get().textureScheme("Reflections").clear();
    res::Textures::get().textureScheme("Masks").clear();
    for (int i = 0; i < defs.reflections.size(); ++i)
    {
        ded_reflection_t *ref = &defs.reflections[i];

        // Ignore definitions which do not specify a material.
        if (!ref->material || ref->material->isEmpty()) continue;

        if (ref->stage.texture)
        {
            res::Textures::get().defineTexture("Reflections", *ref->stage.texture);
        }
        if (ref->stage.maskTexture)
        {
            res::Textures::get().defineTexture("Masks", *ref->stage.maskTexture,
                            Vec2ui(ref->stage.maskWidth, ref->stage.maskHeight));
        }
    }

    // Materials.
    for (int i = 0; i < defs.materials.size(); ++i)
    {
        interpretMaterialDef(defs.materials[i]);
    }

    // Dynamic lights. Update the sprite numbers.
    for (int i = 0; i < defs.lights.size(); ++i)
    {
        const int stateIdx = defs.getStateNum(defs.lights[i].state);
        if (stateIdx < 0)
        {
            // It's probably a bias light definition, then?
            if (!defs.lights[i].uniqueMapID[0])
            {
                LOG_RES_WARNING("Undefined state '%s' in Light definition") << defs.lights[i].state;
            }
            continue;
        }
        ::runtimeDefs.stateInfo[stateIdx].light = &defs.lights[i];
    }

    // Sound effects.
    ::runtimeDefs.sounds.append(defs.sounds.size());
    for (int i = 0; i < ::runtimeDefs.sounds.size(); ++i)
    {
        ded_sound_t *snd = &defs.sounds[i];
        // Make sure duplicate defs overwrite the earliest.
        sfxinfo_t *si    = &::runtimeDefs.sounds[defs.getSoundNum(snd->id)];

        strcpy(si->id, snd->id);
        strcpy(si->lumpName, snd->lumpName);
        si->lumpNum     = (strlen(snd->lumpName) > 0? fileSys().lumpNumForName(snd->lumpName) : -1);
        strcpy(si->name, snd->name);

        const int soundIdx = defs.getSoundNum(snd->link);
        si->link        = (soundIdx >= 0 ? &::runtimeDefs.sounds[soundIdx] : 0);

        si->linkPitch   = snd->linkPitch;
        si->linkVolume  = snd->linkVolume;
        si->priority    = snd->priority;
        si->channels    = snd->channels;
        si->flags       = snd->flags;
        si->group       = snd->group;

        Str_Init(&si->external);
        if (snd->ext)
        {
            Str_Set(&si->external, snd->ext->pathCStr());
        }
    }

    // Music.
    for (int i = defs.musics.size() - 1; i >= 0; --i)
    {
        const Record &mus = defs.musics[i];

        // Make sure duplicate defs overwrite contents from the earlier ones.
        // IDs can't be fully trusted because music definitions are sometimes
        // generated by idtech1importer, so they might have IDs that don't
        // match the vanilla IDs.

        for (int k = i - 1; k >= 0; --k)
        {
            Record &earlier = defs.musics[k];

            if (mus.gets("id").compareWithoutCase(earlier.gets("id")) == 0)
            {
                earlier.set("lumpName", mus.gets("lumpName"));
                earlier.set("cdTrack", mus.geti("cdTrack"));
                earlier.set("path", mus.gets("path"));
            }
            else if (mus.gets("lumpName").compareWithoutCase(earlier.gets("lumpName")) == 0)
            {
                earlier.set("path", mus.gets("path"));
                earlier.set("cdTrack", mus.geti("cdTrack"));
            }
        }
    }

    // Text.
    ::runtimeDefs.texts.append(defs.text.size());
    for (int i = 0; i < defs.text.size(); ++i)
    {
        Def_InitTextDef(&::runtimeDefs.texts[i], defs.text[i].text);
    }
    // Handle duplicate strings.
    for (int i = 0; i < ::runtimeDefs.texts.size(); ++i)
    {
        if (!::runtimeDefs.texts[i].text) continue;

        for (int k = i + 1; k < ::runtimeDefs.texts.size(); ++k)
        {
            if (!::runtimeDefs.texts[k].text) continue; // Already done.
            if (iCmpStrCase(defs.text[i].id, defs.text[k].id)) continue; // ID mismatch.

            // Update the earlier string.
            ::runtimeDefs.texts[i].text = (char *) M_Realloc(
                ::runtimeDefs.texts[i].text, strlen(::runtimeDefs.texts[k].text) + 1);
            strcpy(::runtimeDefs.texts[i].text, ::runtimeDefs.texts[k].text);

            // Free the later string, it isn't used (>NUMTEXT).
            M_Free(::runtimeDefs.texts[k].text);
            ::runtimeDefs.texts[k].text = nullptr;
        }
    }

#if defined(__CLIENT__)
    // Particle generators.
    for (int i = 0; i < defs.ptcGens.size(); ++i)
    {
        ded_ptcgen_t *pg = &defs.ptcGens[i];
        int st = defs.getStateNum(pg->state);

        if (!iCmpStr(pg->type, "*"))
            pg->typeNum = DED_PTCGEN_ANY_MOBJ_TYPE;
        else
            pg->typeNum = defs.getMobjNum(pg->type);
        pg->type2Num  = defs.getMobjNum(pg->type2);
        pg->damageNum = defs.getMobjNum(pg->damage);

        // Figure out embedded sound ID numbers.
        for (int k = 0; k < pg->stages.size(); ++k)
        {
            if (pg->stages[k].sound.name[0])
            {
                pg->stages[k].sound.id = defs.getSoundNum(pg->stages[k].sound.name);
            }
            if (pg->stages[k].hitSound.name[0])
            {
                pg->stages[k].hitSound.id = defs.getSoundNum(pg->stages[k].hitSound.name);
            }
        }

        if (st <= 0)
            continue; // Not state triggered, then...

        stateinfo_t *stinfo = &::runtimeDefs.stateInfo[st];

        // Link the definition to the state.
        if (pg->flags & Generator::StateChain)
        {
            // Add to the chain.
            pg->stateNext = stinfo->ptcGens;
            stinfo->ptcGens = pg;
        }
        else
        {
            // Make sure the previously built list is unlinked.
            while (stinfo->ptcGens)
            {
                ded_ptcgen_t *temp = stinfo->ptcGens->stateNext;

                stinfo->ptcGens->stateNext = nullptr;
                stinfo->ptcGens = temp;
            }
            stinfo->ptcGens = pg;
            pg->stateNext = nullptr;
        }
    }
#endif

    // Map infos.
    for (int i = 0; i < defs.mapInfos.size(); ++i)
    {
        Record &mi = defs.mapInfos[i];

        // Historically, the map info flags field was used for sky flags, here we copy
        // those flags to the embedded sky definition for backward-compatibility.
        if (mi.geti("flags") & MIF_DRAW_SPHERE)
        {
            mi.set("sky.flags", mi.geti("sky.flags") | SIF_DRAW_SPHERE);
        }
    }

    // Log a summary of the definition database.
    LOG_RES_MSG(_E(b) "Definitions:");
    String str;
    str += defCountMsg(defs.episodes.size(),         "episodes");
    str += defCountMsg(defs.groups.size(),           "animation groups");
    str += defCountMsg(defs.compositeFonts.size(),   "composite fonts");
    str += defCountMsg(defs.details.size(),          "detail textures");
    str += defCountMsg(defs.finales.size(),          "finales");
    str += defCountMsg(defs.lights.size(),           "lights");
    str += defCountMsg(defs.lineTypes.size(),        "line types");
    str += defCountMsg(defs.mapInfos.size(),         "map infos");

    int nonAutoGeneratedCount = 0;
    for (int i = 0; i < defs.materials.size(); ++i)
    {
        if (!defs.materials[i].getb("autoGenerated"))
            ++nonAutoGeneratedCount;
    }
    str += defCountMsg(nonAutoGeneratedCount,        "materials");

    str += defCountMsg(defs.models.size(),           "models");
    str += defCountMsg(defs.ptcGens.size(),          "particle generators");
    str += defCountMsg(defs.skies.size(),            "skies");
    str += defCountMsg(defs.sectorTypes.size(),      "sector types");
    str += defCountMsg(defs.musics.size(),           "songs");
    str += defCountMsg(::runtimeDefs.sounds.size(),  "sound effects");
    str += defCountMsg(defs.sprites.size(),          "sprite names");
    str += defCountMsg(::runtimeDefs.states.size(),  "states");
    str += defCountMsg(defs.decorations.size(),      "surface decorations");
    str += defCountMsg(defs.reflections.size(),      "surface reflections");
    str += defCountMsg(::runtimeDefs.texts.size(),   "text strings");
    str += defCountMsg(defs.textureEnv.size(),       "texture environments");
    str += defCountMsg(::runtimeDefs.mobjInfo.size(),"things");

    LOG_RES_MSG("%s") << str.rightStrip();

    ::defsInited = true;
}

static void initMaterialGroup(ded_group_t &def)
{
    world::Materials::MaterialManifestGroup *group = nullptr;
    for (int i = 0; i < def.members.size(); ++i)
    {
        ded_group_member_t *gm = &def.members[i];
        if (!gm->material) continue;

        try
        {
            world::MaterialManifest &manifest = world::Materials::get().materialManifest(*gm->material);

            if (def.flags & AGF_PRECACHE) // A precache group.
            {
                // Only create the group once the first material has been found.
                if (!group)
                {
                    group = &world::Materials::get().newMaterialGroup();
                }

                group->insert(&manifest);
            }
#if 0 /// @todo $revise-texture-animation
            else // An animation group.
            {
                // Only create the group once the first material has been found.
                if (animNumber == -1)
                {
                    animNumber = resSys().newAnimGroup(def.flags & ~AGF_PRECACHE);
                }

                resSys().animGroup(animNumber).addFrame(manifest.material(), gm->tics, gm->randomTics);
            }
#endif
        }
        catch (const Resources::MissingResourceManifestError &er)
        {
            // Log but otherwise ignore this error.
            LOG_RES_WARNING("Unknown material \"%s\" in group def %i: %s")
                    << *gm->material
                    << i << er.asText();
        }
    }
}

void Def_PostInit()
{
#ifdef __CLIENT__

    // Particle generators: model setup.
    for (int i = 0; i < DED_Definitions()->ptcGens.size(); ++i)
    {
        ded_ptcgen_t *gen = &DED_Definitions()->ptcGens[i];

        for (int k = 0; k < gen->stages.size(); ++k)
        {
            ded_ptcstage_t *st = &gen->stages[k];

            if (st->type < PTC_MODEL || st->type >= PTC_MODEL + MAX_PTC_MODELS)
                continue;

            st->model = -1;
            try
            {
                FrameModelDef &modef = ClientResources::get().modelDef(
                    Stringf("Particle%02i", st->type - PTC_MODEL));
                if (modef.subModelId(0) == NOMODELID)
                {
                    continue;
                }

                FrameModel &mdl = ClientResources::get().model(modef.subModelId(0));

                st->model = ClientResources::get().indexOf(&modef);
                st->frame = mdl.frameNumber(st->frameName);
                if (st->frame < 0) st->frame = 0;
                if (st->endFrameName[0])
                {
                    st->endFrame = mdl.frameNumber(st->endFrameName);
                    if (st->endFrame < 0) st->endFrame = 0;
                }
                else
                {
                    st->endFrame = -1;
                }
            }
            catch (const ClientResources::MissingModelDefError &)
            {}  // Ignore this error.
        }
    }

#endif // __CLIENT__

    // Lights.
    for (int i = 0; i < DED_Definitions()->lights.size(); ++i)
    {
        ded_light_t &lightDef = DED_Definitions()->lights[i];

        if (lightDef.up)    defineLightmap(*lightDef.up);
        if (lightDef.down)  defineLightmap(*lightDef.down);
        if (lightDef.sides) defineLightmap(*lightDef.sides);
        if (lightDef.flare) defineFlaremap(*lightDef.flare);
    }

    // Material groups (e.g., for precaching).
    world::Materials::get().clearAllMaterialGroups();
    for (int i = 0; i < DED_Definitions()->groups.size(); ++i)
    {
        initMaterialGroup(DED_Definitions()->groups[i]);
    }
}

bool Def_SameStateSequence(state_t *snew, state_t *sold)
{
    if (!snew || !sold) return false;
    if (snew == sold) return true;  // Trivial.

    const int target = ::runtimeDefs.states.indexOf(snew);
    const int start  = ::runtimeDefs.states.indexOf(sold);

    int count = 0;
    for (int it = sold->nextState; it >= 0 && it != start && count < 16;
        it = ::runtimeDefs.states[it].nextState, ++count)
    {
        if (it == target)
            return true;

        if (it == ::runtimeDefs.states[it].nextState)
            break;
    }
    return false;
}

String Def_GetStateName(const state_t *state)
{
    if (!state) return "(nullptr)";
    const int idx = ::runtimeDefs.states.indexOf(state);
    DE_ASSERT(idx >= 0);
    return DED_Definitions()->states[idx].gets("id");
}

static inline int Friendly(int num)
{
    return de::max(0, num);
}

/**
 * Converts a DED line type to the internal format.
 * Bit of a nuisance really...
 */
void Def_CopyLineType(linetype_t *l, ded_linetype_t *def)
{
    DE_ASSERT(l && def);

    l->id               = def->id;
    l->flags            = def->flags[0];
    l->flags2           = def->flags[1];
    l->flags3           = def->flags[2];
    l->lineClass        = def->lineClass;
    l->actType          = def->actType;
    l->actCount         = def->actCount;
    l->actTime          = def->actTime;
    l->actTag           = def->actTag;

    for (int i = 0; i < 10; ++i)
    {
        if (i == 9)
            l->aparm[i] = DED_Definitions()->getMobjNum(def->aparm9);
        else
            l->aparm[i] = def->aparm[i];
    }

    l->tickerStart      = def->tickerStart;
    l->tickerEnd        = def->tickerEnd;
    l->tickerInterval   = def->tickerInterval;
    l->actSound         = Friendly(DED_Definitions()->getSoundNum(def->actSound));
    l->deactSound       = Friendly(DED_Definitions()->getSoundNum(def->deactSound));
    l->evChain          = def->evChain;
    l->actChain         = def->actChain;
    l->deactChain       = def->deactChain;
    l->actLineType      = def->actLineType;
    l->deactLineType    = def->deactLineType;
    l->wallSection      = def->wallSection;

    if (def->actMaterial)
    {
        try
        {
            l->actMaterial = world::Materials::get().materialManifest(*def->actMaterial).id();
        }
        catch (const Resources::MissingResourceManifestError &)
        {}  // Ignore this error.
    }

    if (def->deactMaterial)
    {
        try
        {
            l->deactMaterial = world::Materials::get().materialManifest(*def->deactMaterial).id();
        }
        catch (const Resources::MissingResourceManifestError &)
        {}  // Ignore this error.
    }

    l->actMsg            = def->actMsg;
    l->deactMsg          = def->deactMsg;
    l->materialMoveAngle = def->materialMoveAngle;
    l->materialMoveSpeed = def->materialMoveSpeed;

    int i;
    LOOPi(20) l->iparm[i] = def->iparm[i];
    LOOPi(20) l->fparm[i] = def->fparm[i];
    LOOPi(5)  l->sparm[i] = def->sparm[i];

    // Some of the parameters might be strings depending on the line class.
    // Find the right mapping table.
    for (int k = 0; k < 20; ++k)
    {
        const int a = XG_Class(l->lineClass)->iparm[k].map;
        if (a < 0) continue;

        if (a & MAP_SND)
        {
            l->iparm[k] = Friendly(DED_Definitions()->getSoundNum(def->iparmStr[k]));
        }
        else if (a & MAP_MATERIAL)
        {
            if (def->iparmStr[k][0])
            {
                if (!iCmpStrCase(def->iparmStr[k], "-1"))
                {
                    l->iparm[k] = -1;
                }
                else
                {
                    try
                    {
                        l->iparm[k] = world::Materials::get().materialManifest(res::makeUri(def->iparmStr[k])).id();
                    }
                    catch (const Resources::MissingResourceManifestError &)
                    {}  // Ignore this error.
                }
            }
        }
        else if (a & MAP_MUS)
        {
            int temp = Friendly(DED_Definitions()->getMusicNum(def->iparmStr[k]));

            if (temp == 0)
            {
                temp = DED_Definitions()->evalFlags(def->iparmStr[k]);
                if (temp)
                    l->iparm[k] = temp;
            }
            else
            {
                l->iparm[k] = Friendly(DED_Definitions()->getMusicNum(def->iparmStr[k]));
            }
        }
        else
        {
            int temp = DED_Definitions()->evalFlags(def->iparmStr[k]);
            if (temp)
                l->iparm[k] = temp;
        }
    }
}

/**
 * Converts a DED sector type to the internal format.
 */
void Def_CopySectorType(sectortype_t *s, ded_sectortype_t *def)
{
    DE_ASSERT(s && def);
    int i, k;

    s->id           = def->id;
    s->flags        = def->flags;
    s->actTag       = def->actTag;
    LOOPi(5)
    {
        s->chain[i]      = def->chain[i];
        s->chainFlags[i] = def->chainFlags[i];
        s->start[i]      = def->start[i];
        s->end[i]        = def->end[i];
        LOOPk(2) s->interval[i][k] = def->interval[i][k];
        s->count[i]      = def->count[i];
    }
    s->ambientSound = Friendly(DED_Definitions()->getSoundNum(def->ambientSound));
    LOOPi(2)
    {
        s->soundInterval[i]     = def->soundInterval[i];
        s->materialMoveAngle[i] = def->materialMoveAngle[i];
        s->materialMoveSpeed[i] = def->materialMoveSpeed[i];
    }
    s->windAngle    = def->windAngle;
    s->windSpeed    = def->windSpeed;
    s->verticalWind = def->verticalWind;
    s->gravity      = def->gravity;
    s->friction     = def->friction;
    s->lightFunc    = def->lightFunc;
    LOOPi(2) s->lightInterval[i] = def->lightInterval[i];
    LOOPi(3)
    {
        s->colFunc[i] = def->colFunc[i];
        LOOPk(2) s->colInterval[i][k] = def->colInterval[i][k];
    }
    s->floorFunc    = def->floorFunc;
    s->floorMul     = def->floorMul;
    s->floorOff     = def->floorOff;
    LOOPi(2) s->floorInterval[i] = def->floorInterval[i];
    s->ceilFunc     = def->ceilFunc;
    s->ceilMul      = def->ceilMul;
    s->ceilOff      = def->ceilOff;
    LOOPi(2) s->ceilInterval[i] = def->ceilInterval[i];
}

int Def_Get(int type, const char *id, void *out)
{
    switch (type)
    {
    case DD_DEF_ACTION:
        if (acfnptr_t action = P_GetAction(id))
        {
            if (out) *(acfnptr_t *)out = action;
            return true;
        }
        return false;

    case DD_DEF_SOUND_LUMPNAME: {
        int32_t i = *((int32_t *) id);
        if (i < 0 || i >= ::runtimeDefs.sounds.size())
            return false;
        strcpy((char *)out, ::runtimeDefs.sounds[i].lumpName);
        return true; }

    case DD_DEF_LINE_TYPE: {
        int typeId = strtol(id, (char **)nullptr, 10);
        for (int i = DED_Definitions()->lineTypes.size() - 1; i >= 0; i--)
        {
            if (DED_Definitions()->lineTypes[i].id != typeId) continue;
            if (out) Def_CopyLineType((linetype_t *)out, &DED_Definitions()->lineTypes[i]);
            return true;
        }
        return false; }

    case DD_DEF_SECTOR_TYPE: {
        int typeId = strtol(id, (char **)nullptr, 10);
        for (int i = DED_Definitions()->sectorTypes.size() - 1; i >= 0; i--)
        {
            if (DED_Definitions()->sectorTypes[i].id != typeId) continue;
            if (out) Def_CopySectorType((sectortype_t *)out, &DED_Definitions()->sectorTypes[i]);
            return true;
        }
        return false; }

    default: return false;
    }
}

int Def_Set(int type, int index, int value, const void *ptr)
{
    LOG_AS("Def_Set");

    switch (type)
    {
    case DD_DEF_SOUND:
        if (index < 0 || index >= ::runtimeDefs.sounds.size())
        {
            DE_ASSERT_FAIL("Sound index is invalid");
            return false;
        }

        switch (value)
        {
        case DD_LUMP:
            S_StopSound(index, 0);
            strcpy(::runtimeDefs.sounds[index].lumpName, (const char *) ptr);
            if (strlen(::runtimeDefs.sounds[index].lumpName))
            {
                ::runtimeDefs.sounds[index].lumpNum = fileSys().lumpNumForName(::runtimeDefs.sounds[index].lumpName);
                if (::runtimeDefs.sounds[index].lumpNum < 0)
                {
                    LOG_RES_WARNING("Unknown sound lump name \"%s\"; sound #%i will be inaudible")
                            << ::runtimeDefs.sounds[index].lumpName << index;
                }
            }
            else
            {
                ::runtimeDefs.sounds[index].lumpNum = 0;
            }
            break;

        default: break;
        }
        break;

    default: return false;
    }

    return true;
}

/**
 * Prints a list of all the registered mobjs to the console.
 * @todo Does this belong here?
 */
D_CMD(ListMobjs)
{
    DE_UNUSED(src, argc, argv);

    if (DED_Definitions()->things.size() <= 0)
    {
        LOG_RES_MSG("No mobjtypes defined/loaded");
        return true;
    }

    LOG_RES_MSG(_E(b) "Registered Mobjs (ID | Name):");
    for (int i = 0; i < DED_Definitions()->things.size(); ++i)
    {
        const auto &name = DED_Definitions()->things[i].gets("name");
        if (!name.isEmpty())
            LOG_RES_MSG(" %s | %s") << DED_Definitions()->things[i].gets("id") << name;
        else
            LOG_RES_MSG(" %s | " _E(l) "(Unnamed)") << DED_Definitions()->things[i].gets("id");
    }

    return true;
}

void Def_ConsoleRegister()
{
    C_CMD("listmobjtypes", "", ListMobjs);
}

DE_DECLARE_API(Def) =
{
    { DE_API_DEFINITIONS },

    Def_Get,
    Def_Set
};
