// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
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

#include "NewInstanceDialog.h"
#include "Application.h"
#include "DiscordRichPresence.h"
#include "ui/pages/modplatform/ModpackProviderBasePage.h"
#include "ui/pages/modplatform/import_ftb/ImportFTBPage.h"
#include "ui_NewInstanceDialog.h"

#include <BaseVersion.h>
#include <InstanceList.h>
#include <icons/IconList.h>
#include <tasks/Task.h>

#include "IconPickerDialog.h"
#include "ProgressDialog.h"
#include "VersionSelectDialog.h"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QInputDialog>
#include <QLayout>
#include <QMenu>
#include <QMimeData>
#include <QScreen>
#include <QShowEvent>
#include <QToolButton>
#include <QUrl>
#include <QValidator>
#include <utility>

#include "ui/pages/modplatform/CustomPage.h"
#include "ui/pages/modplatform/ImportPage.h"
#include "ui/pages/modplatform/atlauncher/AtlPage.h"
#include "ui/pages/modplatform/flame/FlamePage.h"
#include "ui/pages/modplatform/ftb/FtbModpacksPage.h"
#include "ui/pages/modplatform/modrinth/ModrinthPage.h"
#include "ui/pages/modplatform/technic/TechnicPage.h"
#include "ui/widgets/DialogButton.h"
#include "ui/widgets/ModpackSearchBox.h"
#include "ui/widgets/PageContainer.h"

namespace {
/** True for drops the Import page can handle: .zip / .mrpack files and modpack links. */
bool isModpackDrop(const QList<QUrl>& urls)
{
    for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
            const QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
            if (suffix == "zip" || suffix == "mrpack")
                return true;
        } else if (const QString scheme = url.scheme();
                   scheme == "http" || scheme == "https" || scheme == "curseforge") {
            return true;
        }
    }
    return false;
}
}  // namespace

NewInstanceDialog::NewInstanceDialog(const QString& initialGroup,
                                     const QString& url,
                                     const QMap<QString, QString>& extra_info,
                                     QWidget* parent)
    : QDialog(parent), ui(new Ui::NewInstanceDialog)
{
    ui->setupUi(this);

    setWindowIcon(QIcon::fromTheme("new"));
    setAcceptDrops(true);

    InstIconKey = "default";
    ui->iconButton->setIcon(APPLICATION->icons()->getIcon(InstIconKey));
    ui->iconButton->setStyleSheet(QStringLiteral("QToolButton { border-radius: 8px; }"));

    // the group chip: a menu of existing groups plus "New group…", like the old combo box
    m_groupChip = ui->groupButton;
    m_groups = APPLICATION->instances()->getGroups();
    m_groups.removeAll("");
    m_groups.prepend("");
    m_currentGroup = m_groups.contains(initialGroup) ? initialGroup : QString();
    if (!initialGroup.isEmpty() && !m_groups.contains(initialGroup))
        m_groups.append(initialGroup);
    buildGroupMenu();
    setCurrentGroup(m_currentGroup);

    // NOTE: the buttons must be initialized before PageContainer, because the pages indirectly
    // access them through setSuggestedPack! Do not move this below.
    m_helpButton = new DialogButton(tr("Help"), DialogButtonRole::Ghost, this);
    m_cancelButton = new DialogButton(tr("Cancel"), DialogButtonRole::Secondary, this);
    m_primaryButton = new DialogButton(tr("Create instance"), DialogButtonRole::Primary, this);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(false);
    auto* mutedHint = new QGraphicsOpacityEffect(m_hintLabel);
    mutedHint->setOpacity(0.7);
    m_hintLabel->setGraphicsEffect(mutedHint);
    QFont hintFont = m_hintLabel->font();
    if (hintFont.pointSizeF() > 2.)
        hintFont.setPointSizeF(hintFont.pointSizeF() - 1.);
    m_hintLabel->setFont(hintFont);

    // open on the last used page (Custom on first run); a URL deep link goes to Import
    QString initialPage = APPLICATION->settings()->get("NewInstanceLastPage").toString();
    if (!url.isEmpty())
        initialPage = "import";
    m_container = new PageContainer(this, initialPage, this);
    m_container->useSidebarStyle(false);
    m_container->setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Expanding);
    m_container->layout()->setContentsMargins(0, 0, 0, 0);
    ui->verticalLayout->insertWidget(1, m_container);

    // the sidebar search box becomes the "Search all modpacks" box (Modrinth + CurseForge)
    m_searchBox = new ModpackSearchBox(this);
    m_searchBox->addSource("modrinth", tr("Modrinth"), modrinthPage->searchModel());
    if (flamePage)
        m_searchBox->addSource("flame", tr("CurseForge"), flamePage->searchModel());
    connect(m_searchBox, &ModpackSearchBox::searchRequested, this, &NewInstanceDialog::modpackSearchRequested);
    connect(m_searchBox, &ModpackSearchBox::resultActivated, this, &NewInstanceDialog::modpackSearchResultActivated);
    m_container->setSearchWidget(m_searchBox);

    // [Help] ............ hint  [Cancel] [primary]
    auto* buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(0, 8, 0, 0);
    buttonRow->setSpacing(8);
    m_hintLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    buttonRow->addWidget(m_helpButton);
    buttonRow->addWidget(m_hintLabel, 1);
    buttonRow->addWidget(m_cancelButton);
    buttonRow->addWidget(m_primaryButton);
    m_container->addButtons(buttonRow);

    // Bonk Qt over its stupid head and make sure it understands which button is the default one...
    // See: https://stackoverflow.com/questions/24556831/qbuttonbox-set-default-button
    m_primaryButton->setDefault(true);
    m_primaryButton->setAutoDefault(true);
    m_cancelButton->setDefault(false);
    m_cancelButton->setAutoDefault(false);
    connect(m_primaryButton, &QPushButton::clicked, this, &NewInstanceDialog::accept);
    connect(m_cancelButton, &QPushButton::clicked, this, &NewInstanceDialog::reject);
    connect(m_helpButton, &QPushButton::clicked, m_container, &PageContainer::help);

    if (!url.isEmpty()) {
        QUrl actualUrl(url);
        m_container->selectPage("import");
        importPage->setUrl(url);
        importPage->setExtraInfo(extra_info);
    }

    updateDialogState();

    if (APPLICATION->settings()->get("NewInstanceGeometry").isValid()) {
        restoreGeometry(QByteArray::fromBase64(APPLICATION->settings()->get("NewInstanceGeometry").toString().toUtf8()));
    } else {
        auto screen = parent->screen();
        auto geometry = screen->availableSize();
        resize(width(), qMin(geometry.height() - 50, 710));
    }

    connect(m_container, &PageContainer::selectedPageChanged, this, &NewInstanceDialog::selectedPageChanged);
}

void NewInstanceDialog::reject()
{
    rememberState();

    // This is just so that the pages get the close() call and can react to it, if needed.
    m_container->prepareToClose();

    QDialog::reject();
}

void NewInstanceDialog::accept()
{
    // Enter on a selected pack lands here even without a task - don't close without one
    if (!creationTask)
        return;

    rememberState();
    importIconNow();

    // This is just so that the pages get the close() call and can react to it, if needed.
    m_container->prepareToClose();

    QDialog::accept();
}

void NewInstanceDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    DiscordRichPresence::instance()->updateBrowsing();
}

void NewInstanceDialog::hideEvent(QHideEvent* event)
{
    QDialog::hideEvent(event);
    DiscordRichPresence::instance()->browsingClosed();
}

QList<BasePage*> NewInstanceDialog::getPages()
{
    QList<BasePage*> pages;

    importPage = new ImportPage(this);
    modrinthPage = new ModrinthPage(this);

    // Order: the sidebar groups pages by BasePage::sectionName() - "Create": Custom, Import;
    // "Browse modpacks": Modrinth, CurseForge, ATLauncher, FTB, Technic; "From other launchers": FTB App.
    pages.append(new CustomPage(this));
    pages.append(importPage);
    pages.append(modrinthPage);
    if (APPLICATION->capabilities() & Application::SupportsFlame) {
        flamePage = new FlamePage(this);
        pages.append(flamePage);
    }
    pages.append(new AtlPage(this));
    pages.append(new FtbModpacksPage(this));
    pages.append(new TechnicPage(this));
    pages.append(new FTBImportAPP::ImportFTBPage(this));

    return pages;
}

QString NewInstanceDialog::dialogTitle()
{
    return tr("New Instance");
}

NewInstanceDialog::~NewInstanceDialog()
{
    delete ui;
}

void NewInstanceDialog::setSuggestedPack(const QString& name, InstanceTask* task)
{
    creationTask.reset(task);

    ui->instNameTextBox->setPlaceholderText(name);
    importVersion.clear();

    if (!task) {
        ui->iconButton->setIcon(APPLICATION->icons()->getIcon(InstIconKey));
        importIcon = false;
    }

    updateDialogState();
}

void NewInstanceDialog::setSuggestedPack(const QString& name, QString version, InstanceTask* task)
{
    creationTask.reset(task);

    ui->instNameTextBox->setPlaceholderText(name);
    importVersion = std::move(version);

    if (!task) {
        ui->iconButton->setIcon(APPLICATION->icons()->getIcon(InstIconKey));
        importIcon = false;
    }

    updateDialogState();
}

void NewInstanceDialog::setSuggestedIconFromFile(const QString& path, const QString& name)
{
    importIcon = true;
    importIconPath = path;
    importIconName = name;

    // Hmm, for some reason they can be to small
    ui->iconButton->setIcon(QIcon(path));
}

void NewInstanceDialog::setSuggestedIcon(const QString& key)
{
    if (key == "default")
        return;

    auto icon = APPLICATION->icons()->getIcon(key);
    importIcon = false;

    ui->iconButton->setIcon(icon);
}

InstanceTask* NewInstanceDialog::extractTask()
{
    InstanceTask* extracted = creationTask.release();

    InstanceName inst_name(ui->instNameTextBox->placeholderText().trimmed(), importVersion);
    inst_name.setName(ui->instNameTextBox->text().trimmed());
    extracted->setName(inst_name);

    extracted->setGroup(instGroup());
    extracted->setIcon(iconKey());
    return extracted;
}

void NewInstanceDialog::updateDialogState()
{
    const bool allowPrimary = creationTask && !instName().isEmpty();
    if (m_primaryButton->isEnabled() != allowPrimary) {
        m_primaryButton->setEnabled(allowPrimary);
    }
    updatePrimaryAction();
}

void NewInstanceDialog::updatePrimaryAction()
{
    // can run before the container is finished constructing (pages call setSuggestedPack early)
    if (!m_container)
        return;
    m_primaryButton->setText(pageActionText(m_container->selectedPage()));
    m_hintLabel->setText(pageHintText(m_container->selectedPage()));
}

QString NewInstanceDialog::pageActionText(BasePage* page) const
{
    if (!page)
        return tr("Create instance");
    const QString id = page->id();
    if (id == "vanilla")
        return tr("Create instance");
    if (id == "import" || id == "import_ftb")
        return tr("Import");
    return tr("Install modpack");
}

QString NewInstanceDialog::pageHintText(BasePage* page) const
{
    if (creationTask && !instName().isEmpty())
        return QString();

    if (creationTask && instName().isEmpty())
        return tr("Enter an instance name");

    if (!page)
        return QString();
    const QString id = page->id();
    if (id == "vanilla")
        return tr("Select a Minecraft version");
    if (id == "import")
        return tr("Paste a link or choose a file");
    return tr("Select a modpack");
}

QString NewInstanceDialog::instName() const
{
    auto result = ui->instNameTextBox->text().trimmed();
    if (result.size()) {
        return result;
    }
    result = ui->instNameTextBox->placeholderText().trimmed();
    if (result.size()) {
        return result;
    }
    return QString();
}

QString NewInstanceDialog::instGroup() const
{
    return m_currentGroup;
}

QString NewInstanceDialog::iconKey() const
{
    return InstIconKey;
}

void NewInstanceDialog::on_iconButton_clicked()
{
    importIconNow();  // so the user can switch back
    IconPickerDialog dlg(this);
    dlg.execWithSelection(InstIconKey);

    if (dlg.result() == QDialog::Accepted) {
        InstIconKey = dlg.selectedIconKey;
        ui->iconButton->setIcon(APPLICATION->icons()->getIcon(InstIconKey));
        importIcon = false;
    }
}

void NewInstanceDialog::on_instNameTextBox_textChanged([[maybe_unused]] const QString& arg1)
{
    updateDialogState();
}

void NewInstanceDialog::importIconNow()
{
    if (importIcon) {
        APPLICATION->icons()->installIcon(importIconPath, importIconName);
        InstIconKey = importIconName.mid(0, importIconName.lastIndexOf('.'));
        importIcon = false;
    }
    APPLICATION->settings()->set("NewInstanceGeometry", QString::fromUtf8(saveGeometry().toBase64()));
}

void NewInstanceDialog::rememberState()
{
    APPLICATION->settings()->set("NewInstanceGeometry", QString::fromUtf8(saveGeometry().toBase64()));
    if (auto* page = m_container->selectedPage())
        APPLICATION->settings()->set("NewInstanceLastPage", page->id());
}

void NewInstanceDialog::selectedPageChanged(BasePage* previous, BasePage* selected)
{
    auto prevPage = dynamic_cast<ModpackProviderBasePage*>(previous);
    if (prevPage) {
        m_searchTerm = prevPage->getSerachTerm();
    }

    auto nextPage = dynamic_cast<ModpackProviderBasePage*>(selected);
    if (nextPage) {
        nextPage->setSearchTerm(m_searchTerm);
    }

    updateDialogState();
}

void NewInstanceDialog::buildGroupMenu()
{
    m_groupMenu = new QMenu(m_groupChip);
    for (const QString& group : m_groups) {
        if (group.isEmpty())
            continue;
        auto* action = m_groupMenu->addAction(group);
        action->setCheckable(true);
        connect(action, &QAction::triggered, this, [this, action] { groupChosen(action); });
    }
    m_groupMenu->addSeparator();
    auto* newGroup = m_groupMenu->addAction(tr("New group…"));
    connect(newGroup, &QAction::triggered, this, &NewInstanceDialog::newGroupRequested);
    m_groupChip->setMenu(m_groupMenu);
}

void NewInstanceDialog::groupChosen(QAction* action)
{
    setCurrentGroup(action->text());
}

void NewInstanceDialog::newGroupRequested()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New group"), tr("Group name:"), QLineEdit::Normal, QString(), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        const QString group = name.trimmed();
        if (!m_groups.contains(group)) {
            m_groups.append(group);
            // keep the new group with the other groups, above the separator
            QAction* before = nullptr;
            for (auto* action : m_groupMenu->actions()) {
                if (action->isSeparator()) {
                    before = action;
                    break;
                }
            }
            auto* action = new QAction(group, m_groupMenu);
            action->setCheckable(true);
            m_groupMenu->insertAction(before, action);
            connect(action, &QAction::triggered, this, [this, action] { groupChosen(action); });
        }
        setCurrentGroup(group);
    }
}

void NewInstanceDialog::setCurrentGroup(const QString& group)
{
    m_currentGroup = group;
    m_groupChip->setText(group.isEmpty() ? tr("No group") : group);
    for (auto* action : m_groupMenu->actions()) {
        if (action->isSeparator())
            continue;
        action->setChecked(action->text() == group);
    }
}

void NewInstanceDialog::modpackSearchRequested(const QString& term)
{
    // one clearly separated step per platform: the pages' own search does the work
    modrinthPage->searchModpacks(term);
    if (flamePage)
        flamePage->searchModpacks(term);
}

void NewInstanceDialog::modpackSearchResultActivated(const QString& sourceId, const QModelIndex& sourceIndex)
{
    if (sourceId == "flame" && flamePage) {
        m_container->selectPage("flame");
        flamePage->setCurrentIndex(sourceIndex);
    } else {
        m_container->selectPage("modrinth");
        modrinthPage->setCurrentIndex(sourceIndex);
    }
}

void NewInstanceDialog::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls() && isModpackDrop(event->mimeData()->urls())) {
        event->acceptProposedAction();
        importPage->setDragActive(true);
    }
}

void NewInstanceDialog::dragLeaveEvent(QDragLeaveEvent* event)
{
    importPage->setDragActive(false);
    QDialog::dragLeaveEvent(event);
}

void NewInstanceDialog::dropEvent(QDropEvent* event)
{
    importPage->setDragActive(false);
    const auto urls = event->mimeData()->urls();
    if (!isModpackDrop(urls))
        return;

    const QUrl url = urls.first();
    m_container->selectPage("import");
    importPage->setUrl(url.isLocalFile() ? url.toLocalFile() : url.toString());
    event->acceptProposedAction();
}
