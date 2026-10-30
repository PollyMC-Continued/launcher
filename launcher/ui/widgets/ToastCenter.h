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

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

class QTimer;
class QWidget;
class ToastNotification;

/*!
 * Shows toast notifications in whichever launcher window the user is working in (main window, Settings,
 * Report a Bug, an instance window, ...). One toast is on screen at a time; the rest wait in a queue.
 * If the user switches window while a toast is visible, it follows them.
 */
class ToastCenter : public QObject {
    Q_OBJECT

   public:
    struct Request {
        QString key;  // optional: a newer request with the same key replaces one still waiting in the queue
        QString title;
        QString body;
        QString actionText;
        QString icon = QStringLiteral("★");
        std::function<void()> action;       // action button clicked (runs after the card closed)
        std::function<void()> onShown;      // the card has actually appeared
        std::function<void()> onDismissed;  // the user closed it or clicked the action (not on auto-hide)
    };

    static ToastCenter* instance();

    /// The window toasts fall back to when no other launcher window is active (the main window).
    void setHomeWindow(QWidget* home);

    void post(Request request);

    /// A toast is on screen or waiting.
    bool isBusy() const;

    /// Re-place the visible toast (e.g. after the status bar was shown or hidden).
    void reposition();

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

   private:
    explicit ToastCenter(QObject* parent = nullptr);

    void pump();
    void follow();
    void attachTo(QWidget* host);
    void rehome();
    bool usable(const QWidget* w) const;
    QWidget* pickTarget() const;
    QPoint positionIn(const QWidget* host) const;
    bool ensureToast();

    QPointer<QWidget> m_home;
    QPointer<QWidget> m_host;
    QPointer<ToastNotification> m_toast;
    QList<Request> m_queue;
    Request m_current;
    QTimer* m_retry = nullptr;
    int m_noTargetTicks = 0;
};
