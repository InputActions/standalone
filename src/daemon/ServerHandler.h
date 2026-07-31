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

#pragma once

#include <QDBusInterface>
#include <QThread>
#include <libinputactions-standalone-common/ipc/messages.h>
#include <sys/types.h>

namespace InputActions
{

class Message;
class Server;

struct ClientConnection
{
    MessageSocketConnection *connection;
    QString tty;
    int64_t mainThreadId;
    pid_t pid;
};

class ServerHandler : public QObject
{
    Q_OBJECT

public:
    ServerHandler(Server &server, gid_t inputActionsGroupId);

private slots:
    void onClientDisconnected(const ClientConnection &client);
    void onMessageReceived(std::shared_ptr<const Message> message);
    void onTtyChangeDetectionTimerTick();

private:
    void initializeRequestMessage(std::shared_ptr<const CInitializeRequestMessage> message);

    void activateClient(const ClientConnection &client);
    void deactivateCurrentClient();

    std::map<QString, ClientConnection> m_clients;
    const ClientConnection *m_currentClient{};

    gid_t m_inputActionsGroupId;

    QTimer m_ttyChangeDetectionTimer;
    QString m_currentTty;

    QDBusInterface m_freedesktopLoginDbusInterface;
};

}