/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/Session"
#include "de/App"
#include "de/Library"
#include "de/Protocol"
#include "de/World"
#include "de/CommandPacket"
#include "de/Record"

using namespace de;

Session::Session() : world_(0)
{
    // Create a blank world.
    world_ = App::game().SYMBOL(deng_NewWorld)();
}

Session::~Session()
{
    delete world_;
}

void Session::processCommand(Link& sender, const CommandPacket& packet)
{
    if(packet.command() == "session.new")
    {
        // Initialize the session with the provided settings.
        try
        {
            world_->setMap(packet.arguments()["map"].value().asText());
            // Respond.
            App::protocol().reply(sender);
        }
        catch(const Error& err)        
        {
            App::protocol().reply(sender, Protocol::FAILURE, err.what());
        }
    }
}
