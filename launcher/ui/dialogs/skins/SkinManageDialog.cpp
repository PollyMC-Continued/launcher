// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (c) 2023-2024 Trial97 <alexandru.tripon97@gmail.com>
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

#include "SkinManageDialog.h"
#include "ui/dialogs/skins/draw/SkinOpenGLWindow.h"
#include "ui_SkinManageDialog.h"

#include <FileSystem.h>
#include <QAction>
#include <QDialog>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMimeDatabase>
#include <QPainter>
#include <QUrl>

#include "Application.h"
#include "settings/SettingsObject.h"
#include "DesktopServices.h"
#include "Json.h"
#include "QObjectPtr.h"

#include "minecraft/auth/Parsers.h"
#include "minecraft/skins/CapeChange.h"
#include "minecraft/skins/CraftySkinAPI.h"
#include "minecraft/skins/SkinDelete.h"
#include "minecraft/skins/SkinList.h"
#include "minecraft/skins/SkinModel.h"
#include "minecraft/skins/SkinUpload.h"

#include "net/Download.h"
#include "net/NetJob.h"
#include "tasks/Task.h"

#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/skins/CapeBrowserDialog.h"
#include "ui/dialogs/skins/SkinBrowserDialog.h"
#include "ui/instanceview/InstanceDelegate.h"

SkinManageDialog::SkinManageDialog(QWidget* parent, MinecraftAccountPtr acct)
    : QDialog(parent), m_acct(acct), m_ui(new Ui::SkinManageDialog), m_list(this, APPLICATION->settings()->get("SkinsDir").toString(), acct), m_capeApi(new Crafty::API(this))
{
    m_ui->setupUi(this);

    if (SkinOpenGLWindow::hasOpenGL()) {
        m_skinPreview = new SkinOpenGLWindow(this, palette().color(QPalette::Normal, QPalette::Base));
    } else {
        m_skinPreviewLabel = new QLabel(this);
        m_skinPreviewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    setWindowModality(Qt::WindowModal);

    auto contentsWidget = m_ui->listView;
    contentsWidget->setViewMode(QListView::IconMode);
    contentsWidget->setFlow(QListView::LeftToRight);
    contentsWidget->setIconSize(QSize(48, 48));
    contentsWidget->setMovement(QListView::Static);
    contentsWidget->setResizeMode(QListView::Adjust);
    contentsWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    contentsWidget->setSpacing(5);
    contentsWidget->setWordWrap(false);
    contentsWidget->setWrapping(true);
    contentsWidget->setUniformItemSizes(true);
    contentsWidget->setTextElideMode(Qt::ElideRight);
    contentsWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    contentsWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    contentsWidget->installEventFilter(this);
    contentsWidget->setItemDelegate(new ListViewDelegate(this));

    contentsWidget->setAcceptDrops(true);
    contentsWidget->setDropIndicatorShown(true);
    contentsWidget->viewport()->setAcceptDrops(true);
    contentsWidget->setDragDropMode(QAbstractItemView::DropOnly);
    contentsWidget->setDefaultDropAction(Qt::CopyAction);

    contentsWidget->installEventFilter(this);
    contentsWidget->setModel(&m_list);

    connect(contentsWidget, &QAbstractItemView::doubleClicked, this, &SkinManageDialog::activated);

    connect(contentsWidget->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SkinManageDialog::selectionChanged);
    connect(m_ui->listView, &QListView::customContextMenuRequested, this, &SkinManageDialog::show_context_menu);
    connect(m_ui->elytraCB, &QCheckBox::stateChanged, this, [this]() {
        if (m_skinPreview) {
            m_skinPreview->setElytraVisible(m_ui->elytraCB->isChecked());
        }
        on_capeCombo_currentIndexChanged(0);
    });

    setupCapes();

    if (m_acct->accountType() == AccountType::Offline) {
        // offline accounts keep their cape on disk instead of in the Mojang profile, so the
        // cape combo above (which lists the account's Mojang capes) is replaced by the
        // offline cape group for them
        m_ui->capeBox->hide();
        setupOfflineCape();
    } else {
        m_ui->offlineCapeBox->hide();
    }

    m_ui->listView->setCurrentIndex(m_list.index(m_list.getSelectedAccountSkin()));

    auto moreMenu = new QMenu(this);
    moreMenu->addAction(tr("Open skins folder"), this, &SkinManageDialog::openSkinsFolder);
    moreMenu->addAction(tr("Reset skin"), this, &SkinManageDialog::resetSkin);
    m_ui->moreBtn->setMenu(moreMenu);

    m_ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("OK"));

    if (m_skinPreview) {
        m_ui->skinLayout->insertWidget(0, QWidget::createWindowContainer(m_skinPreview, this));
    } else {
        m_ui->skinLayout->insertWidget(0, m_skinPreviewLabel);
    }
}

SkinManageDialog::~SkinManageDialog()
{
    // stop any cape download the URL import still has in flight
    m_capeApi->abortAll();
    delete m_ui;
    if (m_skinPreview) {
        delete m_skinPreview;
    }
}

void SkinManageDialog::activated(QModelIndex index)
{
    m_selectedSkinKey = index.data(Qt::UserRole).toString();
    accept();
}

void SkinManageDialog::selectionChanged(QItemSelection selected, [[maybe_unused]] QItemSelection deselected)
{
    if (selected.empty())
        return;

    QString key = selected.first().indexes().first().data(Qt::UserRole).toString();
    if (key.isEmpty())
        return;
    m_selectedSkinKey = key;
    auto skin = getSelectedSkin();
    if (!skin)
        return;

    if (m_skinPreview) {
        m_skinPreview->updateScene(skin);
    } else {
        m_skinPreviewLabel->setPixmap(
            QPixmap::fromImage(skin->getPreview()).scaled(m_skinPreviewLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
    }
    m_ui->capeCombo->setCurrentIndex(m_capesIdx.value(skin->getCapeId()));
    m_ui->steveBtn->setChecked(skin->getModel() == SkinModel::CLASSIC);
    m_ui->alexBtn->setChecked(skin->getModel() == SkinModel::SLIM);
}

void SkinManageDialog::delayed_scroll(QModelIndex model_index)
{
    auto contentsWidget = m_ui->listView;
    contentsWidget->scrollTo(model_index);
}

void SkinManageDialog::openSkinsFolder()
{
    DesktopServices::openPath(m_list.getDir(), true);
}

void SkinManageDialog::on_fileBtn_clicked()
{
    auto filter = QMimeDatabase().mimeTypeForName("image/png").filterString();
    QString raw_path = QFileDialog::getOpenFileName(this, tr("Select Skin Texture"), QString(), filter);
    if (raw_path.isNull()) {
        return;
    }
    auto message = m_list.installSkin(raw_path, {});
    if (!message.isEmpty()) {
        CustomMessageBox::selectable(this, tr("Selected file is not a valid skin"), message, QMessageBox::Critical)->show();
        return;
    }
}

void SkinManageDialog::on_viewOnlineBtn_clicked()
{
    SkinBrowserDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    auto choice = dialog.choice();
    if (choice.texturePath.isEmpty())
        return;

    auto name = choice.playerName.isEmpty() ? QStringLiteral("crafty-") + choice.hash.left(8) : choice.playerName;
    auto path = FS::PathCombine(m_list.getDir(), name + ".png");
    if (QFileInfo::exists(path))
        path = FS::PathCombine(m_list.getDir(), name + "-" + choice.hash.left(6) + ".png");
    if (!QFile::copy(choice.texturePath, path)) {
        CustomMessageBox::selectable(this, tr("Skin Import"), tr("Could not copy the skin into the skins folder."), QMessageBox::Warning)
            ->show();
        return;
    }

    SkinModel skin(path);
    if (!skin.isValid()) {
        QFile::remove(path);
        CustomMessageBox::selectable(this, tr("Skin Import"), tr("The downloaded image is not a valid skin."), QMessageBox::Warning)
            ->show();
        return;
    }
    skin.setModel(choice.model);
    skin.setURL(Crafty::API::pageUrl(choice.hash));
    m_list.updateSkin(&skin);
    m_selectedSkinKey = skin.name();
    // uploads the skin, or stores it locally for offline accounts
    accept();
}

QPixmap previewCape(QImage capeImage, bool elytra = false)
{
    if (capeImage.isNull())  // a corrupt or missing cape must not be painted on
        return {};
    if (elytra) {
        auto wing = capeImage.copy(34, 2, 12, 20);
        if (wing.isNull())  // cape too small to contain the elytra texture
            return {};
        QImage mirrored = wing.mirrored(true, false);

        QImage combined(wing.width() * 2 + 1, wing.height() + 14, capeImage.format());
        if (combined.isNull())  // allocation failed
            return {};
        combined.fill(Qt::transparent);

        QPainter painter(&combined);
        painter.drawImage(0, 7, wing);
        painter.drawImage(wing.width() + 1, 7, mirrored);
        painter.end();
        return QPixmap::fromImage(combined.scaled(84, 128, Qt::KeepAspectRatio, Qt::FastTransformation));
    }
    return QPixmap::fromImage(capeImage.copy(1, 1, 10, 16).scaled(80, 128, Qt::IgnoreAspectRatio, Qt::FastTransformation));
}

// file name for a cape library entry: letters, digits, dashes and underscores only
static QString safeCapeFileName(const QString& name)
{
    QString safe;
    for (const QChar ch : name) {
        if (ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_')) {
            safe += ch;
        } else if (!safe.isEmpty() && !safe.endsWith(QLatin1Char('-'))) {
            safe += QLatin1Char('-');
        }
    }
    while (safe.endsWith(QLatin1Char('-')))
        safe.chop(1);
    if (safe.isEmpty())
        safe = QStringLiteral("cape");
    return safe.left(64);
}

// the informational name stored next to the cape texture
static QString readCapeName(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return {};
    return doc.object().value(QStringLiteral("name")).toString();
}

void SkinManageDialog::setupCapes()
{
    // FIXME: add a model for this, download/refresh the capes on demand
    auto& accountData = *m_acct->accountData();
    int index = 0;
    m_ui->capeCombo->addItem(tr("No Cape"), QVariant());
    auto currentCape = accountData.minecraftProfile.currentCape;
    if (currentCape.isEmpty()) {
        m_ui->capeCombo->setCurrentIndex(index);
    }

    auto capesDir = FS::PathCombine(m_list.getDir(), "capes");
    NetJob::Ptr job{ new NetJob(tr("Download capes"), APPLICATION->network()) };
    bool needsToDownload = false;
    for (auto& cape : accountData.minecraftProfile.capes) {
        auto path = FS::PathCombine(capesDir, cape.id + ".png");
        if (cape.data.size()) {
            QImage capeImage;
            if (capeImage.loadFromData(cape.data, "PNG") && capeImage.save(path)) {
                m_capes[cape.id] = capeImage;
                continue;
            }
        }
        if (QFileInfo(path).exists()) {
            continue;
        }
        if (!cape.url.isEmpty()) {
            needsToDownload = true;
            job->addNetAction(Net::Download::makeFile(cape.url, path));
        }
    }
    if (needsToDownload) {
        ProgressDialog dlg(this);
        dlg.execWithTask(job.get());
    }
    for (auto& cape : accountData.minecraftProfile.capes) {
        index++;
        QImage capeImage;
        if (!m_capes.contains(cape.id)) {
            auto path = FS::PathCombine(capesDir, cape.id + ".png");
            if (QFileInfo(path).exists() && capeImage.load(path)) {
                m_capes[cape.id] = capeImage;
            }
        }
        if (!capeImage.isNull()) {
            m_ui->capeCombo->addItem(previewCape(capeImage, m_ui->elytraCB->isChecked()), cape.alias, cape.id);
        } else {
            m_ui->capeCombo->addItem(cape.alias, cape.id);
        }

        m_capesIdx[cape.id] = index;
    }
}

void SkinManageDialog::setupOfflineCape()
{
    updateOfflineCapeUi();
}

void SkinManageDialog::updateOfflineCapeUi()
{
    auto uuid = m_acct->profileId();
    auto capePath = FS::PathCombine(APPLICATION->dataRoot(), "skins", uuid, "cape.png");
    QImage cape(capePath);
    if (cape.isNull()) {
        m_ui->offlineCapeName->setText(tr("No cape"));
        m_ui->offlineCapeImage->clear();
        m_ui->offlineRemoveBtn->setEnabled(false);
        return;
    }
    auto name = readCapeName(FS::PathCombine(APPLICATION->dataRoot(), "skins", uuid, "cape.json"));
    m_ui->offlineCapeName->setText(name.isEmpty() ? tr("Cape") : name);
    auto pixmap = previewCape(cape, false);
    if (pixmap.isNull())
        m_ui->offlineCapeImage->clear();
    else
        m_ui->offlineCapeImage->setPixmap(pixmap.scaled(160, 160, Qt::KeepAspectRatio, Qt::FastTransformation));
    m_ui->offlineRemoveBtn->setEnabled(true);
}

void SkinManageDialog::applyOfflineCape(const QImage& cape, const QString& name)
{
    if (cape.isNull())  // a corrupt cape must never overwrite the one on disk
        return;
    auto uuid = m_acct->profileId();
    if (uuid.isEmpty())
        return;

    // library copy, shared between the accounts of this launcher
    auto libraryDir = FS::PathCombine(APPLICATION->dataRoot(), "capes");
    FS::ensureFolderPathExists(libraryDir);
    cape.save(FS::PathCombine(libraryDir, safeCapeFileName(name) + ".png"), "PNG");

    // the account-specific cape the offline skin agent serves, mirroring the skin.png/skin.json
    // pattern: cape.json only carries the informational name, the agent reads cape.png alone
    auto skinsDir = FS::PathCombine(APPLICATION->dataRoot(), "skins", uuid);
    if (!FS::ensureFolderPathExists(skinsDir)) {
        CustomMessageBox::selectable(this, tr("Cape Save"), tr("Failed to create skins directory!"), QMessageBox::Warning)->exec();
        return;
    }
    auto capePath = FS::PathCombine(skinsDir, "cape.png");
    QFile::remove(capePath);
    if (!cape.save(capePath, "PNG")) {
        CustomMessageBox::selectable(this, tr("Cape Save"), tr("Failed to save cape file!"), QMessageBox::Warning)->exec();
        return;
    }
    QJsonObject meta;
    meta["name"] = name;
    Json::write(meta, FS::PathCombine(skinsDir, "cape.json"));

    updateOfflineCapeUi();
}

void SkinManageDialog::on_offlineBrowseBtn_clicked()
{
    CapeBrowserDialog dialog(getSelectedSkin(), this);
    connect(&dialog, &CapeBrowserDialog::selected, this, [this](const QString& player, const Crafty::CapePtr& cape) {
        if (!cape || cape->texture.isNull())
            return;
        auto name = cape->name;
        if (name.isEmpty())
            name = player.isEmpty() ? tr("Custom cape") : tr("%1's cape").arg(player);
        applyOfflineCape(cape->texture, name);
    });
    dialog.exec();
}

void SkinManageDialog::on_offlineFileBtn_clicked()
{
    auto filter = QMimeDatabase().mimeTypeForName("image/png").filterString();
    QString raw_path = QFileDialog::getOpenFileName(this, tr("Select Cape Texture"), QString(), filter);
    if (raw_path.isNull()) {
        return;
    }
    QImage cape(raw_path);
    if (cape.isNull()) {
        CustomMessageBox::selectable(this, tr("Cape Import"), tr("The selected file is not a valid image."), QMessageBox::Critical)->show();
        return;
    }
    applyOfflineCape(cape, QFileInfo(raw_path).completeBaseName());
}

void SkinManageDialog::on_offlineUrlBtn_clicked()
{
    bool ok = false;
    auto urlText = QInputDialog::getText(this, tr("Import cape from URL"), tr("URL of a cape PNG image:"), QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        return;
    }
    urlText = urlText.trimmed();
    if (urlText.isEmpty()) {
        return;
    }
    auto url = QUrl::fromUserInput(urlText);
    if (!url.isValid() || (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http"))) {
        CustomMessageBox::selectable(this, tr("Cape Import"), tr("Invalid URL"), QMessageBox::Critical)->show();
        return;
    }

    m_ui->offlineUrlBtn->setEnabled(false);
    m_capeApi->fetchCapeFromUrl(url, [this](const QList<Crafty::CapePtr>& capes, const QString& error) {
        m_ui->offlineUrlBtn->setEnabled(true);
        if (capes.isEmpty()) {
            CustomMessageBox::selectable(this, tr("Cape Import"),
                                          error.isEmpty() ? tr("The URL did not contain a valid cape image.")
                                                          : tr("Unable to download the cape: %1").arg(error),
                                          QMessageBox::Critical)
                ->show();
            return;
        }
        applyOfflineCape(capes.first()->texture, tr("Custom cape"));
    });
}

void SkinManageDialog::on_offlineRemoveBtn_clicked()
{
    // only the account's own cape goes away, the library copy is kept
    auto uuid = m_acct->profileId();
    auto skinsDir = FS::PathCombine(APPLICATION->dataRoot(), "skins", uuid);
    QFile::remove(FS::PathCombine(skinsDir, "cape.png"));
    QFile::remove(FS::PathCombine(skinsDir, "cape.json"));
    updateOfflineCapeUi();
}

void SkinManageDialog::on_capeCombo_currentIndexChanged(int index)
{
    auto id = m_ui->capeCombo->currentData();
    auto cape = m_capes.value(id.toString(), {});
    if (!cape.isNull()) {
        m_ui->capeImage->setPixmap(
            previewCape(cape, m_ui->elytraCB->isChecked()).scaled(size() * (1. / 3), Qt::KeepAspectRatio, Qt::FastTransformation));
    } else {
        m_ui->capeImage->clear();
    }
    if (m_skinPreview) {
        m_skinPreview->updateCape(cape);
    }
    if (auto skin = getSelectedSkin(); skin) {
        skin->setCapeId(id.toString());
        if (m_skinPreview) {
            m_skinPreview->updateScene(skin);
        } else {
            m_skinPreviewLabel->setPixmap(
                QPixmap::fromImage(skin->getPreview()).scaled(m_skinPreviewLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
        }
    }
}

void SkinManageDialog::on_steveBtn_toggled(bool checked)
{
    if (auto skin = getSelectedSkin(); skin) {
        skin->setModel(checked ? SkinModel::CLASSIC : SkinModel::SLIM);
        if (m_skinPreview) {
            m_skinPreview->updateScene(skin);
        } else {
            m_skinPreviewLabel->setPixmap(
                QPixmap::fromImage(skin->getPreview()).scaled(m_skinPreviewLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
        }
    }
}

void SkinManageDialog::accept()
{
    auto skin = m_list.skin(m_selectedSkinKey);
    if (!skin) {
        reject();
        return;
    }
    auto path = skin->getPath();

    if (!QFile::exists(path)) {
        CustomMessageBox::selectable(this, tr("Skin Upload"), tr("Skin file does not exist!"), QMessageBox::Warning)->exec();
        reject();
        return;
    }

    // For offline accounts, save skin locally instead of uploading to Mojang
    if (m_acct->accountType() == AccountType::Offline) {
        auto uuid = m_acct->profileId();
        auto skinsDir = FS::PathCombine(APPLICATION->dataRoot(), "skins", uuid);

        if (!FS::ensureFolderPathExists(skinsDir)) {
            CustomMessageBox::selectable(this, tr("Skin Save"), tr("Failed to create skins directory!"), QMessageBox::Warning)->exec();
            reject();
            return;
        }

        auto targetPath = FS::PathCombine(skinsDir, "skin.png");
        QFile::remove(targetPath);
        if (!QFile::copy(path, targetPath)) {
            CustomMessageBox::selectable(this, tr("Skin Save"), tr("Failed to copy skin file!"), QMessageBox::Warning)->exec();
            reject();
            return;
        }

        // Save skin metadata
        QJsonObject meta;
        meta["model"] = skin->getModelString();
        meta["uuid"] = uuid;
        Json::write(meta, FS::PathCombine(skinsDir, "skin.json"));

        // Note: offline skins only work in singleplayer, not multiplayer
        CustomMessageBox::selectable(this, tr("Skin Saved"),
            tr("Skin saved for singleplayer. Note: other players in multiplayer will not see your skin unless the server has CustomSkinLoader installed."),
            QMessageBox::Information)->exec();

        QDialog::accept();
        return;
    }

    ProgressDialog prog(this);

    // the 24 hour launcher token may already be expired, refresh it before building the upload
    if (m_acct->shouldRefresh() && prog.execWithTask(m_acct->refresh().get()) != QDialog::Accepted) {
        CustomMessageBox::selectable(this, tr("Skin Upload"), tr("Could not refresh the account login."), QMessageBox::Warning)->exec();
        reject();
        return;
    }

    NetJob::Ptr skinUpload{ new NetJob(tr("Change skin"), APPLICATION->network(), 1) };

    skinUpload->addNetAction(SkinUpload::make(m_acct->accessToken(), skin->getPath(), skin->getModelString()));

    auto selectedCape = skin->getCapeId();
    if (selectedCape != m_acct->accountData()->minecraftProfile.currentCape) {
        skinUpload->addNetAction(CapeChange::make(m_acct->accessToken(), selectedCape));
    }

    skinUpload->addTask(m_acct->refresh().staticCast<Task>());
    if (prog.execWithTask(skinUpload.get()) != QDialog::Accepted) {
        CustomMessageBox::selectable(this, tr("Skin Upload"), tr("Failed to upload skin!"), QMessageBox::Warning)->exec();
        reject();
        return;
    }
    skin->setURL(m_acct->accountData()->minecraftProfile.skin.url);
    QDialog::accept();
}

void SkinManageDialog::resetSkin()
{
    // For offline accounts, just delete local skin files
    if (m_acct->accountType() == AccountType::Offline) {
        auto uuid = m_acct->profileId();
        auto skinsDir = FS::PathCombine(APPLICATION->dataRoot(), "skins", uuid);
        QFile::remove(FS::PathCombine(skinsDir, "skin.png"));
        QFile::remove(FS::PathCombine(skinsDir, "skin.json"));
        QDialog::accept();
        return;
    }

    ProgressDialog prog(this);
    NetJob::Ptr skinReset{ new NetJob(tr("Reset skin"), APPLICATION->network(), 1) };
    skinReset->addNetAction(SkinDelete::make(m_acct->accessToken()));
    skinReset->addTask(m_acct->refresh().staticCast<Task>());
    if (prog.execWithTask(skinReset.get()) != QDialog::Accepted) {
        CustomMessageBox::selectable(this, tr("Skin Delete"), tr("Failed to delete current skin!"), QMessageBox::Warning)->exec();
        reject();
        return;
    }
    QDialog::accept();
}

void SkinManageDialog::show_context_menu(const QPoint& pos)
{
    QMenu myMenu(tr("Context menu"), this);
    myMenu.addAction(m_ui->action_Rename_Skin);
    myMenu.addAction(m_ui->action_Delete_Skin);

    myMenu.exec(m_ui->listView->mapToGlobal(pos));
}

bool SkinManageDialog::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == m_ui->listView) {
        if (ev->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(ev);
            switch (keyEvent->key()) {
                case Qt::Key_Delete:
                    on_action_Delete_Skin_triggered(false);
                    return true;
                case Qt::Key_F2:
                    on_action_Rename_Skin_triggered(false);
                    return true;
                default:
                    break;
            }
        }
    }
    return QDialog::eventFilter(obj, ev);
}

void SkinManageDialog::on_action_Rename_Skin_triggered(bool)
{
    if (!m_selectedSkinKey.isEmpty()) {
        m_ui->listView->edit(m_ui->listView->currentIndex());
    }
}

void SkinManageDialog::on_action_Delete_Skin_triggered(bool)
{
    if (m_selectedSkinKey.isEmpty())
        return;

    if (m_list.getSkinIndex(m_selectedSkinKey) == m_list.getSelectedAccountSkin()) {
        CustomMessageBox::selectable(this, tr("Delete error"), tr("Can not delete skin that is in use."), QMessageBox::Warning)->exec();
        return;
    }

    auto skin = m_list.skin(m_selectedSkinKey);
    if (!skin)
        return;

    auto response = CustomMessageBox::selectable(this, tr("Confirm Deletion"),
                                                 tr("You are about to delete \"%1\".\n"
                                                    "Are you sure?")
                                                     .arg(skin->name()),
                                                 QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                        ->exec();

    if (response == QMessageBox::Yes) {
        if (!m_list.deleteSkin(m_selectedSkinKey, true)) {
            m_list.deleteSkin(m_selectedSkinKey, false);
        }
    }
}

void SkinManageDialog::importFromUrl()
{
    auto url = QUrl(m_ui->urlLine->text());
    if (!url.isValid()) {
        CustomMessageBox::selectable(this, tr("Invalid url"), tr("Invalid url"), QMessageBox::Critical)->show();
        return;
    }

    NetJob::Ptr job{ new NetJob(tr("Download skin"), APPLICATION->network()) };
    job->setAskRetry(false);

    auto path = FS::PathCombine(m_list.getDir(), url.fileName());
    job->addNetAction(Net::Download::makeFile(url, path));
    ProgressDialog dlg(this);
    dlg.execWithTask(job.get());
    SkinModel s(path);
    if (!s.isValid()) {
        CustomMessageBox::selectable(this, tr("URL is not a valid skin"),
                                     QFileInfo::exists(path) ? tr("Skin images must be 64x64 or 64x32 pixel PNG files.")
                                                             : tr("Unable to download the skin: '%1'.").arg(m_ui->urlLine->text()),
                                     QMessageBox::Critical)
            ->show();
        QFile::remove(path);
        return;
    }
    m_ui->urlLine->setText("");
    if (QFileInfo(path).suffix().isEmpty()) {
        QFile::rename(path, path + ".png");
    }
}

class WaitTask : public Task {
   public:
    WaitTask() : m_loop(), m_done(false) {};
    virtual ~WaitTask() = default;

   public slots:
    void quit()
    {
        m_done = true;
        m_loop.quit();
    }

   protected:
    virtual void executeTask()
    {
        if (!m_done)
            m_loop.exec();
        emitSucceeded();
    };

   private:
    QEventLoop m_loop;
    bool m_done;
};

void SkinManageDialog::on_importBtn_clicked()
{
    const auto text = m_ui->urlLine->text().trimmed();
    if (text.isEmpty())
        return;
    m_ui->urlLine->setText(text);
    if (text.startsWith("http://", Qt::CaseInsensitive) || text.startsWith("https://", Qt::CaseInsensitive))
        importFromUrl();
    else
        importFromUser();
}

void SkinManageDialog::importFromUser()
{
    auto user = m_ui->urlLine->text();
    if (user.isEmpty()) {
        return;
    }
    MinecraftProfile mcProfile;
    auto path = FS::PathCombine(m_list.getDir(), user + ".png");

    NetJob::Ptr job{ new NetJob(tr("Download user skin"), APPLICATION->network(), 1) };
    job->setAskRetry(false);

    auto uuidLoop = makeShared<WaitTask>();
    auto profileLoop = makeShared<WaitTask>();

    auto [getUUID, uuidOut] = Net::Download::makeByteArray("https://api.minecraftservices.com/minecraft/profile/lookup/name/" + user);
    auto [getProfile, profileOut] = Net::Download::makeByteArray(QUrl());
    auto downloadSkin = Net::Download::makeFile(QUrl(), path);

    QString failReason;

    connect(getUUID.get(), &Task::aborted, uuidLoop.get(), &WaitTask::quit);
    connect(getUUID.get(), &Task::failed, this, [&failReason](QString reason) {
        qCritical() << "Couldn't get user UUID:" << reason;
        failReason = tr("failed to get user UUID");
    });
    connect(getUUID.get(), &Task::failed, uuidLoop.get(), &WaitTask::quit);
    connect(getProfile.get(), &Task::aborted, profileLoop.get(), &WaitTask::quit);
    connect(getProfile.get(), &Task::failed, profileLoop.get(), &WaitTask::quit);
    connect(getProfile.get(), &Task::failed, this, [&failReason](QString reason) {
        qCritical() << "Couldn't get user profile:" << reason;
        failReason = tr("failed to get user profile");
    });
    connect(downloadSkin.get(), &Task::failed, this, [&failReason](QString reason) {
        qCritical() << "Couldn't download skin:" << reason;
        failReason = tr("failed to download skin");
    });

    connect(getUUID.get(), &Task::succeeded, this, [uuidLoop, uuidOut, job, getProfile, &failReason] {
        try {
            QJsonParseError parse_error{};
            QJsonDocument doc = QJsonDocument::fromJson(*uuidOut, &parse_error);
            if (parse_error.error != QJsonParseError::NoError) {
                qWarning() << "Error while parsing JSON response from Minecraft skin service at" << parse_error.offset
                           << "reason:" << parse_error.errorString();
                failReason = tr("failed to parse get user UUID response");
                uuidLoop->quit();
                return;
            }
            const auto root = doc.object();
            auto id = root["id"].toString();
            if (!id.isEmpty()) {
                getProfile->setUrl("https://sessionserver.mojang.com/session/minecraft/profile/" + id);
            } else {
                failReason = tr("user id is empty");
                job->abort();
            }
        } catch (const Exception& e) {
            qCritical() << "Couldn't load skin json:" << e.cause();
            failReason = tr("failed to parse get user UUID response");
        }
        uuidLoop->quit();
    });

    connect(getProfile.get(), &Task::succeeded, this, [profileLoop, profileOut, job, getProfile, &mcProfile, downloadSkin, &failReason] {
        if (Parsers::parseMinecraftProfileMojang(*profileOut, mcProfile)) {
            downloadSkin->setUrl(mcProfile.skin.url);
        } else {
            failReason = tr("failed to parse get user profile response");
            job->abort();
        }
        profileLoop->quit();
    });

    job->addNetAction(getUUID);
    job->addTask(uuidLoop);
    job->addNetAction(getProfile);
    job->addTask(profileLoop);
    job->addNetAction(downloadSkin);
    ProgressDialog dlg(this);
    dlg.execWithTask(job.get());

    SkinModel s(path);
    if (!s.isValid()) {
        if (failReason.isEmpty()) {
            failReason = tr("the skin is invalid");
        }
        CustomMessageBox::selectable(this, tr("Username not found"),
                                     tr("Unable to find the skin for '%1'\n because: %2.").arg(user, failReason), QMessageBox::Critical)
            ->show();
        QFile::remove(path);
        return;
    }
    m_ui->urlLine->setText("");
    s.setModel(mcProfile.skin.variant.toUpper() == "SLIM" ? SkinModel::SLIM : SkinModel::CLASSIC);
    s.setURL(mcProfile.skin.url);
    if (m_capes.contains(mcProfile.currentCape)) {
        s.setCapeId(mcProfile.currentCape);
    }
    m_list.updateSkin(&s);
}

void SkinManageDialog::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    QSize s = size() * (1. / 3);

    auto id = m_ui->capeCombo->currentData();
    auto cape = m_capes.value(id.toString(), {});
    if (!cape.isNull()) {
        m_ui->capeImage->setPixmap(previewCape(cape, m_ui->elytraCB->isChecked()).scaled(s, Qt::KeepAspectRatio, Qt::FastTransformation));
    } else {
        m_ui->capeImage->clear();
    }
    if (auto skin = getSelectedSkin(); skin && !m_skinPreview) {
        m_skinPreviewLabel->setPixmap(
            QPixmap::fromImage(skin->getPreview()).scaled(m_skinPreviewLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
    }
}

SkinModel* SkinManageDialog::getSelectedSkin()
{
    if (auto skin = m_list.skin(m_selectedSkinKey); skin && skin->isValid()) {
        return skin;
    }
    return nullptr;
}

QHash<QString, QImage> SkinManageDialog::capes()
{
    return m_capes;
}
