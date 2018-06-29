/** @file idgameslink.cpp
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/filesys/idgameslink.h"
#include "doomsday/filesys/idgamespackageinfofile.h"
#include "doomsday/DataBundle"

#include <de/Async>
#include <de/FileSystem>
#include <de/RemoteFile>
#include <de/data/gzip.h>

#include <de/RegExp>
//#include <QUrl>

using namespace de;

static String const DOMAIN_IDGAMES("idgames");
static String const CATEGORY_LEVELS("levels");
static String const CATEGORY_MUSIC ("music");
static String const CATEGORY_SOUNDS("sounds");
static String const CATEGORY_THEMES("themes");

DE_PIMPL(IdgamesLink)
{
    struct PackageIndexEntry : public PathTree::Node
    {
        FileEntry const *file = nullptr;
        Version version;

        PackageIndexEntry(PathTree::NodeArgs const &args) : Node(args) {}

        String descriptionPath() const
        {
            return file->path().toString().fileNameAndPathWithoutExtension() + ".txt";
        }
    };
    PathTreeT<PackageIndexEntry> packageIndex;

    String localRootPath;

    Impl(Public *i) : Base(i)
    {
        localRootPath = "/remote/" + QUrl(self().address()).host();
    }

    String packageIdentifierForFileEntry(FileEntry const &entry) const
    {
        if (entry.name().fileNameExtension() == DE_STR(".zip"))
        {
            Path const path = entry.path();
            String id = DataBundle::cleanIdentifier(path.fileName().fileNameWithoutExtension()) +
                        "_" + DataBundle::versionFromTimestamp(entry.modTime);

            // Remove the hour:minute part.
            id.truncate(id.sizeb() - 5);

            if (path.segment(1) == CATEGORY_MUSIC ||
                path.segment(1) == CATEGORY_SOUNDS ||
                path.segment(1) == CATEGORY_THEMES)
            {
                return String::format("%s.%s.%s",
                        DOMAIN_IDGAMES.c_str(),
                        path.segment(1).toString().c_str(),
                        id.c_str());
            }
            if (path.segment(1) == CATEGORY_LEVELS)
            {
                String subset;
                if      (path.segment(3) == DE_STR("deathmatch")) subset = DE_STR("deathmatch.");
                else if (path.segment(3) == DE_STR("megawads"))   subset = DE_STR("megawads.");
                return String::format("%s.%s.%s.%s%s",
                        DOMAIN_IDGAMES.c_str(),
                        CATEGORY_LEVELS.c_str(),
                        path.segment(2).toString().c_str(),
                        subset.c_str(),
                        id.c_str());
            }
            return String::format("%s.%s", DOMAIN_IDGAMES.c_str(), id.c_str());
        }
        else
        {
            return String();
        }
    }

    void buildPackageIndex()
    {
        packageIndex.clear();

        PathTreeIterator<FileTree> iter(self().fileTree().leafNodes());
        while (iter.hasNext())
        {
            auto const &fileEntry = iter.next();
            if (String pkg = packageIdentifierForFileEntry(fileEntry))
            {
                auto const id_ver = Package::split(pkg);
                auto &pkgEntry = packageIndex.insert(DotPath(id_ver.first));
                pkgEntry.file    = &fileEntry;
                pkgEntry.version = id_ver.second;
            }
        }

        debug("idgames package index has %zu entries", packageIndex.size());
    }

    PackageIndexEntry const *findPackage(String const &packageId) const
    {
        auto const id_ver = Package::split(packageId);
        if (auto *found = packageIndex.tryFind(DotPath(id_ver.first),
                                               PathTree::MatchFull | PathTree::NoBranch))
        {
            if (!id_ver.second.isValid() || found->version == id_ver.second)
            {
                return found;
            }
        }
        return nullptr;
    }

    RemoteFile &makeRemoteFile(Folder &folder, String const &remotePath, Block const &remoteMetaId)
    {
        auto *file = new RemoteFile(remotePath.fileName(),
                                    remotePath,
                                    remoteMetaId,
                                    self().address());
        folder.add(file);
        FS::get().index(*file);
        return *file;
    }
};

IdgamesLink::IdgamesLink(String const &address)
    : filesys::WebHostedLink(address, "ls-laR.gz")
    , d(new Impl(this))
{}

void IdgamesLink::parseRepositoryIndex(const Block &data)
{
    // This may be a long list, so let's do it in a background thread.
    // The link will be marked connected only after the data has been parsed.

    scope() += async([this, data] () -> String
    {
        const String listing = gDecompress(data);

        const RegExp reDir("^\\.?(.*):$");
        const RegExp reTotal("^total\\s+\\d+$");
        const RegExp reFile("^(-|d)[-rwxs]+\\s+\\d+\\s+\\w+\\s+\\w+\\s+"
                                        "(\\d+)\\s+(\\w+\\s+\\d+\\s+[0-9:]+)\\s+(.*)$",
                                        CaseInsensitive);
        String currentPath;
        bool ignore = false;
        const RegExp reIncludedPaths("^/(levels|music|sounds|themes)");

        std::unique_ptr<FileTree> tree(new FileTree);
        for (const auto &lineRef : listing.splitRef("\n"))
        {
            if (const String line = String(lineRef).strip())
            {
                if (!currentPath)
                {
                    // This should be a directory path.
                    auto match = reDir.match(line);
                    if (match.hasMatch())
                    {
                        currentPath = match.captured(1);
                        //qDebug() << "[WebRepositoryLink] Parsing path:" << currentPath;

                        ignore = !reIncludedPaths.match(currentPath).hasMatch();
                    }
                }
                else if (!ignore && reTotal.match(line).hasMatch())
                {
                    // Ignore directory sizes.
                }
                else if (!ignore)
                {
                    auto match = reFile.match(line);
                    if (match.hasMatch())
                    {
                        bool const isFolder = (match.captured(1) == DE_STR("d"));
                        if (!isFolder)
                        {
                            String const name = match.captured(4);
                            if (name.beginsWith('.') || name.contains(" -> "))
                                continue;

                            auto &entry = tree->insert((currentPath / name).lower());
                            entry.size = match.captured(2).toULongLong(nullptr, 10);;
                            entry.modTime = Time::fromText(match.captured(3), Time::UnixLsStyleDateTime);
                        }
                    }
                }
            }
            else
            {
                currentPath.clear();
            }
        }
        debug("idgames file tree contains %zu entries", tree->size());
        setFileTree(tree.release());
        return String();
    },
    [this] (String const &errorMessage)
    {
        if (!errorMessage)
        {
            wasConnected();
        }
        else
        {
            handleError("Failed to parse directory listing: " + errorMessage);
            wasDisconnected();
        }
});
}

StringList IdgamesLink::categoryTags() const
{
    return {CATEGORY_LEVELS, CATEGORY_MUSIC, CATEGORY_SOUNDS, CATEGORY_THEMES};
}

LoopResult IdgamesLink::forPackageIds(std::function<LoopResult (String const &)> func) const
{
    PathTreeIterator<Impl::PackageIndexEntry> iter(d->packageIndex.leafNodes());
    while (iter.hasNext())
    {
        if (auto result = func(iter.next().path('.')))
        {
            return result;
        }
    }
    return LoopContinue;
}

String IdgamesLink::findPackagePath(String const &packageId) const
{
    if (auto *found = d->findPackage(packageId))
    {
        return found->file->path();
    }
    return String();
}

filesys::Link *IdgamesLink::construct(String const &address)
{
    if ((address.beginsWith("http:") || address.beginsWith("https:")) &&
        !address.contains("dengine.net"))
    {
        return new IdgamesLink(address);
    }
    return nullptr;
}

File *IdgamesLink::populateRemotePath(String const &packageId,
                                      filesys::RepositoryPath const &path) const
{
    DE_ASSERT(path.link == this);

    auto *pkgEntry = d->findPackage(packageId);
    if (!pkgEntry)
    {
        DE_ASSERT(pkgEntry);
        return nullptr;
    }

    auto &pkgFolder = FS::get().makeFolder(path.localPath, FS::DontInheritFeeds);

    // The main data file of the package.
    auto &dataFile = d->makeRemoteFile(pkgFolder,
                                       pkgEntry->file->path(),
                                       pkgEntry->file->metaId(*this));
    dataFile.setStatus(File::Status(pkgEntry->file->size, pkgEntry->file->modTime));

    // Additional description.
    auto &txtFile = d->makeRemoteFile(pkgFolder,
                                      pkgEntry->descriptionPath(),
                                      md5Hash(address(), pkgEntry->descriptionPath(), pkgEntry->file->modTime));
    if (auto const *txtEntry = findFile(pkgEntry->descriptionPath()))
    {
        txtFile.setStatus(File::Status(txtEntry->size, txtEntry->modTime));
    }

    auto *infoFile = new IdgamesPackageInfoFile("info.dei");
    infoFile->setSourceFiles(dataFile, txtFile);
    pkgFolder.add(infoFile);
    FS::get().index(*infoFile);

    return &pkgFolder;
}

void IdgamesLink::setFileTree(FileTree *tree)
{
    WebHostedLink::setFileTree(tree);
    d->buildPackageIndex();
}
