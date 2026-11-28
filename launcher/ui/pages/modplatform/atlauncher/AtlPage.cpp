// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
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
 *      Copyright 2020-2021 Jamie Mansfield <jmansfield@cadixdev.org>
 *      Copyright 2021 Philip T <me@phit.link>
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

#include "AtlPage.h"
#include "ui/widgets/ModpackDetailsHeader.h"
#include "ui/widgets/ModpackItemDelegate.h"
#include "ui/widgets/ProjectItem.h"
#include "ui_AtlPage.h"

#include "BuildConfig.h"
#include "StringUtils.h"

#include "AtlUserInteractionSupportImpl.h"
#include "modplatform/atlauncher/ATLPackInstallTask.h"
#include "ui/dialogs/NewInstanceDialog.h"

#include <QKeyEvent>
#include <QMessageBox>

AtlPage::AtlPage(NewInstanceDialog* dialog, QWidget* parent) : QWidget(parent), ui(new Ui::AtlPage), dialog(dialog)
{
    ui->setupUi(this);

    filterModel = new Atl::FilterModel(this);
    listModel = new Atl::ListModel(this);
    filterModel->setSourceModel(listModel);
    ui->packView->setModel(filterModel);
    ui->packView->setItemDelegate(new ModpackItemDelegate(this));
    ui->packView->setEmptyText(tr("No modpacks found"));
    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 1);

    // don't let the description pane swallow drag and drop meant for the dialog
    ui->packDescription->setAcceptDrops(false);
    ui->packDescription->viewport()->setAcceptDrops(false);

    ui->versionSelectionBox->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->versionSelectionBox->view()->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->versionSelectionBox->view()->parentWidget()->setMaximumHeight(300);

    for (int i = 0; i < filterModel->getAvailableSortings().size(); i++) {
        ui->sortByBox->addItem(filterModel->getAvailableSortings().keys().at(i));
    }
    ui->sortByBox->setCurrentText(filterModel->translateCurrentSorting());

    connect(ui->searchEdit, &QLineEdit::textChanged, this, &AtlPage::triggerSearch);
    connect(ui->sortByBox, &QComboBox::currentTextChanged, this, &AtlPage::onSortingSelectionChanged);
    connect(ui->packView->selectionModel(), &QItemSelectionModel::currentChanged, this, &AtlPage::onSelectionChanged);
    connect(ui->versionSelectionBox, &QComboBox::currentTextChanged, this, &AtlPage::onVersionSelectionChanged);

    ui->packView->installEventFilter(this);
    ui->packView->setLoading(true);

    // the empty details state: a centred muted placeholder, not a blank description area
    ModpackDetailsHeader::stylePlaceholder(ui->selectPackLabel);
    ui->detailsStack->setCurrentWidget(ui->emptyPage);
}

AtlPage::~AtlPage()
{
    delete ui;
}

bool AtlPage::shouldDisplay() const
{
    return true;
}

void AtlPage::retranslate()
{
    ui->retranslateUi(this);
}

bool AtlPage::eventFilter(QObject* watched, QEvent* event)
{
    // Enter on a selected pack installs it, like double clicking
    if (watched == ui->packView && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) && ui->packView->currentIndex().isValid()) {
            dialog->accept();
            keyEvent->accept();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void AtlPage::openedImpl()
{
    if (!initialized) {
        listModel->request();
        initialized = true;
    }

    suggestCurrent();
}

void AtlPage::suggestCurrent()
{
    if (!isOpened) {
        return;
    }

    if (selectedVersion.isEmpty()) {
        dialog->setSuggestedPack();
        return;
    }

    auto uiSupport = new AtlUserInteractionSupportImpl(this);
    dialog->setSuggestedPack(selected.name, selectedVersion, new ATLauncher::PackInstallTask(uiSupport, selected.name, selectedVersion));

    auto editedLogoName = "atl_" + selected.safeName;
    auto url = QString(BuildConfig.ATL_DOWNLOAD_SERVER_URL + "launcher/images/%1").arg(selected.safeName);
    listModel->getLogo(selected.safeName, url,
                       [this, editedLogoName](QString logo) { dialog->setSuggestedIconFromFile(logo, editedLogoName); });
}

void AtlPage::triggerSearch()
{
    filterModel->setSearchTerm(ui->searchEdit->text());
}

void AtlPage::onSortingSelectionChanged(QString sort)
{
    auto toSet = filterModel->getAvailableSortings().value(sort);
    filterModel->setSorting(toSet);
}

void AtlPage::onSelectionChanged(QModelIndex first, [[maybe_unused]] QModelIndex second)
{
    ui->versionSelectionBox->clear();

    if (!first.isValid()) {
        if (isOpened) {
            dialog->setSuggestedPack();
        }
        ui->packHeader->clearPack();
        ui->detailsStack->setCurrentWidget(ui->emptyPage);
        return;
    }

    QVariant raw = filterModel->data(first, Qt::UserRole);
    Q_ASSERT(raw.canConvert<ATLauncher::IndexedPack>());
    selected = raw.value<ATLauncher::IndexedPack>();

    ui->detailsStack->setCurrentWidget(ui->detailsPage);
    ui->packHeader->setPack(selected.name, QString(), tr("ATLauncher"));
    const auto logoUrl = QString(BuildConfig.ATL_DOWNLOAD_SERVER_URL + "launcher/images/%1").arg(selected.safeName);
    listModel->getLogo(selected.safeName, logoUrl, [this, name = selected.name](const QString& logo) {
        // late logo callback: only apply it while the same pack is still shown
        if (ui->packHeader->name() == name)
            ui->packHeader->setIcon(QIcon(logo));
    });
    ui->packDescription->setHtml(StringUtils::htmlListPatch(stripHtml(selected.description)));

    for (const auto& version : selected.versions) {
        ui->versionSelectionBox->addItem(version.version);
    }

    suggestCurrent();
}

void AtlPage::onVersionSelectionChanged(QString version)
{
    if (version.isNull() || version.isEmpty()) {
        selectedVersion = "";
        return;
    }

    selectedVersion = version;
    suggestCurrent();
}

void AtlPage::setSearchTerm(QString term)
{
    ui->searchEdit->setText(term);
}

QString AtlPage::getSerachTerm() const
{
    return ui->searchEdit->text();
}
