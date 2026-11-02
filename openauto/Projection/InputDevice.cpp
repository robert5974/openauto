/*
*  This file is part of openauto project.
*  Copyright (C) 2018 f1x.studio (Michal Szwaj)
*
*  openauto is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 3 of the License, or
*  (at your option) any later version.

*  openauto is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with openauto. If not, see <http://www.gnu.org/licenses/>.
*/

#include "OpenautoLog.hpp"
#include "openauto/Projection/IInputDeviceEventHandler.hpp"
#include "openauto/Projection/InputDevice.hpp"
#include <QApplication>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QWidget>
#include <algorithm>

namespace openauto
{
namespace projection
{

InputDevice::InputDevice(QObject& parent, configuration::IConfiguration::Pointer configuration, const QRect& touchscreenGeometry, const QRect& displayGeometry)
    : parent_(parent)
    , configuration_(std::move(configuration))
    , touchscreenGeometry_(touchscreenGeometry)
    , displayGeometry_(displayGeometry)
    , eventHandler_(nullptr)
{
    this->moveToThread(parent.thread());
}

void InputDevice::start(IInputDeviceEventHandler& eventHandler)
{
    std::lock_guard<decltype(mutex_)> lock(mutex_);

    OPENAUTO_LOG(info) << "[InputDevice] start.";
    eventHandler_ = &eventHandler;
    parent_.installEventFilter(this);
    if(qApp != nullptr)
    {
        qApp->installEventFilter(this);
    }
}

void InputDevice::stop()
{
    std::lock_guard<decltype(mutex_)> lock(mutex_);

    OPENAUTO_LOG(info) << "[InputDevice] stop.";
    parent_.removeEventFilter(this);
    if(qApp != nullptr)
    {
        qApp->removeEventFilter(this);
    }
    eventHandler_ = nullptr;
}

bool InputDevice::eventFilter(QObject* obj, QEvent* event)
{
    std::lock_guard<decltype(mutex_)> lock(mutex_);

    if(eventHandler_ != nullptr)
    {
        if(event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
        {
            QKeyEvent* key = static_cast<QKeyEvent*>(event);
            if(!key->isAutoRepeat())
            {
                return this->handleKeyEvent(event, key);
            }
        }
        else if(event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate ||
                event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel)
        {
            QWidget* activeArea = qobject_cast<QWidget*>(&parent_);
            if(activeArea != nullptr && activeArea->isVisible())
            {
                QWidget* target = qobject_cast<QWidget*>(obj);
                bool isFloatingHome = false;
                for(QWidget* p = target; p != nullptr; p = p->parentWidget())
                {
                    if(p->objectName() == "FloatingHomeButton" || p->property("isFloatingHome").toBool())
                    {
                        isFloatingHome = true;
                        break;
                    }
                }
                if(isFloatingHome)
                {
                    return QObject::eventFilter(obj, event);
                }

                QTouchEvent* te = static_cast<QTouchEvent*>(event);
                if(!te->points().isEmpty())
                {
                    QPoint localPt = activeArea->mapFromGlobal(te->points().first().globalPosition().toPoint());
                    if(activeArea->rect().contains(localPt))
                    {
                        touchActive_ = (event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate);
                        return this->handleQTouchEvent(te);
                    }
                }
            }
        }
        else if(event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease ||
                event->type() == QEvent::MouseMove)
        {
            QWidget* activeArea = qobject_cast<QWidget*>(&parent_);
            if(activeArea != nullptr && activeArea->isVisible())
            {
                QMouseEvent* me = static_cast<QMouseEvent*>(event);
                if(me->source() == Qt::MouseEventSynthesizedByQt && touchActive_)
                {
                    if(event->type() == QEvent::MouseButtonRelease)
                    {
                        touchActive_ = false;
                    }
                    return true;
                }

                QWidget* target = qobject_cast<QWidget*>(obj);
                bool isFloatingHome = false;
                for(QWidget* p = target; p != nullptr; p = p->parentWidget())
                {
                    if(p->objectName() == "FloatingHomeButton" || p->property("isFloatingHome").toBool())
                    {
                        isFloatingHome = true;
                        break;
                    }
                }
                if(isFloatingHome)
                {
                    return QObject::eventFilter(obj, event);
                }

                QPoint localPt = activeArea->mapFromGlobal(me->globalPosition().toPoint());
                if(activeArea->rect().contains(localPt))
                {
                    return this->handleMouseEvent(me);
                }
            }
        }
    }

    return QObject::eventFilter(obj, event);
}

bool InputDevice::handleKeyEvent(QEvent* event, QKeyEvent* key)
{
    auto eventType = event->type() == QEvent::KeyPress ? ButtonEventType::PRESS : ButtonEventType::RELEASE;
    aasdk::proto::enums::ButtonCode::Enum buttonCode;
    WheelDirection wheelDirection = WheelDirection::NONE;

    switch(key->key())
    {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        buttonCode = aasdk::proto::enums::ButtonCode::ENTER;
        break;

    case Qt::Key_Left:
        buttonCode = aasdk::proto::enums::ButtonCode::LEFT;
        break;

    case Qt::Key_Right:
        buttonCode = aasdk::proto::enums::ButtonCode::RIGHT;
        break;

    case Qt::Key_Up:
        buttonCode = aasdk::proto::enums::ButtonCode::UP;
        break;

    case Qt::Key_Down:
        buttonCode = aasdk::proto::enums::ButtonCode::DOWN;
        break;

    case Qt::Key_Escape:
        buttonCode = aasdk::proto::enums::ButtonCode::BACK;
        break;

    case Qt::Key_H:
        buttonCode = aasdk::proto::enums::ButtonCode::HOME;
        break;

    case Qt::Key_P:
        buttonCode = aasdk::proto::enums::ButtonCode::PHONE;
        break;

    case Qt::Key_O:
        buttonCode = aasdk::proto::enums::ButtonCode::CALL_END;
        break;

    case Qt::Key_X:
        buttonCode = aasdk::proto::enums::ButtonCode::PLAY;
        break;

    case Qt::Key_C:
        buttonCode = aasdk::proto::enums::ButtonCode::PAUSE;
        break;

    case Qt::Key_MediaPrevious:
    case Qt::Key_V:
        buttonCode = aasdk::proto::enums::ButtonCode::PREV;
        break;

    case Qt::Key_MediaPlay:
    case Qt::Key_B:
        buttonCode = aasdk::proto::enums::ButtonCode::TOGGLE_PLAY;
        break;

    case Qt::Key_MediaNext:
    case Qt::Key_N:
        buttonCode = aasdk::proto::enums::ButtonCode::NEXT;
        break;

    case Qt::Key_M:
        buttonCode = aasdk::proto::enums::ButtonCode::MICROPHONE_1;
        break;

    case Qt::Key_1:
        wheelDirection = WheelDirection::LEFT;
        eventType = ButtonEventType::NONE;
        buttonCode = aasdk::proto::enums::ButtonCode::SCROLL_WHEEL;
        break;

    case Qt::Key_2:
        wheelDirection = WheelDirection::RIGHT;
        eventType = ButtonEventType::NONE;
        buttonCode = aasdk::proto::enums::ButtonCode::SCROLL_WHEEL;
        break;

    default:
        return true;
    }

    const auto& buttonCodes = this->getSupportedButtonCodes();
    if(std::find(buttonCodes.begin(), buttonCodes.end(), buttonCode) != buttonCodes.end())
    {
        if(buttonCode != aasdk::proto::enums::ButtonCode::SCROLL_WHEEL || event->type() == QEvent::KeyRelease)
        {
            eventHandler_->onButtonEvent({eventType, wheelDirection, buttonCode});
        }
    }

    return true;
}

bool InputDevice::handleQTouchEvent(QTouchEvent* event)
{
    if(!configuration_->getTouchscreenEnabled() || event->points().isEmpty())
    {
        return true;
    }

    aasdk::proto::enums::TouchAction::Enum type;
    switch(event->type())
    {
    case QEvent::TouchBegin:
        type = aasdk::proto::enums::TouchAction::PRESS;
        break;
    case QEvent::TouchUpdate:
        type = aasdk::proto::enums::TouchAction::DRAG;
        break;
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        type = aasdk::proto::enums::TouchAction::RELEASE;
        break;
    default:
        return true;
    }

    QWidget* activeArea = qobject_cast<QWidget*>(&parent_);
    const auto& pt = event->points().first();
    QPointF localPos = activeArea ? activeArea->mapFromGlobal(pt.globalPosition()) : pt.position();

    float w = activeArea ? activeArea->width() : touchscreenGeometry_.width();
    float h = activeArea ? activeArea->height() : touchscreenGeometry_.height();
    if(w <= 0.0f) w = 1.0f;
    if(h <= 0.0f) h = 1.0f;

    float lx = std::clamp(static_cast<float>(localPos.x()), 0.0f, w);
    float ly = std::clamp(static_cast<float>(localPos.y()), 0.0f, h);

    const uint32_t x = std::clamp(static_cast<uint32_t>((lx / w) * displayGeometry_.width()), 0u, static_cast<uint32_t>(displayGeometry_.width() - 1));
    const uint32_t y = std::clamp(static_cast<uint32_t>((ly / h) * displayGeometry_.height()), 0u, static_cast<uint32_t>(displayGeometry_.height() - 1));

    eventHandler_->onTouchEvent({type, x, y, 0});
    return true;
}

bool InputDevice::handleMouseEvent(QMouseEvent* mouse)
{
    if(!configuration_->getTouchscreenEnabled())
    {
        return true;
    }

    aasdk::proto::enums::TouchAction::Enum type;
    switch(mouse->type())
    {
    case QEvent::MouseButtonPress:
        type = aasdk::proto::enums::TouchAction::PRESS;
        break;
    case QEvent::MouseButtonRelease:
        type = aasdk::proto::enums::TouchAction::RELEASE;
        break;
    case QEvent::MouseMove:
        type = aasdk::proto::enums::TouchAction::DRAG;
        break;
    default:
        return true;
    }

    if(mouse->type() == QEvent::MouseButtonRelease || mouse->buttons().testFlag(Qt::LeftButton))
    {
        QWidget* activeArea = qobject_cast<QWidget*>(&parent_);
        QPointF localPos = activeArea ? activeArea->mapFromGlobal(mouse->globalPosition()) : mouse->position();

        float w = activeArea ? activeArea->width() : touchscreenGeometry_.width();
        float h = activeArea ? activeArea->height() : touchscreenGeometry_.height();
        if(w <= 0.0f) w = 1.0f;
        if(h <= 0.0f) h = 1.0f;

        float lx = std::clamp(static_cast<float>(localPos.x()), 0.0f, w);
        float ly = std::clamp(static_cast<float>(localPos.y()), 0.0f, h);

        const uint32_t x = std::clamp(static_cast<uint32_t>((lx / w) * displayGeometry_.width()), 0u, static_cast<uint32_t>(displayGeometry_.width() - 1));
        const uint32_t y = std::clamp(static_cast<uint32_t>((ly / h) * displayGeometry_.height()), 0u, static_cast<uint32_t>(displayGeometry_.height() - 1));

        eventHandler_->onTouchEvent({type, x, y, 0});
    }

    return true;
}

bool InputDevice::handleTouchEvent(QEvent* event)
{
    if(event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate ||
       event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel)
    {
        return this->handleQTouchEvent(static_cast<QTouchEvent*>(event));
    }
    else if(event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease ||
            event->type() == QEvent::MouseMove)
    {
        return this->handleMouseEvent(static_cast<QMouseEvent*>(event));
    }
    return true;
}

bool InputDevice::hasTouchscreen() const
{
    return configuration_->getTouchscreenEnabled();
}

QRect InputDevice::getTouchscreenGeometry() const
{
    return touchscreenGeometry_;
}

void InputDevice::setTouchscreenGeometry(QRect& touchscreenGeometry)
{
    touchscreenGeometry_ = touchscreenGeometry;
}

IInputDevice::ButtonCodes InputDevice::getSupportedButtonCodes() const
{
    return configuration_->getButtonCodes();
}

}
}
