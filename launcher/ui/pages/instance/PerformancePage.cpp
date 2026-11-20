// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (C) 2026 PollyMC-Continued Contributors
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2026 PollyMC-Continued Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "PerformancePage.h"

#include <QDateTime>
#include <QFile>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPolygon>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "HardwareInfo.h"
#include "minecraft/MinecraftInstance.h"
#include "ui/dialogs/CustomMessageBox.h"

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif
#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#endif
#ifdef Q_OS_MACOS
#include <libproc.h>
#endif

namespace {
/* Number of samples kept in the rolling history of the charts. */
constexpr int HistorySize = 90;

/* One raw reading of the game process statistics. cpuSeconds is the total CPU
 * time (user + system) the process has consumed, the percentage shown in the
 * page is derived from the deltas of two consecutive readings. */
struct ProcessStat {
    bool valid = false;
    bool supported = true;
    double cpuSeconds = 0.0;
    double ramMiB = 0.0;
    double uptimeSeconds = 0.0;
};

QString formatDuration(qint64 seconds)
{
    seconds = qMax<qint64>(seconds, 0);
    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    const qint64 restSeconds = seconds % 60;

    if (hours > 0) {
        return QObject::tr("%1h %2m").arg(hours).arg(minutes);
    }
    if (minutes > 0) {
        return QObject::tr("%1m %2s").arg(minutes).arg(restSeconds);
    }
    return QObject::tr("%1s").arg(restSeconds);
}

QString formatChartValue(double value)
{
    if (value >= 100.0) {
        return QString::number(value, 'f', 0);
    }
    return QString::number(value, 'f', 1);
}

#if defined(Q_OS_LINUX)
double readBootTimeSeconds()
{
    // /proc/stat contains a "btime" line with the boot time in seconds since the epoch
    QFile file(QStringLiteral("/proc/stat"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0.0;
    }
    const QString content = QString::fromLatin1(file.readAll());
    const QStringList lines = content.split('\n');
    for (const QString& line : lines) {
        if (line.startsWith(QLatin1String("btime"))) {
            bool ok = false;
            const double value = line.section(' ', 1, 1).toDouble(&ok);
            if (ok) {
                return value;
            }
            break;
        }
    }
    return 0.0;
}

ProcessStat queryProcessStat(qint64 pid)
{
    ProcessStat result;

    // /proc/<pid>/stat: pid (comm) state ppid ... utime stime ... starttime
    // the comm field may contain spaces, so split everything after the last ')'
    QFile statFile(QStringLiteral("/proc/") + QString::number(pid) + QStringLiteral("/stat"));
    if (!statFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return result;
    }
    const QString statContent = QString::fromLatin1(statFile.readAll());
    const int lastParenthesis = statContent.lastIndexOf(')');
    if (lastParenthesis < 0) {
        return result;
    }
    const QStringList fields = statContent.mid(lastParenthesis + 1).simplified().split(' ');
    // the first entry behind the comm field is "state", the third field of the file
    enum FieldOffsets { utimeOffset = 11, stimeOffset = 12, starttimeOffset = 19 };
    if (fields.size() <= starttimeOffset) {
        return result;
    }

    bool utimeOk = false;
    bool stimeOk = false;
    bool starttimeOk = false;
    const double utime = fields.at(utimeOffset).toDouble(&utimeOk);
    const double stime = fields.at(stimeOffset).toDouble(&stimeOk);
    const double starttime = fields.at(starttimeOffset).toDouble(&starttimeOk);
    if (!utimeOk || !stimeOk || !starttimeOk) {
        return result;
    }

    // utime and stime are counted in clock ticks
    static const double clockTicksPerSecond = double(sysconf(_SC_CLK_TCK));
    if (clockTicksPerSecond <= 0.0) {
        return result;
    }

    // resident set size from /proc/<pid>/status
    double rssKiB = 0.0;
    QFile statusFile(QStringLiteral("/proc/") + QString::number(pid) + QStringLiteral("/status"));
    if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString content = QString::fromLatin1(statusFile.readAll());
        const QStringList lines = content.split('\n');
        for (const QString& line : lines) {
            if (line.startsWith(QLatin1String("VmRSS:"))) {
                bool ok = false;
                const double value = line.simplified().section(' ', 1, 1).toDouble(&ok);
                if (ok) {
                    rssKiB = value;
                }
                break;
            }
        }
    }

    result.valid = true;
    result.cpuSeconds = (utime + stime) / clockTicksPerSecond;
    result.ramMiB = rssKiB / 1024.0;

    static const double bootTimeSeconds = readBootTimeSeconds();
    if (bootTimeSeconds > 0.0) {
        const double processStart = bootTimeSeconds + starttime / clockTicksPerSecond;
        result.uptimeSeconds = qMax(0.0, double(QDateTime::currentDateTime().toSecsSinceEpoch()) - processStart);
    }
    return result;
}
#elif defined(Q_OS_WIN)
ProcessStat queryProcessStat(qint64 pid)
{
    ProcessStat result;

    // access rights mirror the winapi usage in HardwareInfo.cpp
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, static_cast<DWORD>(pid));
    if (!process) {
        return result;
    }

    FILETIME createTime = {};
    FILETIME exitTime = {};
    FILETIME kernelTime = {};
    FILETIME userTime = {};
    if (!GetProcessTimes(process, &createTime, &exitTime, &kernelTime, &userTime)) {
        CloseHandle(process);
        return result;
    }

    PROCESS_MEMORY_COUNTERS memoryCounters = {};
    const BOOL memoryOk = GetProcessMemoryInfo(process, &memoryCounters, sizeof(memoryCounters));

    CloseHandle(process);

    auto fileTimeTo100ns = [](const FILETIME& time) -> double {
        ULARGE_INTEGER value;
        value.LowPart = time.dwLowDateTime;
        value.HighPart = time.dwHighDateTime;
        return double(value.QuadPart);
    };

    // FILETIME counts 100 nanosecond ticks since 1601-01-01, QDateTime uses the Unix epoch
    const qint64 epochDifferenceSec = Q_INT64_C(11644473600);
    const double nowSec = double(QDateTime::currentDateTime().toSecsSinceEpoch() + epochDifferenceSec);

    result.valid = true;
    // kernel and user time combined
    result.cpuSeconds = (fileTimeTo100ns(kernelTime) + fileTimeTo100ns(userTime)) / 10000000.0;
    result.ramMiB = double(memoryCounters.WorkingSetSize) / 1024.0 / 1024.0;
    result.uptimeSeconds = qMax(0.0, nowSec - fileTimeTo100ns(createTime) / 10000000.0);
    if (!memoryOk) {
        result.ramMiB = 0.0;
    }
    return result;
}
#elif defined(Q_OS_MACOS)
ProcessStat queryProcessStat(qint64 pid)
{
    ProcessStat result;

    struct rusage_info_v4 usage = {};
    if (proc_pid_rusage(static_cast<int>(pid), RUSAGE_INFO_V4, (rusage_info_t*)&usage) != 0) {
        return result;
    }

    result.valid = true;
    // ri_user_time and ri_system_time are counted in microseconds
    result.cpuSeconds = (double(usage.ri_user_time) + double(usage.ri_system_time)) / 1000000.0;
    // ri_footprint is the resident memory footprint in bytes
    result.ramMiB = double(usage.ri_phys_footprint) / 1024.0 / 1024.0;

    struct proc_bsdinfo info = {};
    if (proc_pidinfo(static_cast<int>(pid), PROC_PIDTBSDINFO, 0, &info, sizeof(info)) > 0) {
        const double startedSec = double(info.pbi_start_tvsec) + double(info.pbi_start_tvusec) / 1000000.0;
        result.uptimeSeconds = qMax(0.0, double(QDateTime::currentDateTime().toSecsSinceEpoch()) - startedSec);
    }
    return result;
}
#else
ProcessStat queryProcessStat(qint64)
{
    // live game process statistics are not implemented for this platform
    ProcessStat result;
    result.supported = false;
    return result;
}
#endif
}  // namespace

HistoryChart::HistoryChart(QString unit, qreal fixedMax, QColor color, QWidget* parent)
    : QWidget(parent), m_unit(std::move(unit)), m_color(std::move(color)), m_fixedMax(fixedMax)
{
    setMinimumHeight(120);
}

void HistoryChart::addSample(double value)
{
    m_history.append(qMax(0.0, value));
    while (m_history.size() > HistorySize) {
        m_history.removeFirst();
    }
    update();
}

void HistoryChart::clear()
{
    m_history.clear();
    update();
}

void HistoryChart::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF chart = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
    if (m_history.isEmpty() || chart.width() < 8.0 || chart.height() < 24.0) {
        return;
    }

    double minimum = m_history.first();
    double maximum = m_history.first();
    for (double sample : m_history) {
        minimum = qMin(minimum, sample);
        maximum = qMax(maximum, sample);
    }

    // the vertical scale always covers at least the fixed maximum (100% CPU usage)
    qreal scaleMaximum = qMax<qreal>(m_fixedMax, qreal(maximum));
    if (scaleMaximum <= 0.0) {
        scaleMaximum = 1.0;
    }

    const QFontMetrics metrics = painter.fontMetrics();
    const int textHeight = metrics.height();
    const qreal topMargin = qreal(textHeight) + 2.0;
    const qreal bottomMargin = 4.0;
    const qreal leftMargin = 4.0;
    const qreal rightMargin = 4.0;

    const QPointF origin(chart.left() + leftMargin, chart.bottom() - bottomMargin);
    const qreal usableWidth = chart.width() - leftMargin - rightMargin;
    const qreal usableHeight = chart.height() - topMargin - bottomMargin;

    // labels for the observed minimum and maximum, theme-adaptive text color
    const QColor textColor = palette().color(QPalette::WindowText);
    painter.setPen(textColor);
    const QString minimumText = tr("min %1 %2").arg(formatChartValue(minimum), m_unit);
    painter.drawText(QPointF(origin.x(), chart.top() + textHeight), minimumText);
    const QString maximumText = tr("max %1 %2").arg(formatChartValue(maximum), m_unit);
    painter.drawText(QPointF(chart.right() - rightMargin - qreal(metrics.horizontalAdvance(maximumText)), chart.top() + textHeight),
                     maximumText);

    // baseline
    painter.drawLine(QPointF(chart.left(), origin.y()), QPointF(chart.right(), origin.y()));

    // dashed reference line at the fixed maximum, the 100% mark for CPU usage
    if (m_fixedMax > 0.0) {
        const qreal referenceY = origin.y() - (m_fixedMax / scaleMaximum) * usableHeight;
        painter.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DotLine));
        painter.drawLine(QPointF(chart.left(), referenceY), QPointF(chart.right(), referenceY));
    }

    // the samples, spread over the whole width of the chart
    QPolygonF polyline;
    const int count = m_history.size();
    const qreal dx = count > 1 ? usableWidth / qreal(count - 1) : 0.0;
    for (int i = 0; i < count; ++i) {
        const qreal x = origin.x() + qreal(i) * dx;
        const qreal y = origin.y() - (qreal(m_history.at(i)) / scaleMaximum) * usableHeight;
        polyline.append(QPointF(x, y));
    }

    // translucent fill between the line and the baseline
    QPolygonF area = polyline;
    area.append(QPointF(polyline.last().x(), origin.y()));
    area.append(QPointF(polyline.first().x(), origin.y()));

    QColor fillColor = m_color;
    fillColor.setAlpha(48);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);
    painter.drawPolygon(area);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(m_color, 2));
    painter.drawPolyline(polyline);
}

PerformancePage::PerformancePage(MinecraftInstance* inst, QWidget* parent) : QWidget(parent), m_inst(inst)
{
    createUI();
    retranslateStrings();

    m_pollTimer.setInterval(2000);
    connect(&m_pollTimer, &QTimer::timeout, this, &PerformancePage::refresh);
    connect(m_inst, &BaseInstance::runningStatusChanged, this, &PerformancePage::onRunningChanged);

    refreshHardware();
    refresh();
}

PerformancePage::~PerformancePage() = default;

QIcon PerformancePage::icon() const
{
    auto icon = QIcon::fromTheme("performance");
    if (icon.isNull()) {
        icon = QIcon::fromTheme("settings");
    }
    return icon;
}

void PerformancePage::createUI()
{
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    auto scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    mainLayout->addWidget(scrollArea);

    auto contentWidget = new QWidget(scrollArea);
    auto contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setSpacing(12);
    scrollArea->setWidget(contentWidget);

    // playtime section
    m_playtimeGroup = new QGroupBox(contentWidget);
    auto playtimeLayout = new QVBoxLayout(m_playtimeGroup);
    auto playtimeForm = new QFormLayout();

    m_totalTimeCaption = new QLabel(m_playtimeGroup);
    m_totalTimeLabel = new QLabel(m_playtimeGroup);
    playtimeForm->addRow(m_totalTimeCaption, m_totalTimeLabel);

    m_lastSessionCaption = new QLabel(m_playtimeGroup);
    m_lastSessionLabel = new QLabel(m_playtimeGroup);
    playtimeForm->addRow(m_lastSessionCaption, m_lastSessionLabel);

    m_lastLaunchCaption = new QLabel(m_playtimeGroup);
    m_lastLaunchLabel = new QLabel(m_playtimeGroup);
    playtimeForm->addRow(m_lastLaunchCaption, m_lastLaunchLabel);

    m_launchCountCaption = new QLabel(m_playtimeGroup);
    m_launchCountLabel = new QLabel(m_playtimeGroup);
    playtimeForm->addRow(m_launchCountCaption, m_launchCountLabel);

    playtimeLayout->addLayout(playtimeForm);

    m_resetButton = new QPushButton(m_playtimeGroup);
    connect(m_resetButton, &QPushButton::clicked, this, &PerformancePage::onResetPlaytimeClicked);
    playtimeLayout->addWidget(m_resetButton, 0, Qt::AlignLeft);

    contentLayout->addWidget(m_playtimeGroup);

    // hardware section
    m_hardwareGroup = new QGroupBox(contentWidget);
    auto hardwareForm = new QFormLayout(m_hardwareGroup);

    m_cpuCaption = new QLabel(m_hardwareGroup);
    m_cpuLabel = new QLabel(m_hardwareGroup);
    m_cpuLabel->setWordWrap(true);
    hardwareForm->addRow(m_cpuCaption, m_cpuLabel);

    m_coresCaption = new QLabel(m_hardwareGroup);
    m_coresLabel = new QLabel(m_hardwareGroup);
    hardwareForm->addRow(m_coresCaption, m_coresLabel);

    m_ramCaption = new QLabel(m_hardwareGroup);
    m_ramLabel = new QLabel(m_hardwareGroup);
    hardwareForm->addRow(m_ramCaption, m_ramLabel);

    m_gpuCaption = new QLabel(m_hardwareGroup);
    m_gpuCaption->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_gpuLabel = new QLabel(m_hardwareGroup);
    m_gpuLabel->setWordWrap(true);
    m_gpuLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    hardwareForm->addRow(m_gpuCaption, m_gpuLabel);

    contentLayout->addWidget(m_hardwareGroup);

    // game process section
    m_processGroup = new QGroupBox(contentWidget);
    auto processLayout = new QVBoxLayout(m_processGroup);

    m_hintLabel = new QLabel(m_processGroup);
    m_hintLabel->setWordWrap(true);
    processLayout->addWidget(m_hintLabel);

    m_processDetails = new QWidget(m_processGroup);
    auto processForm = new QFormLayout(m_processDetails);

    m_pidCaption = new QLabel(m_processDetails);
    m_pidLabel = new QLabel(m_processDetails);
    processForm->addRow(m_pidCaption, m_pidLabel);

    m_cpuUsageCaption = new QLabel(m_processDetails);
    m_cpuUsageLabel = new QLabel(m_processDetails);
    processForm->addRow(m_cpuUsageCaption, m_cpuUsageLabel);

    m_ramUsageCaption = new QLabel(m_processDetails);
    m_ramUsageLabel = new QLabel(m_processDetails);
    processForm->addRow(m_ramUsageCaption, m_ramUsageLabel);

    m_uptimeCaption = new QLabel(m_processDetails);
    m_uptimeLabel = new QLabel(m_processDetails);
    processForm->addRow(m_uptimeCaption, m_uptimeLabel);

    m_cpuNoteLabel = new QLabel(m_processDetails);
    m_cpuNoteLabel->setWordWrap(true);
    processForm->addRow(QString(), m_cpuNoteLabel);

    processLayout->addWidget(m_processDetails);

    // the two rolling charts: CPU usage in percent and memory usage in MB
    // fixed, medium-light colors that stay readable on both light and dark themes
    m_cpuChartCaption = new QLabel(m_processDetails);
    m_ramChartCaption = new QLabel(m_processDetails);
    m_cpuChart = new HistoryChart(QStringLiteral("%"), 100.0, QColor(0x3d, 0xa9, 0x5f), m_processDetails);
    m_ramChart = new HistoryChart(QStringLiteral("MB"), 0.0, QColor(0x4f, 0x92, 0xd9), m_processDetails);

    auto chartsLayout = new QHBoxLayout();
    auto cpuColumn = new QVBoxLayout();
    cpuColumn->addWidget(m_cpuChartCaption);
    cpuColumn->addWidget(m_cpuChart);
    auto ramColumn = new QVBoxLayout();
    ramColumn->addWidget(m_ramChartCaption);
    ramColumn->addWidget(m_ramChart);
    chartsLayout->addLayout(cpuColumn);
    chartsLayout->addLayout(ramColumn);
    processLayout->addLayout(chartsLayout);

    contentLayout->addWidget(m_processGroup);
    contentLayout->addStretch(1);
}

void PerformancePage::retranslateStrings()
{
    m_playtimeGroup->setTitle(tr("Playtime"));
    m_totalTimeCaption->setText(tr("Total time played:"));
    m_lastSessionCaption->setText(tr("Last session:"));
    m_lastLaunchCaption->setText(tr("Last launched:"));
    m_launchCountCaption->setText(tr("Launch count:"));
    m_resetButton->setText(tr("Reset playtime"));

    m_hardwareGroup->setTitle(tr("Hardware"));
    m_cpuCaption->setText(tr("CPU:"));
    m_coresCaption->setText(tr("Cores:"));
    m_ramCaption->setText(tr("RAM:"));
    m_gpuCaption->setText(tr("GPUs:"));

    m_processGroup->setTitle(tr("Game process"));
    m_hintLabel->setText(tr("Launch the game to see live statistics."));
    m_pidCaption->setText(tr("PID:"));
    m_cpuUsageCaption->setText(tr("CPU usage:"));
    m_ramUsageCaption->setText(tr("Memory usage:"));
    m_uptimeCaption->setText(tr("Uptime:"));
    m_cpuNoteLabel->setText(tr("CPU usage is measured as a share of a single core and can exceed 100% on multi-core systems."));
    m_cpuChartCaption->setText(tr("CPU usage (%)"));
    m_ramChartCaption->setText(tr("Memory usage (MB)"));
}

void PerformancePage::retranslate()
{
    retranslateStrings();
    refreshHardware();
    refresh();
}

void PerformancePage::openedImpl()
{
    refreshHardware();
    refresh();
    m_pollTimer.start();
}

void PerformancePage::closedImpl()
{
    m_pollTimer.stop();
}

void PerformancePage::refresh()
{
    refreshPlaytime();
    refreshProcess();
}

void PerformancePage::onRunningChanged(bool running)
{
    Q_UNUSED(running)
    refresh();
}

void PerformancePage::onResetPlaytimeClicked()
{
    auto response = CustomMessageBox::selectable(this,
                                                 tr("Confirm Reset"),
                                                 tr("You are about to reset the playtime statistics of this instance.\n"
                                                    "The total time played, the last session and the launch count\n"
                                                    "will be reset to zero.\n\n"
                                                    "Are you sure you want to do this?"),
                                                 QMessageBox::Warning,
                                                 QMessageBox::Yes | QMessageBox::No,
                                                 QMessageBox::No)
                        ->exec();
    if (response != QMessageBox::Yes) {
        return;
    }

    m_inst->resetTimePlayed();
    m_inst->settings()->reset("launchCount");
    refreshPlaytime();
}

void PerformancePage::refreshPlaytime()
{
    m_totalTimeLabel->setText(formatDuration(m_inst->totalTimePlayed()));
    m_lastSessionLabel->setText(formatDuration(m_inst->lastTimePlayed()));

    const qint64 lastLaunch = m_inst->lastLaunch();
    if (lastLaunch > 0) {
        m_lastLaunchLabel->setText(QLocale().toString(QDateTime::fromMSecsSinceEpoch(lastLaunch), QLocale::ShortFormat));
    } else {
        m_lastLaunchLabel->setText(tr("never"));
    }

    m_launchCountLabel->setText(QString::number(m_inst->settings()->get("launchCount").toInt()));
}

void PerformancePage::refreshHardware()
{
    const QString cpuInfo = HardwareInfo::cpuInfo();
    m_cpuLabel->setText(cpuInfo.isEmpty() ? tr("unknown") : cpuInfo);

    const uint32_t cores = HardwareInfo::cpuCores();
    m_coresLabel->setText(cores > 0 ? QString::number(cores) : tr("unknown"));

    const uint64_t totalMiB = HardwareInfo::totalRamMiB();
    const uint64_t availableMiB = HardwareInfo::availableRamMiB();
    if (totalMiB > 0) {
        QString ramText = tr("%1 GB total").arg(QString::number(totalMiB / 1024.0, 'f', 1));
        if (availableMiB > 0) {
            ramText += tr(", %1 GB available").arg(QString::number(availableMiB / 1024.0, 'f', 1));
        }
        m_ramLabel->setText(ramText);
    } else {
        m_ramLabel->setText(tr("unknown"));
    }

    const QStringList gpus = HardwareInfo::gpuInfo();
    m_gpuLabel->setText(gpus.isEmpty() ? tr("unknown") : gpus.join(QLatin1Char('\n')));
}

void PerformancePage::refreshProcess()
{
    const bool canPoll = m_inst->isRunning() && m_inst->runningPid() > 0;

    m_processDetails->setEnabled(canPoll);
    m_hintLabel->setVisible(!canPoll);

    if (!canPoll) {
        resetSamplingState();
        return;
    }

    const qint64 pid = m_inst->runningPid();
    if (pid != m_lastPolledPid) {
        // a new game process: start over with a fresh history
        resetSamplingState();
        m_lastPolledPid = pid;
    }

    const ProcessStat stat = queryProcessStat(pid);
    if (!stat.supported) {
        m_pidLabel->setText(QString::number(pid));
        m_cpuUsageLabel->setText(tr("not available on this platform"));
        m_ramUsageLabel->setText(tr("not available on this platform"));
        m_uptimeLabel->setText(tr("not available on this platform"));
        return;
    }

    if (!stat.valid) {
        // the process died between polls: stop updating the live statistics
        m_cpuUsageLabel->setText(tr("not running"));
        m_ramUsageLabel->setText(tr("not running"));
        m_uptimeLabel->setText(tr("not running"));
        return;
    }

    double cpuPercent = 0.0;
    if (m_lastCpuSeconds >= 0.0) {
        const double elapsed = double(m_lastSampleTime.msecsTo(QDateTime::currentDateTime())) / 1000.0;
        if (elapsed > 0.0) {
            // share of a single core; can exceed 100% on multi-core systems
            cpuPercent = qMax(0.0, (stat.cpuSeconds - m_lastCpuSeconds) / elapsed * 100.0);
        }
    }
    m_lastCpuSeconds = stat.cpuSeconds;
    m_lastSampleTime = QDateTime::currentDateTime();

    m_pidLabel->setText(QString::number(pid));
    m_cpuUsageLabel->setText(tr("%1%").arg(QString::number(cpuPercent, 'f', 1)));
    m_ramUsageLabel->setText(tr("%1 MB").arg(QString::number(stat.ramMiB, 'f', 0)));
    m_uptimeLabel->setText(stat.uptimeSeconds > 0.0 ? formatDuration(qint64(stat.uptimeSeconds)) : tr("unknown"));

    m_cpuChart->addSample(cpuPercent);
    m_ramChart->addSample(stat.ramMiB);
}

void PerformancePage::resetSamplingState()
{
    m_lastPolledPid = 0;
    m_lastCpuSeconds = -1.0;
    m_lastSampleTime = QDateTime();
    m_cpuChart->clear();
    m_ramChart->clear();
}
