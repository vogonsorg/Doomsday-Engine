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
 
#ifndef LIBDENG2_PROTOCOL_H
#define LIBDENG2_PROTOCOL_H

#include <de/deng.h>

#include <list>

/**
 * @defgroup protocol Protocol
 *
 * Classes that define the protocol for network communications.
 * 
 * @ingroup net
 */

namespace de
{
    class Block;
    class Link;
    class Packet;
    class CommandPacket;
    class RecordPacket;
    
    /**
     * The protocol is responsible for recognizing an incoming data packet and
     * constructing a specialized packet object of the appropriate type.
     *
     * @ingroup protocol
     */
    class PUBLIC_API Protocol
    {
    public:
        /// The response was not success. @ingroup errors
        DEFINE_ERROR(ResponseError);
        
        /// The response to a decree was FAILURE. @ingroup errors
        DEFINE_SUB_ERROR(ResponseError, FailureError);
        
        /// The response to a decree was DENY. @ingroup errors
        DEFINE_SUB_ERROR(ResponseError, DenyError);
        
        /**
         * A constructor function examines a block of data and determines 
         * whether a specialized Packet can be constructed based on the data.
         *
         * @param block  Block of data.
         *
         * @return  Specialized Packet, or @c NULL.
         */
        typedef Packet* (*Constructor)(const Block&);

        /// Reply types. @see reply()
        enum Reply {
            OK,         ///< Command performed successfully.
            FAILURE,    ///< Command failed.
            DENY        ///< Permission denied. No rights to perform the command.
        };
        
    public:
        Protocol();
        
        ~Protocol();
        
        /**
         * Registers a new constructor function.
         *
         * @param constructor  Constructor.
         */
        void define(Constructor constructor);
        
        /**
         * Interprets a block of data.
         *
         * @param block  Block of data that should contain a Packet of some type.
         *
         * @return  Specialized Packet, or @c NULL.
         */
        Packet* interpret(const Block& block) const;

        /**
         * Sends a command packet and waits for reply. This is intended for issuing
         * commands that rarely fail.
         *
         * @param to  Link over which to converse.
         * @param command  Packet to send.
         * @param response  If not NULL, the reponse packet is returned to caller here.
         *      Otherwise the response packet is deleted.
         *
         * @return  Response from the remote end. Caller gets ownership of the packet.
         */
        void decree(Link& to, const CommandPacket& command, RecordPacket** response = 0);

        /**
         * Sends a reply over a link. This is used as a general response to 
         * commands or any messages received from the link.
         *
         * @param to  Link where to send the reply.
         * @param type  Type of reply.
         * @param message  Optional message (human readable).
         */
        void reply(Link& to, Reply type = OK, const std::string& message = "");

    private:
        typedef std::list<Constructor> Constructors;
        Constructors constructors_;
    };
}

#endif /* LIBDENG2_PROTOCOL_H */
