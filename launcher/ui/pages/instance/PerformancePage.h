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

#pragma once

#include <QColor>
#include <QDateTime>
#include <QIcon>
#include <QList>
#include <QString>
#include <QTimer>
#include <QWidget>

#include "ui/pages/BasePage.h"

class QFormLayout;
class QGroupBox;
class QLabel;
class QPushButton;
class QPaintEvent;

class MinecraftInstance;

/*!
 * \brief Minimal rolling line chart drawn with QPainter.
 *
 * Keeps the most recent 90 samples and paints them as a simple polyline,
 * with the observed minimum and maximum values labeled in the corners.
 * No external chart dependencies.
 */
class HistoryChart : public QWidget {
    Q_OBJECT

   public:
    explicit HistoryChart(QString unit, qreal fixedMax, QColor color, QWidget* parent = nullptr);

    void addSample(double value);
    void clear();

   protected:
    void paintEvent(QPaintEvent* event) override;

   private:
    QString m_unit;
    QColor m_color;
    /// lower bound of the vertical scale is always 0; for CPU usage this is 100 so the
    /// usual range stays visible even when the current sample is smaller
    qreal m_fixedMax = 0.0;
    QList<double> m_history;
};

/*!
 * \brief Instance page with playtime statistics, hardware info and live game
 * process statistics (PID, CPU usage, memory usage and uptime with two
 * rolling charts).
 */
class PerformancePage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit PerformancePage(MinecraftInstance* inst, QWidget* parent = nullptr);
    virtual ~PerformancePage();

    virtual QString displayName() const override { return tr("Performance"); }
    virtual QIcon icon() const override;
    virtual QString id() const override { return "performance"; }
    virtual QString helpPage() const override { return QString(); }
    void retranslate() override;

    void openedImpl() override;
    void closedImpl() override;

   private slots:
    void refresh();
    void onRunningChanged(bool running);
    void onResetPlaytimeClicked();

   private:
    void createUI();
    void retranslateStrings();
    void refreshPlaytime();
    void refreshHardware();
    void refreshProcess();
    void resetSamplingState();

   private:  // data
    MinecraftInstance* m_inst;

    // playtime section
    QGroupBox* m_playtimeGroup = nullptr;
    QLabel* m_totalTimeCaption = nullptr;
    QLabel* m_totalTimeLabel = nullptr;
    QLabel* m_lastSessionCaption = nullptr;
    QLabel* m_lastSessionLabel = nullptr;
    QLabel* m_lastLaunchCaption = nullptr;
    QLabel* m_lastLaunchLabel = nullptr;
    QLabel* m_launchCountCaption = nullptr;
    QLabel* m_launchCountLabel = nullptr;
    QPushButton* m_resetButton = nullptr;

    // hardware section
    QGroupBox* m_hardwareGroup = nullptr;
    QLabel* m_cpuCaption = nullptr;
    QLabel* m_cpuLabel = nullptr;
    QLabel* m_coresCaption = nullptr;
    QLabel* m_coresLabel = nullptr;
    QLabel* m_ramCaption = nullptr;
    QLabel* m_ramLabel = nullptr;
    QLabel* m_gpuCaption = nullptr;
    QLabel* m_gpuLabel = nullptr;

    // game process section
    QGroupBox* m_processGroup = nullptr;
    QLabel* m_hintLabel = nullptr;
    QWidget* m_processDetails = nullptr;
    QLabel* m_pidCaption = nullptr;
    QLabel* m_pidLabel = nullptr;
    QLabel* m_cpuUsageCaption = nullptr;
    QLabel* m_cpuUsageLabel = nullptr;
    QLabel* m_ramUsageCaption = nullptr;
    QLabel* m_ramUsageLabel = nullptr;
    QLabel* m_uptimeCaption = nullptr;
    QLabel* m_uptimeLabel = nullptr;
    QLabel* m_cpuNoteLabel = nullptr;
    QLabel* m_cpuChartCaption = nullptr;
    QLabel* m_ramChartCaption = nullptr;
    HistoryChart* m_cpuChart = nullptr;
    HistoryChart* m_ramChart = nullptr;

    QTimer m_pollTimer;

    // state of the last live sample, used to derive percentages from deltas
    qint64 m_lastPolledPid = 0;
    double m_lastCpuSeconds = -1.0;
    QDateTime m_lastSampleTime;
};
