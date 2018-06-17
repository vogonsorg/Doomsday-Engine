/** @file beacon.cpp  Presence service based on UDP broadcasts.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Beacon"
#include "de/Reader"
#include "de/Writer"
#include "de/LogBuffer"
#include "de/Map"
#include "de/Timer"
//#include <QUdpSocket>
//#include <QHostInfo>
//#include <QTimer>
//#include <QMap>

namespace de {

/**
 * Maximum number of Beacon UDP ports in simultaneous use at one machine, i.e.,
 * maximum number of servers on one machine.
 */
static duint16 const MAX_LISTEN_RANGE = 16;

// 1.0: Initial version.
// 1.1: Advertised message is compressed with zlib (deflate).
static const char *discoveryMessage = "Doomsday Beacon 1.1";

DE_PIMPL(Beacon)
{
    duint16 port;
    duint16 servicePort;
//    QUdpSocket *socket;
    Block message;
    std::unique_ptr<Timer> timer;
    Time discoveryEndsAt;
    Map<Address, Block> found;

    Impl(Public *i) : Base(i) {}

#if 0
    Impl() : socket(0), timer(0)
    {}

    ~Impl()
    {
        delete socket;
        delete timer;
    }
#endif

    void continueDiscovery()
    {
        DE_ASSERT(timer);
#if 0
        DE_ASSERT(socket);

        // Time to stop discovering?
        if (d->discoveryEndsAt.isValid() && Time() > d->discoveryEndsAt)
        {
            d->timer->stop();

            emit finished();

            d->socket->deleteLater();
            d->socket = 0;

            d->timer->deleteLater();
            d->timer = 0;
            return;
        }

        Block block(discoveryMessage);

        LOG_NET_XVERBOSE("Broadcasting %i bytes", block.size());

        // Send a new broadcast to the whole listening range of the beacons.
        for (duint16 range = 0; range < MAX_LISTEN_RANGE; ++range)
        {
            d->socket->writeDatagram(block,
                                     QHostAddress::Broadcast,
                                     d->port + range);
        }
#endif
    }

#if 0
void Beacon::readIncoming()
{
    LOG_AS("Beacon");

    if (!d->socket) return;

    while (d->socket->hasPendingDatagrams())
    {
        QHostAddress from;
        duint16 port = 0;
        Block block(d->socket->pendingDatagramSize());
        d->socket->readDatagram(reinterpret_cast<char *>(block.data()),
                                block.size(), &from, &port);

        LOG_NET_XVERBOSE("Received %i bytes from %s port %i", block.size() << from.toString() << port);

        if (block == discoveryMessage)
        {
            // Send a reply.
            d->socket->writeDatagram(d->message, from, port);
        }
    }
}

void Beacon::readDiscoveryReply()
{
    LOG_AS("Beacon");

    if (!d->socket) return;

    while (d->socket->hasPendingDatagrams())
    {
        QHostAddress from;
        quint16 port = 0;
        Block block(d->socket->pendingDatagramSize());
        d->socket->readDatagram(reinterpret_cast<char *>(block.data()),
                                block.size(), &from, &port);

        if (block == discoveryMessage)
            continue;

        try
        {
            // Remove the service listening port from the beginning.
            duint16 listenPort = 0;
            Reader(block) >> listenPort;
            block.remove(0, 2);
            block = block.decompressed();

            Address const host(from, listenPort);
            d->found.insert(host, block);

            emit found(host, block);
        }
        catch (Error const &)
        {
            // Bogus reply message, ignore.
        }
    }
}

#endif

    DE_PIMPL_AUDIENCES(Discovery, Finished)
};

DE_AUDIENCE_METHODS(Beacon, Discovery, Finished)

Beacon::Beacon(duint16 port) : d(new Impl(this))
{
    d->port = port;
}

duint16 Beacon::port() const
{
    return d->port;
}

void Beacon::start(duint16 serviceListenPort)
{
#if 0
    DE_ASSERT(!d->socket);

    d->servicePort = serviceListenPort;

    d->socket = new QUdpSocket;
    connect(d->socket, SIGNAL(readyRead()), this, SLOT(readIncoming()));

    for (duint16 attempt = 0; attempt < MAX_LISTEN_RANGE; ++attempt)
    {
        if (d->socket->bind(d->port + attempt, QUdpSocket::DontShareAddress))
        {
            d->port = d->port + attempt;
            return;
        }
    }

    /// @throws PortError Could not open the UDP port.
    throw PortError("Beacon::start", "Could not bind to UDP port " + String::asText(d->port));
#endif
}

void Beacon::setMessage(IByteArray const &advertisedMessage)
{
    d->message.clear();

    // Begin with the service listening port.
    Writer(d->message) << d->servicePort;

    d->message += Block(advertisedMessage).compressed();

    //qDebug() << "Beacon message:" << advertisedMessage.size() << d->message.size();
}

void Beacon::stop()
{
#if 0
    delete d->socket;
    d->socket = 0;
#endif
}

void Beacon::discover(TimeSpan timeOut, TimeSpan interval)
{
#if 0
    if (d->timer) return; // Already discovering.

    DE_ASSERT(!d->socket);

    d->socket = new QUdpSocket;
    connect(d->socket, SIGNAL(readyRead()), this, SLOT(readDiscoveryReply()));

    // Choose a semi-random port for listening to replies from servers' beacons.
    int tries = 10;
    for (;;)
    {
        if (d->socket->bind(d->port + Rangeui16(1, 0x4000).random(), QUdpSocket::DontShareAddress))
        {
            // Got a port open successfully.
            break;
        }
        if (!--tries)
        {
            /// @throws PortError Could not open the UDP port.
            throw PortError("Beacon::start", "Could not bind to UDP port " + String::asText(d->port));
        }
    }

    d->found.clear();

    // Time-out timer.
    if (timeOut > 0.0)
    {
        d->discoveryEndsAt = Time() + timeOut;
    }
    else
    {
        d->discoveryEndsAt = Time::invalidTime();
    }
    d->timer.reset(new Timer);
    d->timer->audienceForTrigger() += [this](){ d->continueDiscovery(); };
    d->timer->start(interval);

    d->continueDiscovery();
#endif
}

List<Address> Beacon::foundHosts() const
{
    return map<List<Address>>(d->found, [](const std::pair<Address, Block> &v){
        return v.first;
    });
}

Block Beacon::messageFromHost(Address const &host) const
{
    if (!d->found.contains(host)) return Block();
    return d->found[host];
}

} // namespace de
