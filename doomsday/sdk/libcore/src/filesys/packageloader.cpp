/** @file packageloader.cpp  Loads and unloads packages.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/PackageLoader"
#include "de/FS"
#include "de/App"
#include "de/Version"
#include "de/Info"
#include "de/Log"
#include "de/Parser"

#include <QMap>

namespace de {

DENG2_PIMPL(PackageLoader)
{
    LoadedPackages loaded;
    int loadCounter;

    Instance(Public *i) : Base(i), loadCounter(0)
    {}

    ~Instance()
    {
        // We own all loaded packages.
        qDeleteAll(loaded.values());
    }

    /**
     * Determines if a specific package is currently loaded.
     * @param file  Package root.
     * @return @c true, if loaded.
     */
    bool isLoaded(File const &file) const
    {
        LoadedPackages::const_iterator found = loaded.constFind(Package::identifierForFile(file));
        if(found != loaded.constEnd())
        {
            return &found.value()->file() == &file;
        }
        return false;
    }

    static int ascendingPackagesByLatest(File const *a, File const *b)
    {
        // The version must be specified using a format understood by Version.
        Version const aVer(a->objectNamespace().gets("package.version"));
        Version const bVer(b->objectNamespace().gets("package.version"));

        if(aVer == bVer)
        {
            // Identifical versions are prioritized by modification time.
            return de::cmp(a->status().modifiedAt, b->status().modifiedAt);
        }
        return aVer < bVer? -1 : 1;
    }

    struct PackageIdentifierDoesNotMatch
    {
        String matchId;

        PackageIdentifierDoesNotMatch(String const &id) : matchId(id) {}
        bool operator () (File *file) const {
            return Package::identifierForFile(*file) != matchId;
        }
    };

    int findAllVariants(String const &packageId, FS::FoundFiles &found) const
    {
        QStringList const components = packageId.split('.');

        String id;

        // The package may actually be inside other packages, so we need to check
        // each component of the package identifier.
        for(int i = components.size() - 1; i >= 0; --i)
        {
            id = components.at(i) + (!id.isEmpty()? "." + id : "");

            FS::FoundFiles files;
            App::fileSystem().findAllOfTypes(StringList()
                                             << DENG2_TYPE_NAME(Folder)
                                             << DENG2_TYPE_NAME(ArchiveFolder),
                                             id + ".pack", files);

            files.remove_if(PackageIdentifierDoesNotMatch(packageId));

            std::copy(files.begin(), files.end(), std::back_inserter(found));
        }

        return int(found.size());
    }

    /**
     * Parses or updates the metadata of a package, and checks it for validity.
     * A ValidationError is thrown if the package metadata does not comply with the
     * minimum requirements.
     *
     * @param packFile  Package file (".pack" folder).
     */
    static void checkPackage(File &packFile)
    {
        Package::parseMetadata(packFile);
        Package::validateMetadata(packFile.objectNamespace().subrecord("package"));
    }

    /**
     * Given a package identifier, pick one of the available versions of the package
     * based on predefined criteria.
     *
     * @param packageId  Package identifier.
     *
     * @return Selected package, or @c NULL if a version could not be selected.
     */
    File const *selectPackage(String const &packageId) const
    {
        LOG_AS("selectPackage");

        FS::FoundFiles found;
        if(!findAllVariants(packageId, found))
        {
            // None found.
            return 0;
        }

        // Each must have a version specified.
        DENG2_FOR_EACH_CONST(FS::FoundFiles, i, found)
        {
            checkPackage(**i);
        }

        found.sort(ascendingPackagesByLatest);

        LOG_RES_VERBOSE("Selected '%s': %s") << packageId << found.back()->description();

        return found.back();
    }

    Package &load(String const &packageId, File const &source)
    {
        if(loaded.contains(packageId))
        {
            throw AlreadyLoadedError("PackageLoader::load",
                                     "Package '" + packageId + "' is already loaded from \"" +
                                     loaded[packageId]->objectNamespace().gets("path") + "\"");
        }

        Package *pkg = new Package(source);
        loaded.insert(packageId, pkg);
        pkg->setOrder(loadCounter++);
        pkg->didLoad();
        return *pkg;
    }

    bool unload(String identifier)
    {
        LoadedPackages::iterator found = loaded.find(identifier);
        if(found == loaded.end()) return false;

        Package *pkg = found.value();
        pkg->aboutToUnload();
        delete pkg;

        loaded.remove(identifier);
        return true;
    }

    void listPackagesInIndex(FileIndex const &index, StringList &list)
    {
        for(auto i = index.begin(); i != index.end(); ++i)
        {
            if(i->first.fileNameExtension() == ".pack")
            {
                try
                {
                    File &file = *i->second;
                    String path = file.path();

                    // The special persistent data package should be ignored.
                    if(path == "/home/persist.pack") continue;

                    // Check the metadata.
                    checkPackage(file);

                    list.append(path);
                }
                catch(Package::ValidationError const &er)
                {
                    // Not a loadable package.
                    qDebug() << i->first << ": Package is invalid:" << er.asText();
                }
                catch(Parser::SyntaxError const &er)
                {
                    LOG_RES_NOTE("\"%s\": Package has a Doomsday Script syntax error: %s")
                            << i->first << er.asText();
                }
                catch(Info::SyntaxError const &er)
                {
                    // Not a loadable package.
                    LOG_RES_NOTE("\"%s\": Package has a syntax error: %s")
                            << i->first << er.asText();
                }

                /// @todo Store the errors so that the UI can show a list of
                /// problematic packages. -jk
            }
        }
    }

    DENG2_PIMPL_AUDIENCE(Activity)
    DENG2_PIMPL_AUDIENCE(Load)
    DENG2_PIMPL_AUDIENCE(Unload)
};

DENG2_AUDIENCE_METHOD(PackageLoader, Activity)
DENG2_AUDIENCE_METHOD(PackageLoader, Load)
DENG2_AUDIENCE_METHOD(PackageLoader, Unload)

PackageLoader::PackageLoader() : d(new Instance(this))
{}

Package const &PackageLoader::load(String const &packageId)
{
    LOG_AS("PackageLoader");

    File const *packFile = d->selectPackage(packageId);
    if(!packFile)
    {
        throw NotFoundError("PackageLoader::load",
                            "Package \"" + packageId + "\" is not available");
    }

    d->load(packageId, *packFile);

    try
    {
        DENG2_FOR_AUDIENCE2(Load, i)
        {
            i->packageLoaded(packageId);
        }
        DENG2_FOR_AUDIENCE2(Activity, i)
        {
            i->setOfLoadedPackagesChanged();
        }
    }
    catch(Error const &er)
    {
        // Someone took issue with the loaded package; cancel the load.
        unload(packageId);
        throw PostLoadError("PackageLoader::load",
                            "Error during post-load actions of package \"" +
                            packageId + "\": " + er.asText());
    }

    return package(packageId);
}

void PackageLoader::unload(String const &packageId)
{
    if(isLoaded(packageId))
    {
        DENG2_FOR_AUDIENCE2(Unload, i)
        {
            i->aboutToUnloadPackage(packageId);
        }

        d->unload(packageId);

        DENG2_FOR_AUDIENCE2(Activity, i)
        {
            i->setOfLoadedPackagesChanged();
        }
    }
}

void PackageLoader::unloadAll()
{
    LOG_AS("PackageLoader");
    LOG_RES_MSG("Unloading %i packages") << d->loaded.size();

    while(!d->loaded.isEmpty())
    {
        unload(d->loaded.firstKey());
    }
}

bool PackageLoader::isLoaded(String const &packageId) const
{
    return d->loaded.contains(packageId);
}

bool PackageLoader::isLoaded(File const &file) const
{
    return d->isLoaded(file);
}

PackageLoader::LoadedPackages const &PackageLoader::loadedPackages() const
{
    return d->loaded;
}

Package const &PackageLoader::package(String const &packageId) const
{
    if(!isLoaded(packageId))
    {
        throw NotFoundError("PackageLoader::package", "Package '" + packageId + "' is not loaded");
    }
    return *d->loaded[packageId];
}

namespace internal
{
    typedef std::pair<File *, int> FileAndOrder;

    static bool packageOrderLessThan(FileAndOrder const &a, FileAndOrder const &b) {
        return a.second < b.second;
    }
}

using namespace internal;

void PackageLoader::sortInPackageOrder(FS::FoundFiles &filesToSort) const
{
    // Find the packages for files.
    QList<FileAndOrder> all;
    DENG2_FOR_EACH_CONST(FS::FoundFiles, i, filesToSort)
    {
        Package const *pkg = 0;
        String identifier = Package::identifierForContainerOfFile(**i);
        if(isLoaded(identifier))
        {
            pkg = &package(identifier);
        }
        all << FileAndOrder(*i, pkg? pkg->order() : -1);
    }

    // Sort by package order.
    std::sort(all.begin(), all.end(), packageOrderLessThan);

    // Put the results back in the given array.
    filesToSort.clear();
    foreach(FileAndOrder const &f, all)
    {
        filesToSort.push_back(f.first);
    }
}

void PackageLoader::loadFromCommandLine()
{
    CommandLine &args = App::commandLine();

    for(int p = 0; p < args.count(); )
    {
        // Find all the -pkg options.
        if(!args.matches("-pkg", args.at(p)))
        {
            ++p;
            continue;
        }

        // Load all the specified packages (by identifier, not by path).
        while(++p != args.count() && !args.isOption(p))
        {
            load(args.at(p));
        }
    }
}

StringList PackageLoader::findAllPackages() const
{
    StringList all;
    d->listPackagesInIndex(App::fileSystem().indexFor(DENG2_TYPE_NAME(Folder)), all);
    d->listPackagesInIndex(App::fileSystem().indexFor(DENG2_TYPE_NAME(ArchiveFolder)), all);
    return all;
}

} // namespace de
