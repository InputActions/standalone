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

#include "ClientHandler.h"
#include "Client.h"
#include <QCoreApplication>
#include <QThread>
#include <libinputactions-standalone-common/helpers/Session.h>
#include <libinputactions-standalone-common/ipc/MessageSocketConnection.h>
#include <libinputactions-standalone-common/ipc/messages.h>
#include <libinputactions/InputActionsMain.h>
#include <libinputactions/config/ConfigLoader.h>
#include <libinputactions/config/GlobalConfig.h>
#include <libinputactions/dbus/MainDBusInterface.h>
#include <libinputactions/globals.h>
#include <pwd.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utmp.h>

namespace InputActions
{

ClientHandler::ClientHandler(Client &client)
    : m_tty(SessionHelpers::currentTty())
{
    connect(&client, &Client::connected, this, &ClientHandler::onConnected);
    connect(&client, &Client::disconnected, this, &ClientHandler::onDisconnected);
    connect(&client, &Client::messageReceived, this, &ClientHandler::onMessageReceived);
}

void ClientHandler::onConnected(MessageSocketConnection *connection)
{
    CInitializeRequestMessage initializeRequest;
    initializeRequest.setTty(m_tty);
    initializeRequest.setMainThreadId(gettid());

    const auto response = connection->sendMessageAndWaitForResponse(initializeRequest);
    if (!response) {
        qCritical("Daemon did not respond to initialization request in time.");
        QCoreApplication::exit(-1);
    } else if (!response->success()) {
        qCritical().noquote().nospace() << response->error();
        qCritical("Initialization request rejected by daemon.");
        QCoreApplication::exit(-1);
    }
}

void ClientHandler::onDisconnected()
{
    g_inputActions->suspend();
}

void ClientHandler::onMessageReceived(std::shared_ptr<const Message> message)
{
    switch (message->type()) {
        case MessageType::SActivateRequest:
            activateRequestMessage(std::dynamic_pointer_cast<const SActivateRequestMessage>(message));
            break;
        case MessageType::SDeactivateRequest:
            deactivateRequestMessage(std::dynamic_pointer_cast<const SDeactivateRequestMessage>(message));
            break;
    }
}

void ClientHandler::activateRequestMessage(std::shared_ptr<const SActivateRequestMessage> message)
{
    g_mainDbusInterface->setAllowConfigLoading(true);
    g_configLoader->load();
    message->reply();
}

void ClientHandler::deactivateRequestMessage(std::shared_ptr<const SDeactivateRequestMessage> message)
{
    g_mainDbusInterface->setAllowConfigLoading(false);
    g_inputActions->suspend();
    g_globalConfig->setAutoReload(false);
    message->reply();
}

}