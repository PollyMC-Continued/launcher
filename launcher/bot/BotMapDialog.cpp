#include "BotMapDialog.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QPainter>
#include <QPen>
#include <QTimer>

#include <cmath>

// Same palette as BotManagerDialog's table rows, kept in sync by hand.
static const char* BOT_MAP_COLORS[] = {
    "#6ee7b7", "#60a5fa", "#fbbf24", "#f472b6",
    "#a78bfa", "#34d399", "#f87171", "#38bdf8"
};
static const int BOT_MAP_COLOR_COUNT = sizeof(BOT_MAP_COLORS) / sizeof(BOT_MAP_COLORS[0]);

// remembered positions per bot: long enough for a fading trail, cheap to scan
static const int TRAIL_MAX = 100;
// world blocks between grid lines (one chunk)
static const double GRID_STEP = 64.0;
// never zoom in tighter than a 64-block window, whatever the trail looks like
static const double MIN_SPAN = 64.0;

BotMapWidget::BotMapWidget(const QMap<QString, BotTrail>& trails, QWidget* parent)
    : QWidget(parent), m_trails(trails)
{
    setMinimumSize(320, 240);
}

void BotMapWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    // dark map background; everything on top of this is painted by hand
    p.fillRect(rect(), QColor("#111418"));

    // auto-fit: bounding box over every trail point of every bot
    double minX = 0, maxX = 0, minZ = 0, maxZ = 0;
    bool any = false;
    for (auto it = m_trails.cbegin(); it != m_trails.cend(); ++it) {
        for (const BotMapPoint& pt : it->points) {
            if (!any) {
                minX = maxX = pt.x;
                minZ = maxZ = pt.z;
                any = true;
            } else {
                minX = qMin(minX, pt.x);
                maxX = qMax(maxX, pt.x);
                minZ = qMin(minZ, pt.z);
                maxZ = qMax(maxZ, pt.z);
            }
        }
    }
    if (!any) {
        p.setPen(QColor("#8a93a2"));
        p.drawText(rect(), Qt::AlignCenter, tr("Waiting for bot positions..."));
        return;
    }

    // margin around the box plus a minimum span guard so a stationary bot
    // does not zoom the view into a single pixel
    const double margin = 16.0;
    const double spanX = qMax(maxX - minX + 2 * margin, MIN_SPAN);
    const double spanZ = qMax(maxZ - minZ + 2 * margin, MIN_SPAN);
    const double scale = qMin(width() / spanX, height() / spanZ);
    const double centerX = (minX + maxX) / 2;
    const double centerZ = (minZ + maxZ) / 2;
    // world -> widget helpers; the view is centered on the bounding box
    const auto wx = [this, centerX, scale](double worldX) {
        return width() / 2.0 + (worldX - centerX) * scale;
    };
    const auto wz = [this, centerZ, scale](double worldZ) {
        return height() / 2.0 + (worldZ - centerZ) * scale;
    };

    // fine grid every 64 blocks with axis labels at the widget edges
    const QPen gridPen(QColor("#232a33"), 1);
    const QPen labelPen(QColor("#5a6572"), 1);
    const double halfW = width() / (2.0 * scale);
    const double halfH = height() / (2.0 * scale);
    for (qint64 gx = qint64(std::floor((centerX - halfW) / GRID_STEP));
         gx * GRID_STEP <= centerX + halfW; gx++) {
        const double worldX = gx * GRID_STEP;
        const int px = int(wx(worldX));
        p.setPen(gridPen);
        p.drawLine(px, 0, px, height());
        p.setPen(labelPen);
        p.drawText(QRect(px + 3, height() - 16, 96, 14), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("x %1").arg(worldX));
    }
    for (qint64 gz = qint64(std::floor((centerZ - halfH) / GRID_STEP));
         gz * GRID_STEP <= centerZ + halfH; gz++) {
        const double worldZ = gz * GRID_STEP;
        const int pz = int(wz(worldZ));
        p.setPen(gridPen);
        p.drawLine(0, pz, width(), pz);
        p.setPen(labelPen);
        p.drawText(QRect(3, pz + 3, 96, 14), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("z %1").arg(worldZ));
    }

    // trails with fading tails, current dot and name label per bot
    for (auto it = m_trails.cbegin(); it != m_trails.cend(); ++it) {
        const QVector<BotMapPoint>& pts = it->points;
        if (pts.isEmpty())
            continue;
        const QColor c = it->color;
        for (int i = 1; i < pts.size(); i++) {
            // older segments fade out; the newest is nearly opaque
            const int alpha = 40 + (215 * i) / pts.size();
            QPen pen(QColor(c.red(), c.green(), c.blue(), alpha));
            pen.setWidthF(1.5);
            p.setPen(pen);
            p.drawLine(QPointF(wx(pts[i - 1].x), wz(pts[i - 1].z)), QPointF(wx(pts[i].x), wz(pts[i].z)));
        }
        const BotMapPoint& last = pts.last();
        p.setPen(c);
        p.setBrush(c);
        p.drawEllipse(QPointF(wx(last.x), wz(last.z)), 4.0, 4.0);
        p.setPen(QColor("#dde1e7"));
        p.drawText(QPointF(wx(last.x) + 7.0, wz(last.z) - 7.0), it.key());
    }
}

BotMapDialog::BotMapDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Bot Map"));
    resize(820, 560);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_map = new BotMapWidget(m_bots, this);
    layout->addWidget(m_map, 1);

    m_legend = new QListWidget(this);
    m_legend->setFixedWidth(280);
    m_legend->setStyleSheet(
        "QListWidget { background: #1a1a1a; color: #dde1e7; border: 1px solid #333; font-size: 12px; }"
        "QListWidget::item { padding: 6px 8px; }"
    );
    layout->addWidget(m_legend);

    // 1 Hz repaint keeps the fit and trails fresh even without fresh events
    m_repaintTimer = new QTimer(this);
    connect(m_repaintTimer, &QTimer::timeout, this, [this] { m_map->update(); });
    m_repaintTimer->start(1000);
}

void BotMapDialog::addPosition(const QString& username, double x, double y, double z, double yaw, double health)
{
    if (username.isEmpty())
        return;
    BotTrail& trail = m_bots[username];
    if (trail.points.isEmpty()) {
        // first sighting: hand out the next palette color
        trail.color = QColor(BOT_MAP_COLORS[m_colorCursor % BOT_MAP_COLOR_COUNT]);
        m_colorCursor++;
    }
    BotMapPoint pt;
    pt.x = x;
    pt.y = y;
    pt.z = z;
    pt.yaw = yaw;
    pt.health = health;
    trail.points.append(pt);
    while (trail.points.size() > TRAIL_MAX)
        trail.points.removeFirst();
    refreshLegend();
    m_map->update();
}

void BotMapDialog::clearBot(const QString& username)
{
    if (m_bots.remove(username) > 0) {
        refreshLegend();
        m_map->update();
    }
}

void BotMapDialog::clearAll()
{
    if (m_bots.isEmpty())
        return;
    m_bots.clear();
    refreshLegend();
    m_map->update();
}

void BotMapDialog::refreshLegend()
{
    m_legend->clear();
    for (auto it = m_bots.cbegin(); it != m_bots.cend(); ++it) {
        if (it->points.isEmpty())
            continue;
        const BotMapPoint& last = it->points.last();
        auto* item = new QListWidgetItem(
            QStringLiteral("%1\nx %2  y %3  z %4\nhealth %5")
                .arg(it.key(), QString::number(last.x, 'f', 1), QString::number(last.y, 'f', 1),
                     QString::number(last.z, 'f', 1), QString::number(last.health, 'f', 1)),
            m_legend);
        item->setForeground(it->color);
    }
}
