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

#include <QDBusConnection>
#include <QObject>
#include <libinputactions/interfaces/PointerPositionGetter.h>
#include <libinputactions/interfaces/Window.h>
#include <libinputactions/interfaces/WindowProvider.h>

namespace InputActions
{

class Server;

class DBusWindow : public Window
{
public:
    std::optional<QString> id() override;
    std::optional<pid_t> pid() override;
    std::optional<QRectF> geometry() override;
    std::optional<QString> title() override;
    std::optional<QString> resourceClass() override;
    std::optional<QString> resourceName() override;
    std::optional<bool> maximized() override;
    std::optional<bool> fullscreen() override;

    std::optional<QString> m_id;
    std::optional<pid_t> m_pid;
    std::optional<QRectF> m_geometry;
    std::optional<QString> m_title;
    std::optional<QString> m_resourceClass;
    std::optional<QString> m_resourceName;
    std::optional<bool> m_maximized;
    std::optional<bool> m_fullscreen;
};

/**
 * Temporary interface, it will be rewritten in the future.
 */
class DBusEnvironmentStateProvider
    : public QObject
    , public PointerPositionGetter
    , public WindowProvider
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.inputactions.standalone.DBusEnvironmentStateProvider")

public:
    DBusEnvironmentStateProvider();
    ~DBusEnvironmentStateProvider() override;

    std::shared_ptr<Window> activeWindow() override;
    std::shared_ptr<Window> windowUnderPointer() override;

    std::optional<PointF> globalPointerPosition() override;
    std::optional<PointF> screenPointerPosition() override;

public slots:
    void updateState(const QString &json);

signals:
    void stateRequested();

private:
    std::shared_ptr<DBusWindow> m_activeWindow;
    std::shared_ptr<DBusWindow> m_windowUnderPointer;

    std::optional<PointF> m_globalPointerPosition;
    std::optional<PointF> m_screenPointerPosition;

    QDBusConnection m_bus;
};

}