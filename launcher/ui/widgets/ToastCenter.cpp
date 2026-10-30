// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ToastCenter.h"

#include <QApplication>
#include <QEvent>
#include <QStatusBar>
#include <QTimer>
#include <QWidget>

#include "ToastNotification.h"

namespace {
constexpr int kMargin = 12;
constexpr int kBusyRetryMs = 500;
constexpr int kNoTargetRetryMs = 1000;
constexpr int kNoTargetGiveUpTicks = 120;  // ~2 minutes without any window to show it in: drop the queue
}  // namespace

ToastCenter* ToastCenter::instance()
{
    static ToastCenter* center = new ToastCenter(qApp);
    return center;
}

ToastCenter::ToastCenter(QObject* parent) : QObject(parent)
{
    m_retry = new QTimer(this);
    m_retry->setSingleShot(true);
    connect(m_retry, &QTimer::timeout, this, &ToastCenter::pump);

    // The user moved to another window: take the toast along.
    connect(qApp, &QApplication::focusChanged, this, [this]() { QTimer::singleShot(0, this, &ToastCenter::follow); });
}

void ToastCenter::setHomeWindow(QWidget* home)
{
    m_home = home;
}

bool ToastCenter::ensureToast()
{
    if (m_toast)
        return true;
    QWidget* parent = m_host ? m_host.data() : m_home.data();
    if (!parent)
        return false;
    m_toast = new ToastNotification(parent);
    connect(m_toast, &ToastNotification::dismissed, this, [this]() {
        auto callback = m_current.onDismissed;
        m_current.onDismissed = nullptr;
        if (callback)
            callback();
    });
    return true;
}

void ToastCenter::post(Request request)
{
    if (!request.key.isEmpty()) {
        for (int i = m_queue.size() - 1; i >= 0; --i)
            if (m_queue.at(i).key == request.key)
                m_queue.removeAt(i);
    }
    m_queue.append(std::move(request));
    pump();
}

bool ToastCenter::isBusy() const
{
    return !m_queue.isEmpty() || (m_toast && m_toast->isBusy());
}

bool ToastCenter::usable(const QWidget* w) const
{
    if (!w || w == m_toast || !w->isVisible() || w->isMinimized())
        return false;
    const auto type = w->windowType();
    return type == Qt::Window || type == Qt::Dialog || type == Qt::Sheet;
}

QWidget* ToastCenter::pickTarget() const
{
    QWidget* active = QApplication::activeWindow();
    if (usable(active))
        return active;
    // The launcher is not focused (or a menu has focus): fall back to the main window, as before.
    if (usable(m_home))
        return m_home;
    return nullptr;
}

QPoint ToastCenter::positionIn(const QWidget* host) const
{
    int statusBarHeight = 0;
    if (auto* bar = host->findChild<QStatusBar*>(QString(), Qt::FindDirectChildrenOnly))
        if (bar->isVisible())
            statusBarHeight = bar->height();
    // height() (not sizeHint()) so word-wrapped / translated text is accounted for
    const int y = host->height() - statusBarHeight - m_toast->height() - kMargin;
    return QPoint(kMargin, qMax(kMargin, y));
}

void ToastCenter::attachTo(QWidget* host)
{
    if (m_host == host)
        return;
    if (m_host)
        m_host->removeEventFilter(this);
    m_host = host;
    host->installEventFilter(this);
    m_toast->moveToWindow(host);
}

void ToastCenter::pump()
{
    if (m_queue.isEmpty())
        return;
    if (!ensureToast()) {
        m_retry->start(kNoTargetRetryMs);
        return;
    }
    if (m_toast->isBusy()) {
        m_retry->start(kBusyRetryMs);
        return;
    }

    QWidget* target = pickTarget();
    if (!target) {
        if (++m_noTargetTicks > kNoTargetGiveUpTicks) {
            m_noTargetTicks = 0;
            m_queue.clear();
            return;
        }
        m_retry->start(kNoTargetRetryMs);
        return;
    }
    m_noTargetTicks = 0;

    m_current = m_queue.takeFirst();
    attachTo(target);
    m_toast->setIconGlyph(m_current.icon);
    m_toast->prepareToast(m_current.title, m_current.body, m_current.actionText, m_current.action);
    m_toast->showToast(positionIn(target));
    if (m_current.onShown)
        m_current.onShown();

    if (!m_queue.isEmpty())
        m_retry->start(kBusyRetryMs);
}

void ToastCenter::follow()
{
    if (!m_toast || !m_toast->isToastVisible())
        return;
    QWidget* active = QApplication::activeWindow();
    if (!usable(active) || active == m_host)
        return;
    attachTo(active);
    m_toast->updatePosition(positionIn(active));
}

void ToastCenter::rehome()
{
    if (!m_toast)
        return;
    QWidget* target = pickTarget();
    if (!target || target == m_host)
        target = m_home;  // may be hidden (tray): the card then simply times out unseen
    if (!target || target == m_host)
        return;
    attachTo(target);
    m_toast->updatePosition(positionIn(target));
}

void ToastCenter::reposition()
{
    if (m_toast && m_host && m_toast->isToastVisible())
        m_toast->updatePosition(positionIn(m_host));
}

bool ToastCenter::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_host) {
        switch (event->type()) {
            case QEvent::Resize:
                reposition();
                break;
            case QEvent::Close:
                rehome();
                break;
            case QEvent::Hide:
                // A real hide (dialog closed), not the window being minimised: move out before the window goes away.
                if (!event->spontaneous())
                    rehome();
                break;
            default:
                break;
        }
    }
    return QObject::eventFilter(watched, event);
}
