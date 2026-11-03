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

#include "SplashScreen.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QRadialGradient>
#include <QScreen>
#include <QWindow>
#include <QtMath>

#include <algorithm>
#include <cmath>

#include "BuildConfig.h"

namespace {
constexpr int Width = 480;
constexpr int Height = 300;
constexpr qreal LogoWidth = 300;  // the wordmark is 3:1
constexpr qreal LogoTop = 56;
constexpr qreal BarLeft = 80;
constexpr qreal BarTop = 214;
constexpr qreal BarHeight = 4;
const QColor Accent(0x8A, 0xCA, 0x00);  // the green of the logo
}  // namespace

SplashScreen::SplashScreen(QWidget* parent) : QWidget(parent, Qt::SplashScreen | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedSize(Width, Height);
    // never a black rectangle: even before the first paint the window has the splash's own background
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0x17, 0x1A, 0x17));
    setPalette(pal);
    setAutoFillBackground(true);
    // we paint every pixel ourselves: tell Qt not to clear the window to the system colour first
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);

    m_logo.load(QStringLiteral(":/branding/splash-logo.svg"));
    m_version = BuildConfig.printableVersionString();

    if (auto* screen = QGuiApplication::primaryScreen())
        move(screen->availableGeometry().center() - rect().center());

    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &SplashScreen::tick);
    m_clock.start();

    // do the slow first-use work (SVG rasterising, font database, gradients) now, before the window exists
    ensureBase();
}

void SplashScreen::setStatus(const QString& text, double progress)
{
    m_status = text;
    if (progress >= 0.0) {
        m_target = std::clamp(progress, 0.0, 1.0);
        // startup blocks the event loop, so the easing below can't run between steps:
        // jump most of the way now and let the animation finish the rest when it can
        m_shown = std::max(m_shown, m_target - 0.12);
    }
    if (isVisible())
        repaint();
}

void SplashScreen::presentNow()
{
    show();
    // wait (at most ~0.5 s) for the window system to map the window, then paint synchronously
    QElapsedTimer waited;
    waited.start();
    while (waited.elapsed() < 500) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
        if (windowHandle() && windowHandle()->isExposed())
            break;
    }
    repaint();
}

void SplashScreen::showEvent(QShowEvent* event)
{
    m_clock.restart();
    m_timer.start();
    QWidget::showEvent(event);
}

void SplashScreen::hideEvent(QHideEvent* event)
{
    m_timer.stop();
    QWidget::hideEvent(event);
}

void SplashScreen::tick()
{
    m_shown += (m_target - m_shown) * 0.10;
    if (qAbs(m_target - m_shown) < 0.0005)
        m_shown = m_target;
    update();
}

void SplashScreen::ensureBase()
{
    const qreal ratio = devicePixelRatioF();
    if (!m_base.isNull() && qFuzzyCompare(m_baseRatio, ratio))
        return;
    m_baseRatio = ratio;
    m_base = QPixmap(QSize(qCeil(Width * ratio), qCeil(Height * ratio)));
    m_base.setDevicePixelRatio(ratio);
    m_base.fill(QColor(0x17, 0x1A, 0x17));

    QPainter p(&m_base);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
    const QRectF r(0, 0, Width, Height);

    // background: deep charcoal with a faint green cast, then a glow behind the logo
    QLinearGradient bg(0, 0, 0, Height);
    bg.setColorAt(0.0, QColor(0x20, 0x26, 0x20));
    bg.setColorAt(1.0, QColor(0x11, 0x13, 0x11));
    p.fillRect(r, bg);

    QRadialGradient glow(Width / 2.0, LogoTop + LogoWidth / 6, 210);
    glow.setColorAt(0.0, QColor(Accent.red(), Accent.green(), Accent.blue(), 56));
    glow.setColorAt(1.0, QColor(Accent.red(), Accent.green(), Accent.blue(), 0));
    p.fillRect(r, glow);

    // accent line along the top edge
    QLinearGradient top(0, 0, Width, 0);
    top.setColorAt(0.0, QColor(Accent.red(), Accent.green(), Accent.blue(), 0));
    top.setColorAt(0.5, QColor(Accent.red(), Accent.green(), Accent.blue(), 200));
    top.setColorAt(1.0, QColor(Accent.red(), Accent.green(), Accent.blue(), 0));
    p.fillRect(QRectF(0, 0, Width, 2), top);

    // logo
    if (m_logo.isValid()) {
        m_logo.render(&p, QRectF((Width - LogoWidth) / 2, LogoTop, LogoWidth, LogoWidth / 3));
    } else {
        QFont f = font();
        f.setBold(true);
        f.setPointSizeF(qMax<qreal>(f.pointSizeF(), 9) * 2.4);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(QRectF(0, LogoTop, Width, LogoWidth / 3), Qt::AlignCenter, BuildConfig.LAUNCHER_DISPLAYNAME);
    }

    // version
    {
        QFont f = font();
        if (f.pointSizeF() > 0)
            f.setPointSizeF(f.pointSizeF() * 0.85);
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 85));
        p.drawText(QRectF(0, Height - 34, Width, 20), Qt::AlignHCenter | Qt::AlignVCenter, m_version);
    }

    // hairline border
    p.setBrush(Qt::NoBrush);
    p.setPen(QColor(255, 255, 255, 26));
    p.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
}

void SplashScreen::paintEvent(QPaintEvent*)
{
    ensureBase();  // normally a no-op; re-renders only if the window moved to a screen with another pixel ratio

    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
    const qreal seconds = m_clock.elapsed() / 1000.0;

    // everything static comes from one pre-rendered picture
    p.drawPixmap(QPointF(0, 0), m_base);

    // progress bar
    const qreal barWidth = Width - 2 * BarLeft;
    const QRectF track(BarLeft, BarTop, barWidth, BarHeight);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 28));
    p.drawRoundedRect(track, BarHeight / 2, BarHeight / 2);

    if (m_target <= 0.0) {
        // nothing is known yet: a soft segment glides back and forth so it never looks frozen
        const qreal segment = 90;
        const qreal phase = 0.5 - 0.5 * qCos(seconds * 2.4);  // 0..1, eased at both ends
        QRectF bar(BarLeft + phase * (barWidth - segment), BarTop, segment, BarHeight);
        QLinearGradient g(bar.left(), 0, bar.right(), 0);
        g.setColorAt(0.0, QColor(Accent.red(), Accent.green(), Accent.blue(), 0));
        g.setColorAt(0.5, Accent);
        g.setColorAt(1.0, QColor(Accent.red(), Accent.green(), Accent.blue(), 0));
        p.setBrush(g);
        p.drawRoundedRect(bar, BarHeight / 2, BarHeight / 2);
    } else if (m_shown > 0.0) {
        const qreal fillWidth = qMax<qreal>(BarHeight, barWidth * m_shown);
        const QRectF fill(BarLeft, BarTop, fillWidth, BarHeight);

        // soft glow under the bar
        p.setBrush(QColor(Accent.red(), Accent.green(), Accent.blue(), 48));
        p.drawRoundedRect(fill.adjusted(-1, -3, 1, 3), BarHeight, BarHeight);

        QLinearGradient g(fill.left(), 0, fill.right(), 0);
        g.setColorAt(0.0, QColor(0x5C, 0x8F, 0x00));
        g.setColorAt(1.0, QColor(0xB8, 0xF2, 0x3C));
        p.setBrush(g);
        p.drawRoundedRect(fill, BarHeight / 2, BarHeight / 2);

        // a light band sweeping over the filled part
        QPainterPath clip;
        clip.addRoundedRect(fill, BarHeight / 2, BarHeight / 2);
        p.save();
        p.setClipPath(clip);
        const qreal band = 70;
        const qreal x = fill.left() - band + std::fmod(seconds / 1.5, 1.0) * (fillWidth + band);
        QLinearGradient shine(x, 0, x + band, 0);
        shine.setColorAt(0.0, QColor(255, 255, 255, 0));
        shine.setColorAt(0.5, QColor(255, 255, 255, 90));
        shine.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.setBrush(shine);
        p.drawRect(QRectF(x, BarTop, band, BarHeight));
        p.restore();
    }

    // status line (already translated by the caller)
    if (!m_status.isEmpty()) {
        QFont f = font();
        if (f.pointSizeF() > 0)
            f.setPointSizeF(f.pointSizeF() * 1.05);
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 190));
        const QRectF line(BarLeft, BarTop + 16, barWidth, 24);
        p.drawText(line, Qt::AlignHCenter | Qt::AlignVCenter, QFontMetrics(f).elidedText(m_status, Qt::ElideRight, qFloor(line.width())));
    }
}

void SplashScreen::showPreview()
{
    auto* splash = new SplashScreen;
    splash->setAttribute(Qt::WA_DeleteOnClose);
    splash->show();
    splash->raise();

    struct Step {
        int delay;
        double progress;
        QString text;
    };
    const QList<Step> steps = {
        { 250, 0.35, tr("Loading icons…") },
        { 700, 0.60, tr("Loading instances…") },
        { 1150, 0.85, tr("Loading accounts…") },
        { 1600, 1.00, tr("Almost ready…") },
    };
    QPointer<SplashScreen> guard(splash);
    for (const auto& step : steps) {
        QTimer::singleShot(step.delay, splash, [guard, step] {
            if (guard)
                guard->setStatus(step.text, step.progress);
        });
    }
    QTimer::singleShot(2300, splash, &QWidget::close);
}
