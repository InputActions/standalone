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

#include "ServerHandler.h"
#include "Server.h"
#include <QCoreApplication>
#include <QDBusArgument>
#include <QThread>
#include <csignal>
#include <libinputactions-standalone-common/helpers/Session.h>
#include <libinputactions-standalone-common/ipc/MessageSocketConnection.h>
#include <libinputactions-standalone-common/ipc/messages.h>
#include <libinputactions/globals.h>
#include <pwd.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <utmp.h>

namespace InputActions
{

ServerHandler::ServerHandler(Server &server, gid_t inputActionsGroupId)
    : m_inputActionsGroupId(inputActionsGroupId)
    , m_currentTty(SessionHelpers::currentTty())
    , m_freedesktopLoginDbusInterface("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", QDBusConnection::systemBus())
{
    connect(&server, &Server::messageReceived, this, &ServerHandler::onMessageReceived);

    connect(&m_ttyChangeDetectionTimer, &QTimer::timeout, this, &ServerHandler::onTtyChangeDetectionTimerTick);
    m_ttyChangeDetectionTimer.setInterval(1000);
    m_ttyChangeDetectionTimer.start();
}

void ServerHandler::onClientDisconnected(const ClientConnection &client)
{
    if (&client == m_currentClient) {
        m_currentClient = {};
    }
    m_clients.erase(client.tty);
    client.connection->deleteLater();
}

void ServerHandler::onMessageReceived(std::shared_ptr<const Message> message)
{
    switch (message->type()) {
        case MessageType::CInitializeRequest:
            initializeRequestMessage(std::dynamic_pointer_cast<const CInitializeRequestMessage>(message));
            break;
    }
}

void ServerHandler::onTtyChangeDetectionTimerTick()
{
    const auto tty = SessionHelpers::currentTty();
    if (m_currentTty == tty) {
        return;
    }

    qCDebug(INPUTACTIONS).noquote().nospace() << "TTY changed to " << tty;
    m_currentTty = tty;

    if (m_clients.contains(tty)) {
        activateClient(m_clients[tty]);
    } else {
        deactivateCurrentClient();
    }
}

void ServerHandler::initializeRequestMessage(std::shared_ptr<const CInitializeRequestMessage> message)
{
    auto response = message->makeResponse();

    ucred cred;
    socklen_t len = sizeof(struct ucred);

    if (getsockopt(message->sender()->socket()->socketDescriptor(), SOL_SOCKET, SO_PEERCRED, &cred, &len) == -1) {
        response.setError(QString("getsockopt failed: %1").arg(errno));
        message->reply(response);
        return;
    }

    if (cred.gid != m_inputActionsGroupId) {
        response.setError("inputactions-client is not running as the 'inputactions' group.");
        message->reply(response);
        return;
    }

    if (m_freedesktopLoginDbusInterface.isValid()) {
        const auto reply = m_freedesktopLoginDbusInterface.call("ListSessionsEx");
        if (reply.type() == QDBusMessage::MessageType::ErrorMessage) {
            response.setError(QString("ListSessionsEx call failed: %1").arg(reply.errorMessage()));
            message->reply(response);
            return;
        }

        if (reply.arguments().count() == 0) {
            response.setError("ListSessionEx returned no sessions.");
            message->reply(response);
            return;
        }

        bool success{};
        const auto sessionData = reply.arguments().at(0).value<QDBusArgument>();
        sessionData.beginArray();
        while (!sessionData.atEnd()) {
            bool b;
            QDBusObjectPath o;
            QString s;
            quint64 t;
            uint32_t u;

            uint32_t uid;
            QString tty;

            sessionData.beginStructure();
            sessionData >> s >> uid >> s >> s >> u >> s >> tty >> b >> t >> o;
            sessionData.endStructure();

            if (cred.uid == uid && message->tty() == tty) {
                success = true;
                break;
            }
        }
        sessionData.endArray();

        if (!success) {
            response.setError(QString("User logged into tty '%1' is different from the user who started inputactions-client.").arg(message->tty()));
            message->reply(response);
            return;
        }
    } else {
        QString ttyUser;
        setutent();
        utmp *entry;
        while ((entry = getutent()) != nullptr) {
            if (entry->ut_type == USER_PROCESS && message->tty() == entry->ut_line) {
                ttyUser = QString::fromLatin1(entry->ut_user, sizeof(entry->ut_user));
                break;
            }
        }
        endutent();

        if (ttyUser.isEmpty()) {
            response.setError(QString("Failed to get username of user logged into tty '%1'.").arg(message->tty()));
            message->reply(response);
            return;
        }

        passwd *pwd = getpwnam(ttyUser.toStdString().c_str());
        if (!pwd) {
            response.setError("Failed to get uid from username.");
            message->reply(response);
            return;
        }

        if (cred.uid != pwd->pw_uid) {
            response.setError(QString("User logged into tty '%1' is different from the user who started inputactions-client.").arg(message->tty()));
            message->reply(response);
            return;
        }
    }

    if (m_clients.contains(message->tty())) {
        response.setError(QString("Tty '%1' already has a running instance of inputactions-client.").arg(message->tty()));
        message->reply(response);
        return;
    }

    response.setSuccess(true);
    message->reply(response);

    const auto &tty = message->tty();
    auto &client = m_clients[tty] = {
        .connection = message->sender(),
        .tty = tty,
        .mainThreadId = message->mainThreadId(),
        .pid = cred.pid,
    };
    connect(message->sender()->socket(), &QLocalSocket::disconnected, this, [this, &client]() {
        onClientDisconnected(client);
    });

    if (tty == m_currentTty) {
        activateClient(client);
    }
}

void ServerHandler::activateClient(const ClientConnection &client)
{
    deactivateCurrentClient();

    const sched_param param{
        .sched_priority = sched_get_priority_min(SCHED_RR),
    };
    if (sched_setscheduler(client.mainThreadId, SCHED_RR | SCHED_RESET_ON_FORK, &param) != 0) {
        qWarning(INPUTACTIONS, "Failed to set real time thread priority: %s", strerror(errno));
    }

    if (!client.connection->sendMessageAndWaitForResponse(SActivateRequestMessage())) {
        qWarning(INPUTACTIONS, "Client with PID %d did not respond to activation request in time, assuming success.", client.pid);
    }
    m_currentClient = &client;
}

void ServerHandler::deactivateCurrentClient()
{
    if (!m_currentClient) {
        return;
    }

    if (m_currentClient->connection->sendMessageAndWaitForResponse(SDeactivateRequestMessage())) {
        const sched_param param{
            .sched_priority = 0,
        };
        sched_setscheduler(m_currentClient->mainThreadId, SCHED_OTHER, &param);
    } else {
        qWarning(INPUTACTIONS, "Client with PID %d did not respond to deactivation request in time, sending SIGKILL.", m_currentClient->pid);
        kill(m_currentClient->pid, SIGKILL);
        m_clients.erase(m_currentClient->tty);
    }

    m_currentClient = {};
}

}