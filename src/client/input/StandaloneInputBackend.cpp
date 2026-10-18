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

#include "StandaloneInputBackend.h"
#include "StandaloneInputDevice.h"
#include <libevdev-cpp/Device.h>
#include <libevdev-cpp/UInputDevice.h>
#include <libinput-cpp/Device.h>
#include <libinput-cpp/Event.h>
#include <libinput-cpp/GestureEvent.h>
#include <libinput-cpp/KeyboardEvent.h>
#include <libinput-cpp/PointerEvent.h>
#include <libinput-cpp/TouchEvent.h>
#include <libinputactions/input/devices/InputDeviceState.h>
#include <libinputactions/input/events.h>
#include <libudev-cpp/Device.h>
#include <libudev-cpp/Enumerate.h>
#include <libudev-cpp/ListEntry.h>
#include <libudev-cpp/Monitor.h>
#include <libudev-cpp/Udev.h>
#include <linux/uinput.h>
#include <unistd.h>

namespace InputActions
{

static constexpr uint32_t MAX_INITIALIZATION_ATTEMPTS = 5;

StandaloneInputBackend::StandaloneInputBackend()
{
    connect(&m_deviceInitializationRetryTimer, &QTimer::timeout, this, &StandaloneInputBackend::deviceInitializationRetryTimerTick);
    m_deviceInitializationRetryTimer.setInterval(1000);
}

StandaloneInputBackend::~StandaloneInputBackend()
{
    reset();
}

void StandaloneInputBackend::doInitialize()
{
    LibinputInputBackend::doInitialize();

    m_virtualKeyboard.emplace(virtualKeyboardKeys());
    m_virtualMouse.emplace();

    libudev::Enumerate enumerate(m_udev);
    enumerate.addMatchSubsystem("input");
    enumerate.addMatchSysName("event*");
    enumerate.scanDevices();

    for (const auto &entry : enumerate.listEntries()) {
        if (!entry.name()) {
            continue;
        }

        try {
            const auto device = libudev::Device::newFromSysPath(m_udev, entry.name().value());
            evdevDeviceAdded(device.devNode());
        } catch (...) {
        }
    }

    m_udevMonitor = libudev::Monitor::newFromNetlink(m_udev, "udev");
    m_udevMonitor->filterAddMatchSubsystemDevType("input");
    connect(m_udevMonitor.get(), &libudev::Monitor::deviceAdded, this, [this](const auto &device) {
        evdevDeviceAdded(device.devNode());
    });
    connect(m_udevMonitor.get(), &libudev::Monitor::deviceRemoved, this, [this](const auto &device) {
        evdevDeviceRemoved(device.devNode());
    });
    m_udevMonitor->enableReceiving();

    m_deviceInitializationRetryTimer.start();
}

void StandaloneInputBackend::reset()
{
    m_virtualKeyboard.reset();
    m_virtualMouse.reset();

    for (const auto &device : m_devices) {
        if (device->properties().grab()) {
            // The compositor will take long enough to detect device removal that it will start generating key repeat events, reset the device to prevent that
            device->resetVirtualDeviceState();
        }
        removeDevice(device.get());
    }
    m_devices.clear();
    m_udevMonitor.reset();
    m_deviceInitializationRetryTimer.stop();
    LibinputInputBackend::reset();
}

void StandaloneInputBackend::evdevDeviceAdded(const QString &path)
{
    if (!path.startsWith("/dev/input/event")) {
        return;
    }

    if (!tryAddEvdevDevice(path)) {
        m_deviceInitializationQueue[path] = 0;
    }
}

bool StandaloneInputBackend::tryAddEvdevDevice(const QString &path)
{
    for (const auto &device : m_devices) {
        if (path == m_virtualKeyboard->path() || path == m_virtualMouse->path() || device->isDeviceOwnedByThisDevice(path)) {
            return true;
        }
    }

    bool retry{};
    auto device = StandaloneInputDevice::tryCreate(m_udev, path, retry);
    if (!device) {
        return !retry;
    }

    if (device->type() == InputDeviceType::Touchpad) {
        connect(&device->touchpadStateResetTimer(), &QTimer::timeout, this, [device = device.get()]() {
            device->resetVirtualDeviceState();
        });
    }

    if (auto *libevdev = device->libevdev().get()) {
        LibevdevComplementaryInputBackend::addDevice(device.get(), device->libevdev());
        connect(libevdev, &libevdev::Device::eventsAvailable, this, &StandaloneInputBackend::poll);
    } else {
        LibevdevComplementaryInputBackend::addDevice(device.get());
    }
    connect(device->libinput(), &libinput::PathContext::eventsAvailable, this, &StandaloneInputBackend::poll);

    InputBackend::addDevice(device.get());
    m_devices.push_back(std::move(device));
    return true;
}

void StandaloneInputBackend::evdevDeviceRemoved(const QString &path)
{
    auto it = std::ranges::find_if(m_devices, [&path](const auto &device) {
        return device->path() == path;
    });
    if (it != m_devices.end()) {
        removeDevice(it->get());
        m_devices.erase(it);
    }
}

void StandaloneInputBackend::deviceInitializationRetryTimerTick()
{
    for (auto it = m_deviceInitializationQueue.begin(); it != m_deviceInitializationQueue.end();) {
        const auto &path = it->first;
        auto &attempts = it->second;

        if (tryAddEvdevDevice(path) || ++attempts == MAX_INITIALIZATION_ATTEMPTS) {
            it = m_deviceInitializationQueue.erase(it);
            continue;
        }

        ++it;
    }

    for (auto it = m_devices.begin(); it != m_devices.end();) {
        const auto &device = it->get();

        if (!device->isLibinputEventInjectionDeviceInitialized() && device->tryInitializeLibinputEventInjectionDevice() == MAX_INITIALIZATION_ATTEMPTS) {
            it = m_devices.erase(it);
            continue;
        }

        ++it;
    }
}

bool StandaloneInputBackend::handleEvent(StandaloneInputDevice *sender, const libinput::Event &event)
{
    const auto type = event.type();
    switch (type) {
        case LIBINPUT_EVENT_GESTURE_HOLD_BEGIN:
        case LIBINPUT_EVENT_GESTURE_HOLD_END:
        case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
        case LIBINPUT_EVENT_GESTURE_PINCH_END:
        case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:
        case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
        case LIBINPUT_EVENT_GESTURE_SWIPE_END:
        case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE: {
            const auto gestureEvent = event.gestureEvent();
            const auto fingers = gestureEvent->fingerCount();

            auto cancelled = false;
            switch (type) {
                case LIBINPUT_EVENT_GESTURE_HOLD_END:
                case LIBINPUT_EVENT_GESTURE_PINCH_END:
                case LIBINPUT_EVENT_GESTURE_SWIPE_END:
                    cancelled = gestureEvent->cancelled();
                    break;
            }

            switch (type) {
                case LIBINPUT_EVENT_GESTURE_HOLD_BEGIN:
                    return touchpadHoldBegin(sender, fingers);
                case LIBINPUT_EVENT_GESTURE_HOLD_END:
                    return touchpadHoldEnd(sender, cancelled);
                case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
                    touchpadPinchBegin(sender, fingers);
                    // Resetting the touchpad state one frame before libinput recognizes a pinch gesture messes up the state machine, probably a libinput bug.
                    // Resetting it one frame before a pinch update event does not trigger the bug, and the gesture is still blocked, since clients and
                    // compositors only start executing actions after the first update event.
                    return false;
                case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE: {
                    return touchpadPinchUpdate(sender, gestureEvent->scale(), gestureEvent->angleDelta());
                }
                case LIBINPUT_EVENT_GESTURE_PINCH_END:
                    return touchpadPinchEnd(sender, cancelled);
                case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
                    return touchpadSwipeBegin(sender, fingers);
                case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE: {
                    return touchpadSwipeUpdate(sender, {gestureEvent->delta(), gestureEvent->deltaUnaccelerated()});
                }
                case LIBINPUT_EVENT_GESTURE_SWIPE_END:
                    return touchpadSwipeEnd(sender, cancelled);
            }
            break;
        }
        case LIBINPUT_EVENT_KEYBOARD_KEY: {
            const auto keyboardEvent = event.keyboardEvent();
            return keyboardKey(sender, keyboardEvent->key(), keyboardEvent->state());
        }
        case LIBINPUT_EVENT_POINTER_AXIS:
        case LIBINPUT_EVENT_POINTER_BUTTON:
        case LIBINPUT_EVENT_POINTER_MOTION: {
            const auto pointerEvent = event.pointerEvent();
            switch (type) {
                case LIBINPUT_EVENT_POINTER_AXIS: {
                    static const auto axis = [](const auto &pointerEvent, const auto axis) {
                        return pointerEvent->hasAxis(axis) ? pointerEvent->axisValue(axis) : 0;
                    };
                    const PointF delta(axis(pointerEvent, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL), axis(pointerEvent, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL));
                    return pointerAxis(sender, delta);
                }
                case LIBINPUT_EVENT_POINTER_BUTTON: {
                    return pointerButton(sender, pointerEvent->button(), pointerEvent->state());
                }
                case LIBINPUT_EVENT_POINTER_MOTION:
                    return pointerMotion(sender, {pointerEvent->delta(), pointerEvent->deltaUnaccelerated()});
            }
            break;
        }
        case LIBINPUT_EVENT_TOUCH_CANCEL:
            return touchscreenTouchCancel(sender);
        case LIBINPUT_EVENT_TOUCH_FRAME:
            return touchscreenTouchFrame(sender);
        case LIBINPUT_EVENT_TOUCH_DOWN:
        case LIBINPUT_EVENT_TOUCH_MOTION:
        case LIBINPUT_EVENT_TOUCH_UP: {
            const auto touchEvent = event.touchEvent();
            switch (type) {
                case LIBINPUT_EVENT_TOUCH_DOWN: {
                    const auto slot = touchEvent->slot();
                    PointF rawPosition;
                    if (sender->libevdev()) {
                        rawPosition = PointF(sender->libevdev()->slotValue(slot, ABS_MT_POSITION_X), sender->libevdev()->slotValue(slot, ABS_MT_POSITION_Y));
                    }
                    return touchscreenTouchDown(sender, slot, touchEvent->position(), rawPosition);
                }
                case LIBINPUT_EVENT_TOUCH_MOTION: {
                    const auto slot = touchEvent->slot();
                    PointF rawPosition;
                    if (sender->libevdev()) {
                        rawPosition = PointF(sender->libevdev()->slotValue(slot, ABS_MT_POSITION_X), sender->libevdev()->slotValue(slot, ABS_MT_POSITION_Y));
                    }
                    return touchscreenTouchMotion(sender, slot, touchEvent->position(), rawPosition);
                }
                case LIBINPUT_EVENT_TOUCH_UP:
                    return touchscreenTouchUp(sender, touchEvent->slot());
            }
            break;
        }
    }
    return false;
}

LibinputEventsProcessingResult StandaloneInputBackend::handleLibinputEvents(StandaloneInputDevice *device, libinput::PathContext *libinputContext)
{
    libinputContext->dispatch();

    // FIXME: One evdev frame can result in multiple libinput events, but one blocked libinput event will block the entire evdev frame.
    LibinputEventsProcessingResult result;
    while (const auto event = libinputContext->getEvent()) {
        result.block = handleEvent(device, event.value()) || result.block;
        result.eventCount++;
    }
    return result;
}

void StandaloneInputBackend::poll()
{
    std::vector<EvdevEvent> frame;
    for (const auto &device : m_devices) {
        if (!device->properties().grab()) {
            handleLibinputEvents(device.get(), device->libinput());
            continue;
        }

        int status{};
        while (true) {
            input_event evdevEvent{};
            auto flags = status == LIBEVDEV_READ_STATUS_SYNC ? LIBEVDEV_READ_FLAG_SYNC : LIBEVDEV_READ_FLAG_NORMAL;
            status = device->libevdev()->nextEvent(flags, evdevEvent);
            if (status != LIBEVDEV_READ_STATUS_SUCCESS && status != LIBEVDEV_READ_STATUS_SYNC) {
                // Handle events generated after a delay, e.g. pointer button after tapping
                handleLibinputEvents(device.get(), device->libinput());
                break;
            }

            frame.emplace_back(evdevEvent.type, evdevEvent.code, evdevEvent.value);
            LibevdevComplementaryInputBackend::handleEvdevEvent(device.get(), evdevEvent);

            if (evdevEvent.type != EV_SYN) {
                continue;
            }

            const auto blockFrame = InputBackend::handleEvent(EvdevFrameEvent(device.get(), frame));
            for (const auto &event : frame) {
                device->libinputEventInjectionDevice()->writeEvent(event.type(), event.code(), event.value());
            }
            const auto libinputResult = handleLibinputEvents(device.get(), device->libinput());

            if (device->type() == InputDeviceType::Touchpad) {
                // TouchpadTriggerHandler does currently not currently handle evdev events, so no need to check for blockFrame

                // Copy state of the real device to the output device if events suddenly stop being blocked while the device is not in a neutral state
                if (device->isTouchpadBlocked() && !libinputResult.block && libinputResult.eventCount) {
                    device->touchpadStateResetTimer().stop();
                    device->setTouchpadBlocked(false);
                    device->restoreVirtualDeviceState();
                }

                // Touchpad gestures are blocked by blocking the current and all next frames until all fingers are lifted, and changing the state of the output
                // device to neutral after 200ms. The delay is required to block tap gestures, but doesn't affect motion gesture blocking negatively.
                if (libinputResult.block && device->type() == InputDeviceType::Touchpad && !device->isTouchpadBlocked()) {
                    device->setTouchpadBlocked(true);
                    device->touchpadStateResetTimer().start(200);
                } else if (device->isTouchpadNeutral() && device->touchpadStateResetTimer().isActive()) {
                    device->touchpadStateResetTimer().stop();
                    device->resetVirtualDeviceState();
                }

                device->setTouchpadNeutral(false);
            }

            if (!libinputResult.block && !device->isTouchpadBlocked() && !blockFrame) {
                for (const auto &event : frame) {
                    device->outputDevice()->writeEvent(event.type(), event.code(), event.value());
                }
            }
            frame.clear();
        }

        if (device->type() == InputDeviceType::Touchpad && device->physicalState().validTouchPoints().empty()) {
            device->setTouchpadNeutral(true);
            device->setTouchpadBlocked(false);
        }
    }
}

VirtualKeyboard *StandaloneInputBackend::virtualKeyboard()
{
    return m_virtualKeyboard ? &m_virtualKeyboard.value() : nullptr;
}

VirtualMouse *StandaloneInputBackend::virtualMouse()
{
    return m_virtualMouse ? &m_virtualMouse.value() : nullptr;
}

}