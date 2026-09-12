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
#include "ClientHandler.h"
#include "gnome/GNOMEClient.h"
#include "input/StandaloneInputBackend.h"
#include "interfaces/DBusEnvironmentStateProvider.h"
#include "plasma/PlasmaClient.h"
#include "wayland/WaylandClient.h"
#include <QCoreApplication>
#include <QFile>
#include <QThread>
#include <csignal>
#include <libinputactions/InputActionsMain.h>
#include <libinputactions/interfaces/PointerPositionGetter.h>
#include <libinputactions/interfaces/WindowProvider.h>

using namespace InputActions;

void handleSignal(int signal)
{
    if (signal == SIGINT) {
        QCoreApplication::quit();
        std::signal(SIGINT, SIG_DFL);
    }
}

int main()
{
    static int argc = 0;
    QCoreApplication app(argc, nullptr);
    std::signal(SIGINT, handleSignal);

    InputActionsMain main;
    g_inputBackend = std::make_unique<StandaloneInputBackend>();

    auto dbusEnvironmentStateProvider = std::make_shared<DBusEnvironmentStateProvider>();
    g_pointerPositionGetter = dbusEnvironmentStateProvider;
    g_windowProvider = dbusEnvironmentStateProvider;

    main.setMissingImplementations();
    main.initialize();

    Q_EMIT dbusEnvironmentStateProvider->stateRequested();

    auto *clientThread = new QThread;
    auto *client = new Client;
    ClientHandler clientHandler(*client);

    client->moveToThread(clientThread);
    QObject::connect(clientThread, &QThread::started, [&client]() {
        QMetaObject::invokeMethod(client, "start");
    });
    clientThread->start();

    GNOMEClient gnomeClient;
    PlasmaClient plasmaClient;
    WaylandClient waylandClient(*dbusEnvironmentStateProvider);
    gnomeClient.initialize() || plasmaClient.initialize() || waylandClient.initialize();

    return app.exec();
}