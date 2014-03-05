/** @file savedsessionrepository.h  Saved (game) session repository.
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

#ifndef LIBDENG2_SAVEDSESSIONREPOSITORY_H
#define LIBDENG2_SAVEDSESSIONREPOSITORY_H

#include "../Error"
#include "../Folder"
#include "../game/IGameStateReader"
#include "../Path"
#include "../SavedSession"
#include "../String"

/**
 * Game state recognizer function ptr.
 *
 * The job of a recognizer function is to determine whether the game state associated
 * with the given saved @a session is interpretable as a potentially loadable game state.
 *
 * @param metadata  The read metadata is written here.
 */
typedef bool (*GameStateRecognizeFunc)(de::Path const &stateFilePath, de::SessionMetadata &metadata);

/// Game state reader instantiator function ptr.
typedef de::IGameStateReader *(*GameStateReaderMakeFunc)();

namespace de {

/**
 * Centralized saved session repository. The saved game state file structure is automatically
 * initialized when the current game changes.
 *
 * @ingroup core
 */
class DENG2_PUBLIC SavedSessionRepository
{
public:
    /// Required/referenced session is missing. @ingroup errors
    DENG2_ERROR(MissingSessionError);

public:
    SavedSessionRepository();

    /**
     * @param location  Path to the new root of the saved session repository.
     */
    void setLocation(Path const &location);

    /**
     * Returns the root folder of the saved session repository file structure.
     */
    Folder const &folder() const;

    /// @copydoc folder()
    Folder &folder();

    /**
     * Determines whether a saved session exists for @a fileName.
     *
     * @see session()
     */
    bool contains(String fileName) const;

    /**
     * Add/replace a saved session in the repository. If a session already exists, it is
     * replaced by the new one.
     *
     * @param fileName    Name of the associated saved session game state file.
     * @param newSession  New saved session to replace with. Ownership is given.
     */
    void add(String fileName, SavedSession *session);

    /**
     * Lookup the SavedSession for @a fileName.
     *
     * @see contains()
     */
    SavedSession &session(String fileName) const;

    /**
     * Register a game state reader.
     *
     * @param recognizer  Game state recognizer function.
     * @param maker       Game state reader instantiator function.
     */
    void declareReader(GameStateRecognizeFunc recognizer, GameStateReaderMakeFunc maker);

    /**
     * Determines whether a IGameStateReader appropriate for the specified saved @a session
     * is available and if so, reads the session metadata.
     *
     * @param record  The session metadata will be written here if recognized.
     *
     * @return  @c true= the session metadata was read successfully.
     *
     * @see recognizeAndMakeReader()
     */
    bool recognize(SavedSession &session) const;

    /**
     * Determines whether a IGameStateReader appropriate for the specified saved @a session
     * is available and if so, reads the session metadata and returns a new reader instance
     * for deserializing the game state.
     *
     * @param record  The session metadata will be written here if recognized.
     *
     * @return  New reader instance if recognized; otherwise @c 0. Ownership given to the caller.
     *
     * @see recognize()
     */
    IGameStateReader *recognizeAndMakeReader(SavedSession &session) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_SAVEDSESSIONREPOSITORY_H