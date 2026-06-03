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

#include <libevdev-cpp/UInputDevice.h>
#include <libinputactions/input/devices/VirtualMouse.h>

namespace InputActions
{

class EvdevVirtualMouse : public VirtualMouse
{
public:
    EvdevVirtualMouse();
    ~EvdevVirtualMouse() override;

    QString path() const;

    void mouseButton(MouseButton button, bool state) override;
    void mouseMotion(const PointF &pos) override;
    void mouseWheel(const PointF &delta) override;

private:
    std::optional<libevdev::UInputDevice> m_device;

    PointF m_motionDelta;
    PointF m_wheelDelta;
};

}