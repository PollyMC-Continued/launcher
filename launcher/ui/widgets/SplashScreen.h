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

#include <QElapsedTimer>
#include <QPixmap>
#include <QString>
#include <QSvgRenderer>
#include <QTimer>
#include <QWidget>

/**
 * The startup splash: logo on a soft green glow, a progress bar that eases towards the current
 * startup step and a one-line status text below it.
 *
 * It is deliberately not "always on top": it must never sit over a window the launcher opens
 * during startup (settings, wizards, error boxes). The application closes it as soon as any
 * other top-level window appears.
 */
class SplashScreen : public QWidget {
    Q_OBJECT
   public:
    explicit SplashScreen(QWidget* parent = nullptr);

    /**
     * Shows a new status line and moves the bar to `progress` (0..1). An empty text keeps the
     * line blank (used before the user's language is loaded); a negative progress keeps the bar.
     * Repaints right away, because startup blocks the event loop between steps.
     */
    void setStatus(const QString& text, double progress = -1.0);

    /**
     * Shows the splash and makes sure something is really on screen before returning: waits (briefly)
     * until the window system has mapped the window, then paints it synchronously. Without this a window
     * shown right before a long blocking startup stays blank until the event loop finally runs.
     * The static part of the picture is pre-rendered in the constructor, so this first paint is just a blit.
     */
    void presentNow();

    /** Short self-running demo for the developer page. */
    static void showPreview();

   protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

   private:
    void tick();
    /// Renders everything that never changes (background, glow, logo, version, border) into m_base.
    /// Called from the constructor, so the expensive first-use work (SVG, fonts) is done before the
    /// window is mapped and the very first frame the user sees is already complete.
    void ensureBase();

    QSvgRenderer m_logo;
    QPixmap m_base;
    qreal m_baseRatio = 0;

    QTimer m_timer;
    QElapsedTimer m_clock;

    QString m_status;
    QString m_version;
    double m_target = 0.0;  // where the bar should end up
    double m_shown = 0.0;   // where it is drawn (eases towards m_target)
};
