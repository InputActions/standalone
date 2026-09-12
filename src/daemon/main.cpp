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

#include "Server.h"
#include "ServerHandler.h"
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QThread>
#include <csignal>
#include <grp.h>
#include <linux/prctl.h>
#include <sys/file.h>
#include <sys/prctl.h>
#include <sys/stat.h>

using namespace InputActions;

static const QDir VAR_RUN_INPUTACTIONS_DIR("/var/run/inputactions");
static const QString LOCK_FILE_PATH = VAR_RUN_INPUTACTIONS_DIR.path() + "/lock";

void handleSignal(int signal)
{
    if (signal == SIGINT) {
        QCoreApplication::quit();
        std::signal(SIGINT, SIG_DFL);
    }
}

void setfacl(const QString &target, QStringList arguments)
{
    arguments.push_back(target);

    QProcess process;
    process.setProgram("setfacl");
    process.setArguments(arguments);
    process.start();
    if (!process.waitForFinished()) {
        qWarning("setfacl failed: %s", process.errorString().toStdString().c_str());
    }
}

int main()
{
    if (geteuid()) {
        qCritical() << "The daemon must be run as root.";
        return -1;
    }

    auto *inputActionsGroup = getgrnam("inputactions");
    if (!inputActionsGroup) {
        qCritical() << "The 'inputactions' group does not exist.";
        return -1;
    }

    static int argc = 0;
    QCoreApplication app(argc, nullptr);
    std::signal(SIGINT, handleSignal);

    if (!VAR_RUN_INPUTACTIONS_DIR.exists()) {
        VAR_RUN_INPUTACTIONS_DIR.mkpath(".");
        chmod(VAR_RUN_INPUTACTIONS_DIR.path().toStdString().c_str(), 0755);
    }

    const auto fd = open(LOCK_FILE_PATH.toStdString().c_str(), O_RDWR | O_CREAT, 0600);
    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        if (errno == EWOULDBLOCK) {
            qCritical() << "A daemon instance is already running.";
            return -1;
        }
    }

    setfacl("/dev/input", {"-Rdm", "g:inputactions:rw"});
    setfacl("/dev/input", {"-Rm", "g:inputactions:rw"});
    setfacl("/dev/input", {"-m", "g:inputactions:rwx"});
    setfacl("/dev/uinput", {"-m", "g:inputactions:rw"});

    auto *serverThread = new QThread;
    auto *server = new Server;
    server->moveToThread(serverThread);

    ServerHandler serverHandler(*server, inputActionsGroup->gr_gid);

    QObject::connect(serverThread, &QThread::started, [server]() {
        QMetaObject::invokeMethod(server, "start");
    });
    serverThread->start();

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [server]() {
        server->deleteLater();
    });
    return app.exec();
}