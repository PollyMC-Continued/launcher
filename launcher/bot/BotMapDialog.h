#pragma once

#include <QColor>
#include <QDialog>
#include <QMap>
#include <QString>
#include <QVector>

class QListWidget;
class QTimer;

// One reported bot position in world coordinates.
struct BotMapPoint {
    double x = 0;
    double y = 0;
    double z = 0;
    double yaw = 0;
    double health = 0;
};

// Per-bot drawing state: assigned color plus the capped position history.
struct BotTrail {
    QColor color;
    QVector<BotMapPoint> points;
};

// The top-down map widget itself; implemented in BotMapDialog.cpp.
class BotMapWidget : public QWidget {
    Q_OBJECT
   public:
    // The dialog keeps ownership of the trails map and outlives the widget.
    explicit BotMapWidget(const QMap<QString, BotTrail>& trails, QWidget* parent = nullptr);

   protected:
    void paintEvent(QPaintEvent* event) override;

   private:
    const QMap<QString, BotTrail>& m_trails;
};

// Live top-down view of every connected bot. BotManagerDialog forwards
// BotProcess::botPosition() into addPosition(); the dialog owns no process.
class BotMapDialog : public QDialog {
    Q_OBJECT
   public:
    explicit BotMapDialog(QWidget* parent = nullptr);

   public slots:
    // Feed one position update (already throttled to 1/s by the bot server).
    void addPosition(const QString& username, double x, double y, double z, double yaw, double health);
    // Drop one bot's trail (that bot was disconnected).
    void clearBot(const QString& username);
    // Drop everything (bot server exited).
    void clearAll();

   private:
    void refreshLegend();

    QMap<QString, BotTrail> m_bots;
    int m_colorCursor = 0;

    BotMapWidget* m_map = nullptr;
    QListWidget* m_legend = nullptr;
    QTimer* m_repaintTimer = nullptr;
};
