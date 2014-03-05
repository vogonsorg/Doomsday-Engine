/** @file savedsession.cpp  Saved (game) session.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#include "de/Savedsession"

#include "de/App"
#include "de/game/Game"
#include "de/ArrayValue"
#include "de/Log"
#include "de/NumberValue"
#include "de/NativePath"
#include "de/SavedSessionRepository"

namespace de {

using namespace internal;

DENG2_PIMPL(SavedSession)
{
    SavedSessionRepository *repo; ///< The owning repository (if any).

    String fileName; ///< Name of the game session file.

    QScopedPointer<Metadata> metadata;
    Status status;
    bool needUpdateStatus;

    Instance(Public *i)
        : Base(i)
        , repo            (0)
        , metadata        (new Metadata)
        , status          (Unused)
        , needUpdateStatus(true)
    {}

    Instance(Public *i, Instance const &other)
        : Base(i)
        , repo            (other.repo)
        , fileName        (other.fileName)
        , status          (other.status)
        , needUpdateStatus(other.needUpdateStatus)
    {
        if(!other.metadata.isNull())
        {
            metadata.reset(new Metadata(*other.metadata));
        }
    }

    void notifyMetadataChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(MetadataChange, i)
        {
            i->savedSessionMetadataChanged(self);
        }
    }

    void updateStatusIfNeeded()
    {
        if(!needUpdateStatus) return;

        needUpdateStatus = false;
        LOGDEV_XVERBOSE("Updating SavedSession %p status") << thisPublic;
        Status const oldStatus = status;

        status = Unused;
        if(self.hasGameState())
        {
            status = Incompatible;
            // Game identity key missmatch?
            if(!(*metadata)["gameIdentityKey"].value().asText().compareWithoutCase(App::game().id()))
            {
                /// @todo Validate loaded add-ons and checksum the definition database.
                status = Loadable; // It's good!
            }
        }

        if(status != oldStatus)
        {
            DENG2_FOR_PUBLIC_AUDIENCE(StatusChange, i)
            {
                i->savedSessionStatusChanged(self);
            }
        }
    }
};

SavedSession::SavedSession(String const &fileName) : d(new Instance(this))
{
    d->fileName = fileName;
}

SavedSession::SavedSession(SavedSession const &other) : d(new Instance(this, *other.d))
{}

SavedSession &SavedSession::operator = (SavedSession const &other)
{
    d.reset(new Instance(this, *other.d));
    return *this;
}

SavedSessionRepository &SavedSession::repository() const
{
    if(d->repo)
    {
        return *d->repo;
    }
    /// @throw MissingRepositoryError No repository is configured.
    throw MissingRepositoryError("SavedSession::repository", "No repository is configured");
}

void SavedSession::setRepository(SavedSessionRepository *newRepository)
{
    d->repo = newRepository;
}

SavedSession::Status SavedSession::status() const
{
    d->updateStatusIfNeeded();
    return d->status;
}

String SavedSession::statusAsText() const
{
    static String const statusTexts[] = {
        "Loadable",
        "Incompatible",
        "Unused",
    };
    return statusTexts[int(status())];
}

static String metadataAsStyledText(SavedSession::Metadata const &metadata)
{
    return String(_E(b) "%1\n" _E(.)
                  _E(l) "IdentityKey: " _E(.)_E(i) "%2 "  _E(.)
                  _E(l) "Current map: " _E(.)_E(i) "%3\n" _E(.)
                  _E(l) "Version: "     _E(.)_E(i) "%4 "  _E(.)
                  _E(l) "Session id: "  _E(.)_E(i) "%5\n" _E(.)
                  _E(D) "Game rules:\n" _E(.) "  %6")
             .arg(metadata["userDescription"].value().asText())
             .arg(metadata["gameIdentityKey"].value().asText())
             .arg(metadata["mapUri"].value().asText())
             .arg(metadata["version"].value().asNumber())
             .arg(metadata["sessionId"].value().asNumber())
             .arg(metadata["gameRules"].value().asText());
}

String SavedSession::description() const
{
    return metadataAsStyledText(metadata()) + "\n" +
           String(_E(l) "Source file: " _E(.)_E(i) "\"%1\"\n" _E(.)
                  _E(D) "Status: " _E(.) "%2")
               .arg(NativePath(repository().folder().path() / fileName()).pretty())
               .arg(statusAsText());
}

String SavedSession::fileName() const
{
    return d->fileName + ".save";
}

void SavedSession::setFileName(String newName)
{
    if(d->fileName != newName)
    {
        d->fileName         = newName;
        d->needUpdateStatus = true;
    }
}

String SavedSession::fileNameForMap(String mapUriStr) const
{
    return d->fileName + mapUriStr + ".save";
}

bool SavedSession::hasGameState() const
{
    return repository().folder().has(fileName());
}

bool SavedSession::hasMapState(String mapUriStr) const
{
    return repository().folder().has(fileNameForMap(mapUriStr));
}

void SavedSession::updateFromRepository()
{
    LOGDEV_VERBOSE("Updating SavedSession %p from the repository") << this;

    // Is this a recognized game state?
    if(repository().recognize(*this))
    {
        // Ensure we have a valid description.
        if((*d->metadata)["userDescription"].value().asText().isEmpty())
        {
            d->metadata->set("userDescription", "UNNAMED");
            d->notifyMetadataChanged();
        }
    }
    else
    {
        // Unrecognized or the file could not be accessed (perhaps its a network path?).
        // Return the session to the "null/invalid" state.
        d->metadata->set("userDescription", "");
        d->metadata->set("sessionId", duint32(0));
        d->notifyMetadataChanged();
    }

    d->updateStatusIfNeeded();
}

void SavedSession::deleteFilesInRepository()
{
    FS::FoundFiles files;
    App::fileSystem().findAll(repository().folder().path(), files);
    DENG2_FOR_EACH(FS::FoundFiles, i, files)
    {
        if(File *file = (*i)->maybeAs<File>())
        {
            if(file->name().fileNameExtension() == ".save" &&
               file->name().beginsWith(d->fileName))
            {
                // Remove this file.
                delete file;
            }
        }
    }

    // Force a status update.
    updateFromRepository();
}

SavedSession::Metadata const &SavedSession::metadata() const
{
    DENG2_ASSERT(!d->metadata.isNull());
    return *d->metadata;
}

void SavedSession::replaceMetadata(Metadata *newMetadata)
{
    DENG2_ASSERT(newMetadata != 0);
    d->metadata.reset(newMetadata);
    d->needUpdateStatus = true;
}

std::auto_ptr<IGameStateReader> SavedSession::gameStateReader()
{
    std::auto_ptr<IGameStateReader> p(repository().recognizeAndMakeReader(*this));
    if(!p.get())
    {
        /// @throw UnrecognizedGameStateError The game state format was not recognized.
        throw UnrecognizedGameStateError("SavedSession::gameStateReader", "Unrecognized game state format");
    }
    return p;
}

} // namespace de