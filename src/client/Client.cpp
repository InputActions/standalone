/*
    Input Actions - Input handler that executes user-defined actions
    Copyright (C) 2024-2026 Marcin Woźniak

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "Client.h"
#include <libinputactions-standalone-common/ipc/MessageSocketConnection.h>

namespace InputActions
{

void Client::start()
{
    m_connectionRetryTimer = new QTimer(this);
    connect(m_connectionRetryTimer, &QTimer::timeout, this, &Client::connectToDaemon);
    m_connectionRetryTimer->setInterval(1000);

    auto socket = new QLocalSocket(this);
    m_connection = new MessageSocketConnection(socket, this);

    connect(socket, &QLocalSocket::connected, this, &Client::onConnected);
    connect(socket, &QLocalSocket::errorOccurred, this, &Client::onErrorOccurred);
    connect(socket, &QLocalSocket::disconnected, this, &Client::onDisconnected);
    connect(m_connection, &MessageSocketConnection::messageReceived, this, &Client::messageReceived);
    socket->connectToServer(INPUTACTIONS_IPC_SOCKET_PATH);
}

void Client::onConnected()
{
    m_connectionRetryTimer->stop();
    Q_EMIT connected(m_connection);
}

void Client::onDisconnected()
{
    m_connectionRetryTimer->start();
    Q_EMIT disconnected();
}

void Client::onErrorOccurred(QLocalSocket::LocalSocketError error)
{
    qCDebug(INPUTACTIONS_IPC).noquote().nospace() << "Failed to connect to server: " << error;
    m_connectionRetryTimer->start();
}

void Client::connectToDaemon()
{
    m_connection->socket()->connectToServer(INPUTACTIONS_IPC_SOCKET_PATH);
}

}