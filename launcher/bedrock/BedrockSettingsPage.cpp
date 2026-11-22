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

#include "BedrockSettingsPage.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

/* A path editor with a Browse button, as one form row widget. */
QWidget* pathRow(QLineEdit* editor, QWidget* parent)
{
    auto row = new QWidget(parent);
    auto layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    editor->setPlaceholderText(QObject::tr("(automatic)"));
    layout->addWidget(editor, 1);
    auto browse = new QPushButton(QObject::tr("Browse..."), parent);
    layout->addWidget(browse);
    QObject::connect(browse, &QPushButton::clicked, parent, [editor, parent]() {
        const QString path = QFileDialog::getOpenFileName(parent, QObject::tr("Pick the file"), editor->text());
        if (!path.isEmpty()) {
            editor->setText(path);
        }
    });
    return row;
}

}  // namespace

BedrockSettingsPage::BedrockSettingsPage(BedrockInstance* instance, QWidget* parent) : QWidget(parent), m_instance(instance)
{
    auto layout = new QVBoxLayout(this);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setWordWrap(true);
    layout->addWidget(m_infoLabel);

    auto form = new QFormLayout();

#if defined(Q_OS_WIN)
    m_separateGameData = new QCheckBox(tr("Keep this instance's game data separate (worlds, packs, settings)"), this);
    m_separateGameData->setChecked(m_instance->settings()->get("BedrockSeparateGameData").toBool());
    m_separateGameData->setToolTip(tr("The game's data folder is linked to this instance's own folder on launch. When "
                                       "unchecked, the instance shares the game's default data folder with everything "
                                       "else on the machine."));
    form->addRow(QString(), m_separateGameData);

    m_launchHelperPath = new QLineEdit(this);
    m_launchHelperPath->setText(m_instance->settings()->get("BedrockGdkLaunchHelperPath").toString());
    m_launchHelperPath->setToolTip(tr("BedrockLauncher's gamelaunchhelper.dll: pins the exact installed GDK version "
                                       "instead of the bootstrapper that auto-updates the game. Deployed on the next "
                                       "launch when set."));
    form->addRow(tr("GDK launch helper DLL"), pathRow(m_launchHelperPath, this));
#endif

#if defined(Q_OS_LINUX)
    m_xodusPath = new QLineEdit(this);
    m_xodusPath->setText(m_instance->settings()->get("BedrockXodusPath").toString());
    m_xodusPath->setToolTip(tr("A locally built xodus-cli used instead of the hash-pinned managed copy the launcher "
                               "downloads on first use."));
    form->addRow(tr("Xodus executable"), pathRow(m_xodusPath, this));

    m_launchCommand = new QLineEdit(this);
    m_launchCommand->setText(m_instance->settings()->get("BedrockLinuxLaunchCommand").toString());
    m_launchCommand->setPlaceholderText(QStringLiteral("{xodus} run {game_dir}"));
    m_launchCommand->setToolTip(tr("The full launch command of the experimental backend. Placeholders: {umu_run}, "
                                    "{engine}, {prefix}, {xodus}, {game_dir}."));
    form->addRow(tr("Custom launch command"), m_launchCommand);

    m_msaRefreshToken = new QLineEdit(this);
    m_msaRefreshToken->setText(m_instance->settings()->get("BedrockMsaRefreshToken").toString());
    m_msaRefreshToken->setEchoMode(QLineEdit::Password);
    m_msaRefreshToken->setToolTip(tr("Advanced: written into the instance prefix's WineGDK registry for the in-game "
                                     "Xbox sign-in. Leave empty to sign in inside the game."));
    form->addRow(tr("Microsoft account refresh token"), m_msaRefreshToken);
#endif

    layout->addLayout(form);
    layout->addStretch(1);

    retranslate();
}

BedrockSettingsPage::~BedrockSettingsPage() {}

bool BedrockSettingsPage::apply()
{
#if defined(Q_OS_WIN)
    m_instance->settings()->set("BedrockSeparateGameData", m_separateGameData->isChecked());
    m_instance->settings()->set("BedrockGdkLaunchHelperPath", m_launchHelperPath->text().trimmed());
#endif
#if defined(Q_OS_LINUX)
    m_instance->settings()->set("BedrockXodusPath", m_xodusPath->text().trimmed());
    m_instance->settings()->set("BedrockLinuxLaunchCommand", m_launchCommand->text().trimmed());
    m_instance->settings()->set("BedrockMsaRefreshToken", m_msaRefreshToken->text().trimmed());
#endif
    return true;
}

void BedrockSettingsPage::retranslate()
{
#if defined(Q_OS_WIN)
    m_infoLabel->setText(tr(
        "A Bedrock instance is a data profile around the installed game: the game itself is installed once per version "
        "and shared, and this instance keeps its own game data (worlds, resource packs, behavior packs, settings) in its "
        "own folder that the game is linked to on launch.\n\n"
        "Only one GDK-era version can be installed at a time; switching versions reinstalls the shared game (the "
        "launcher asks first). Versions up to 1.21.120.20 install as legacy UWP packages and need Windows developer "
        "mode. Java-related settings do not apply to Bedrock instances."));
#elif defined(Q_OS_LINUX)
    m_infoLabel->setText(tr(
        "The experimental Linux backend runs the Windows game through the GDK-Proton engine and the external Xodus "
        "licensing client, both fetched on first use and hash-pinned. The per-instance Wine prefix keeps the game's "
        "data separate from other instances.\n\n"
        "Requires a Vulkan 1.3-class driver, internet at every game start, and a Microsoft account that owns the game; "
        "achievements do not unlock. Run the licensing client's login once outside the launcher to sign in. "
        "Java-related settings do not apply to Bedrock instances."));
#else
    m_infoLabel->setText(tr(
        "Bedrock Edition has no backend on this platform: the Windows backend needs Windows, the Linux backend needs "
        "Linux, and no macOS backend is planned. Java-related settings do not apply to Bedrock instances."));
#endif
}
