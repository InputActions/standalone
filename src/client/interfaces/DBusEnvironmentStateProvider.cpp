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

#include "DBusEnvironmentStateProvider.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <libinputactions/helpers/QDBusConnection.h>

namespace InputActions
{

static const QString DBUS_OBJECT_PATH = "/org/inputactions/standalone/DBusEnvironmentStateProvider";

DBusEnvironmentStateProvider::DBusEnvironmentStateProvider()
    : m_activeWindow(std::make_unique<DBusWindow>())
    , m_windowUnderPointer(std::make_unique<DBusWindow>())
    , m_bus(QDBusConnectionHelpers::sessionBus())
{
    m_bus.registerObject(DBUS_OBJECT_PATH, this, QDBusConnection::ExportAllContents);
}

DBusEnvironmentStateProvider::~DBusEnvironmentStateProvider()
{
    m_bus.unregisterObject(DBUS_OBJECT_PATH);
}

std::shared_ptr<Window> DBusEnvironmentStateProvider::activeWindow()
{
    return m_activeWindow;
}

std::shared_ptr<Window> DBusEnvironmentStateProvider::windowUnderPointer()
{
    return m_windowUnderPointer;
}

void DBusEnvironmentStateProvider::updateState(const QString &json)
{
    const auto jsonDocument = QJsonDocument::fromJson(json.toUtf8());
    const auto object = jsonDocument.object();

    static const auto readBool = [](auto &member, const QJsonValue &jsonValue) {
        if (jsonValue.isBool()) {
            member = jsonValue.toBool();
        }
    };
    static const auto readInt = [](auto &member, const QJsonValue &jsonValue) {
        if (jsonValue.isDouble()) {
            member = jsonValue.toInteger();
        }
    };
    static const auto readPoint = [](auto &member, const QJsonValue &jsonValue) {
        if (jsonValue.isArray()) {
            const auto array = jsonValue.toArray();
            if (array.size() == 2 && array[0].isDouble() && array[1].isDouble()) {
                member = {array[0].toDouble(), array[1].toDouble()};
            }
        }
    };
    static const auto readRect = [](auto &member, const QJsonValue &jsonValue) {
        if (jsonValue.isArray()) {
            const auto array = jsonValue.toArray();
            if (array.size() == 4 && array[0].isDouble() && array[1].isDouble() && array[2].isDouble() && array[3].isDouble()) {
                member = {array[0].toDouble(), array[1].toDouble(), array[2].toDouble(), array[3].toDouble()};
            }
        }
    };
    static const auto readString = [](auto &member, const QJsonValue &jsonValue) {
        if (jsonValue.isString()) {
            member = jsonValue.toString();
        }
    };

    readString(m_activeWindow->m_id, object["active_window_id"]);
    readInt(m_activeWindow->m_pid, object["active_window_pid"]);
    readString(m_activeWindow->m_resourceClass, object["active_window_class"]);
    readBool(m_activeWindow->m_fullscreen, object["active_window_fullscreen"]);
    readBool(m_activeWindow->m_maximized, object["active_window_maximized"]);
    readString(m_activeWindow->m_resourceName, object["active_window_name"]);
    readString(m_activeWindow->m_title, object["active_window_title"]);

    readString(m_windowUnderPointer->m_id, object["window_under_pointer_id"]);
    readInt(m_windowUnderPointer->m_pid, object["window_under_pointer_pid"]);
    readString(m_windowUnderPointer->m_resourceClass, object["window_under_pointer_class"]);
    readBool(m_windowUnderPointer->m_fullscreen, object["window_under_pointer_fullscreen"]);
    readRect(m_windowUnderPointer->m_geometry, object["window_under_pointer_geometry"]);
    readBool(m_windowUnderPointer->m_maximized, object["window_under_pointer_maximized"]);
    readString(m_windowUnderPointer->m_resourceName, object["window_under_pointer_name"]);
    readString(m_windowUnderPointer->m_title, object["window_under_pointer_title"]);

    readPoint(m_globalPointerPosition, object["pointer_position_global"]);
    readPoint(m_screenPointerPosition, object["pointer_position_screen_percentage"]);
}

std::optional<QString> DBusWindow::id()
{
    return m_id;
}

std::optional<pid_t> DBusWindow::pid()
{
    return m_pid;
}

std::optional<QRectF> DBusWindow::geometry()
{
    return m_geometry;
}

std::optional<QString> DBusWindow::title()
{
    return m_title;
}

std::optional<QString> DBusWindow::resourceClass()
{
    return m_resourceClass;
}

std::optional<QString> DBusWindow::resourceName()
{
    return m_resourceName;
}

std::optional<bool> DBusWindow::maximized()
{
    return m_maximized;
}

std::optional<bool> DBusWindow::fullscreen()
{
    return m_fullscreen;
}

std::optional<PointF> DBusEnvironmentStateProvider::globalPointerPosition()
{
    return m_globalPointerPosition;
}

std::optional<PointF> DBusEnvironmentStateProvider::screenPointerPosition()
{
    return m_screenPointerPosition;
}

}