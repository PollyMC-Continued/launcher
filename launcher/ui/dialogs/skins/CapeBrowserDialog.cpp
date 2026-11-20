// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (C) 2026 PollyMC Continued Contributors
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

#include "CapeBrowserDialog.h"

#include <QCheckBox>
#include <QColor>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QShowEvent>
#include <QSplitter>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "Application.h"
#include "DiscordRichPresence.h"
#include "FileSystem.h"
#include "settings/SettingsObject.h"
#include "ui/dialogs/CustomMessageBox.h"

static const QLatin1String geometryKey("CapeBrowserGeometry");
static constexpr int kPlayersPerPage = 12;

// Vanilla capes are 2:1 (64x32, 512x256, ...) and the 3D preview maps its UVs onto that. OptiFine
// capes are cropped to the used 46x22 area (and sometimes land a pixel or two off at HD sizes), so
// they are pasted onto a transparent 2:1 canvas first, at the same pixel scale.
static QImage normalizedCape(const QImage& cape)
{
    if (cape.isNull() || cape.width() == cape.height() * 2)
        return cape;
    const double ratio = double(cape.width()) / cape.height();
    if (ratio < 2.05 || ratio > 2.2)
        return cape;  // not a shape we know, hand it over untouched
    const double unit = cape.width() / 46.0;
    QImage canvas(qRound(unit * 64), qRound(unit * 32), QImage::Format_ARGB32);
    if (canvas.isNull())
        return cape;
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    painter.drawImage(0, 0, cape);
    painter.end();
    return canvas;
}

CapeBrowserDialog::CapeBrowserDialog(const SkinModel* currentSkin, QWidget* parent) : QDialog(parent), m_api(new Crafty::API(this))
{
    setWindowModality(Qt::WindowModal);
    setWindowTitle(tr("Browse capes"));

    if (currentSkin && currentSkin->isValid()) {
        m_skin = std::make_unique<SkinModel>(currentSkin->getPath());
        m_skin->setModel(currentSkin->getModel());
    }
    if (!m_skin)
        setupFallbackSkin();  // nothing selected in the skin manager: a plain mannequin instead of a black model

    auto geometry = APPLICATION->settings()->get(geometryKey).toString();
    if (geometry.isEmpty()) {
        const auto base = parent ? parent->size() : QSize(1024, 768);
        resize(qMax(qRound(base.width() * 0.6), 700), qMax(qRound(base.height() * 0.75), 480));
    } else {
        restoreGeometry(QByteArray::fromBase64(geometry.toUtf8()));
    }

    auto rootLayout = new QVBoxLayout(this);

    // one compact row: look a player up, or paste a PNG link
    auto topRow = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search by player name..."));
    m_searchEdit->setToolTip(tr("crafty.gg does not publish a cape catalog, so capes are looked up from a player's profile "
                                "where it tracks Mojang and OptiFine capes."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchBtn = new QPushButton(tr("Search"), this);
    m_importBtn = new QPushButton(tr("Import URL..."), this);
    m_importBtn->setToolTip(tr("Import a cape from a PNG link. Cape textures are typically 64x32, 512x256 or OptiFine 46x22 style PNG files."));
    topRow->addWidget(m_searchEdit, 1);
    topRow->addWidget(m_searchBtn);
    topRow->addSpacing(12);
    topRow->addWidget(m_importBtn);
    rootLayout->addLayout(topRow);

    auto splitter = new QSplitter(Qt::Horizontal, this);

    m_capeList = new QListWidget(this);
    m_capeList->setViewMode(QListView::IconMode);
    m_capeList->setMovement(QListView::Static);
    m_capeList->setResizeMode(QListView::Adjust);
    m_capeList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_capeList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_capeList->setIconSize(QSize(60, 96));
    m_capeList->setSpacing(8);
    m_capeList->setWordWrap(true);
    m_capeList->setTextElideMode(Qt::ElideRight);
    m_capeList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    auto detail = new QWidget(this);
    auto detailLayout = new QVBoxLayout(detail);
    detailLayout->setContentsMargins(0, 0, 0, 0);

    if (SkinOpenGLWindow::hasOpenGL()) {
        m_preview = new SkinOpenGLWindow(this, palette().color(QPalette::Normal, QPalette::Base));
        detailLayout->addWidget(QWidget::createWindowContainer(m_preview, this), 1);
    } else {
        m_previewLabel = new QLabel(this);
        m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_previewLabel->setAlignment(Qt::AlignCenter);
        detailLayout->addWidget(m_previewLabel, 1);
    }

    m_nameLabel = new QLabel(tr("Select a cape"), this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_sourceLabel = new QLabel(this);
    m_sourceLabel->setAlignment(Qt::AlignCenter);
    m_sourceLabel->setWordWrap(true);
    m_sourceLabel->setOpenExternalLinks(true);
    m_elytraCB = new QCheckBox(tr("Preview Elytra"), this);
    detailLayout->addWidget(m_nameLabel);
    detailLayout->addWidget(m_sourceLabel);
    detailLayout->addWidget(m_elytraCB, 0, Qt::AlignHCenter);

    splitter->addWidget(m_capeList);
    splitter->addWidget(detail);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({ 380, 520 });
    rootLayout->addWidget(splitter, 1);

    auto bottomRow = new QHBoxLayout();
    m_closeBtn = new QPushButton(tr("Close"), this);
    bottomRow->addWidget(m_closeBtn);
    bottomRow->addStretch(1);
    m_statusLabel = new QLabel(tr("Finding capes..."), this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    bottomRow->addWidget(m_statusLabel, 3);
    bottomRow->addStretch(1);
    m_moreBtn = new QPushButton(tr("Load more"), this);
    m_moreBtn->setEnabled(false);
    m_moreBtn->setToolTip(tr("Look through more players for capes"));
    bottomRow->addWidget(m_moreBtn);
    m_useBtn = new QPushButton(tr("Use cape"), this);
    m_useBtn->setEnabled(false);
    m_useBtn->setToolTip(tr("Apply this cape to the selected account"));
    bottomRow->addWidget(m_useBtn);
    rootLayout->addLayout(bottomRow);

    connect(m_searchBtn, &QPushButton::clicked, this, &CapeBrowserDialog::search);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &CapeBrowserDialog::search);
    connect(m_importBtn, &QPushButton::clicked, this, &CapeBrowserDialog::importUrl);
    connect(m_capeList, &QListWidget::currentRowChanged, this, &CapeBrowserDialog::currentRowChanged);
    connect(m_capeList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { useCape(); });
    connect(m_elytraCB, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_preview)
            m_preview->setElytraVisible(checked);
        else
            updateDetails();
    });
    connect(m_useBtn, &QPushButton::clicked, this, &CapeBrowserDialog::useCape);
    connect(m_moreBtn, &QPushButton::clicked, this, &CapeBrowserDialog::loadMore);
    connect(m_closeBtn, &QPushButton::clicked, this, &CapeBrowserDialog::reject);

    discover(1);
}

CapeBrowserDialog::~CapeBrowserDialog()
{
    // invalidates every callback that is still in flight
    m_request++;
    m_api->abortAll();
}

QHash<QString, QImage> CapeBrowserDialog::capes()
{
    // asked by the preview when it first initializes: hand it the cape that is already picked
    if (!m_skin || !m_current || m_current->texture.isNull())
        return {};
    return { { m_skin->getCapeId(), normalizedCape(m_current->texture) } };
}

void CapeBrowserDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    DiscordRichPresence::instance()->updateBrowsing(tr("Looking for capes"));
}

void CapeBrowserDialog::hideEvent(QHideEvent* event)
{
    QDialog::hideEvent(event);
    DiscordRichPresence::instance()->browsingClosed();
}

void CapeBrowserDialog::accept()
{
    APPLICATION->settings()->set(geometryKey, QString::fromUtf8(saveGeometry().toBase64()));
    QDialog::accept();
}

void CapeBrowserDialog::reject()
{
    APPLICATION->settings()->set(geometryKey, QString::fromUtf8(saveGeometry().toBase64()));
    QDialog::reject();
}

void CapeBrowserDialog::showError(const QString& reason)
{
    CustomMessageBox::selectable(this, tr("Cape lookup"), reason, QMessageBox::Warning)->exec();
}

void CapeBrowserDialog::setLoading(bool loading)
{
    m_searchBtn->setEnabled(!loading);
    m_importBtn->setEnabled(!loading);
}

void CapeBrowserDialog::search()
{
    auto name = m_searchEdit->text().trimmed();
    if (name.isEmpty()) {
        // an empty search goes back to browsing, like the skin browser does
        m_capes.clear();
        m_scanned.clear();
        m_capeKeys.clear();
        m_player.clear();
        fillList();
        discover(1);
        return;
    }
    m_player = name;
    m_discoverMore = false;
    m_moreBtn->hide();

    const int request = ++m_request;
    setLoading(true);
    m_statusLabel->setText(tr("Looking up %1 on crafty.gg...").arg(name));

    m_api->fetchPlayerCapes(name, [this, request, name](const QList<Crafty::CapePtr>& capes, const QString& message) {
        if (request != m_request)
            return;
        setLoading(false);
        m_capes = capes;
        fillList();

        if (!message.isEmpty() && message != Crafty::API::noCapesMessage()) {
            m_statusLabel->setText(message);
            showError(message);
        } else if (capes.isEmpty()) {
            m_statusLabel->setText(tr("No capes found for %1 on crafty.gg.").arg(name));
        } else {
            m_statusLabel->setText(tr("Found %1 cape(s) for %2.").arg(capes.size()).arg(name));
        }
    });
}

void CapeBrowserDialog::importUrl()
{
    bool ok = false;
    auto urlText = QInputDialog::getText(this, tr("Import cape from URL"), tr("URL of a cape PNG image:"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || urlText.isEmpty())
        return;
    auto url = QUrl::fromUserInput(urlText);
    if (!url.isValid() || (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http"))) {
        showError(tr("%1 is not a valid http(s) URL.").arg(urlText));
        return;
    }

    const int request = ++m_request;
    m_discoverMore = false;
    m_moreBtn->hide();
    setLoading(true);
    m_statusLabel->setText(tr("Downloading the cape..."));

    m_api->fetchCapeFromUrl(url, [this, request](const QList<Crafty::CapePtr>& capes, const QString& message) {
        if (request != m_request)
            return;
        setLoading(false);
        if (capes.isEmpty()) {
            auto reason = message.isEmpty() ? tr("The URL did not contain a valid cape image.") : message;
            m_statusLabel->setText(reason);
            showError(reason);
            return;
        }
        // the imported cape is offered as the whole result list, ready for "Use cape"
        m_capes = capes;
        fillList();
        m_statusLabel->setText(tr("Cape imported from URL."));
    });
}

QString CapeBrowserDialog::displayName(const Crafty::CapePtr& cape) const
{
    const auto player = cape->username.isEmpty() ? m_player : cape->username;
    auto title = cape->name;
    if (title.isEmpty())
        title = player.isEmpty() ? tr("Cape") : tr("%1's cape").arg(player);
    return title;
}

// the visible face of the cape (10x16 units at 1,1 of the vanilla layout), enlarged without smoothing
QPixmap CapeBrowserDialog::thumbnail(const Crafty::CapePtr& cape) const
{
    const auto image = normalizedCape(cape->texture);
    if (image.isNull())
        return {};
    const double unit = image.width() / 64.0;
    const auto face = image.copy(qRound(unit), qRound(unit), qRound(unit * 10), qRound(unit * 16));
    if (face.isNull())
        return {};
    return QPixmap::fromImage(face.scaled(60, 96, Qt::IgnoreAspectRatio, Qt::FastTransformation));
}

void CapeBrowserDialog::appendItem(const Crafty::CapePtr& cape)
{
    auto item = new QListWidgetItem(m_capeList);
    item->setText(displayName(cape));
    item->setTextAlignment(Qt::AlignHCenter);
    if (auto pixmap = thumbnail(cape); !pixmap.isNull())
        item->setIcon(QIcon(pixmap));
    item->setToolTip(cape->textureUrl.isEmpty() ? tr("Source: %1").arg(cape->source)
                                                : tr("Source: %1\n%2").arg(cape->source, cape->textureUrl.toString()));
}

void CapeBrowserDialog::fillList()
{
    m_capeList->clear();
    m_current.clear();

    for (const auto& cape : m_capes)
        appendItem(cape);
    if (!m_capes.isEmpty())
        m_capeList->setCurrentRow(0);
    updateDetails();
}

void CapeBrowserDialog::currentRowChanged(int row)
{
    if (row < 0 || row >= m_capes.size())
        m_current.clear();
    else
        m_current = m_capes.at(row);
    updateDetails();
}

void CapeBrowserDialog::updateDetails()
{
    const bool hasCape = m_current && !m_current->texture.isNull();
    m_useBtn->setEnabled(hasCape);

    if (!m_current) {
        m_nameLabel->setText(tr("Select a cape"));
        m_sourceLabel->clear();
    } else {
        m_nameLabel->setText(displayName(m_current));
        QString source = tr("Source: %1").arg(m_current->source);
        const auto player = m_current->username.isEmpty() ? m_player : m_current->username;
        if (!player.isEmpty()) {
            source += QStringLiteral(" · <a href=\"https://crafty.gg/@%1\">%2</a>")
                          .arg(QString::fromUtf8(QUrl::toPercentEncoding(player)), tr("Open crafty.gg page"));
        }
        m_sourceLabel->setText(source);
    }

    // the cape goes onto the skin that is selected in the skin manager
    const QImage cape = hasCape ? normalizedCape(m_current->texture) : QImage();
    if (m_preview) {
        m_preview->updateCape(cape);
        m_preview->setElytraVisible(m_elytraCB->isChecked());
    } else if (m_previewLabel) {
        if (!cape.isNull()) {
            const auto face = cape.copy(cape.width() / 64, cape.width() / 64, cape.width() * 10 / 64, cape.width() * 16 / 64);
            m_previewLabel->setPixmap(QPixmap::fromImage(face.scaled(160, 256, Qt::IgnoreAspectRatio, Qt::FastTransformation)));
        } else {
            m_previewLabel->clear();
        }
    }
}

void CapeBrowserDialog::setupFallbackSkin()
{
    const auto path = FS::PathCombine(APPLICATION->dataRoot(), "cache", "capes", "preview-mannequin.png");
    if (!QFileInfo::exists(path)) {
        QImage image(64, 64, QImage::Format_ARGB32);
        image.fill(QColor(160, 160, 160));
        FS::ensureFolderPathExists(QFileInfo(path).absolutePath());
        if (!image.save(path, "PNG"))
            return;
    }
    auto skin = std::make_unique<SkinModel>(path);
    if (skin->isValid())
        m_skin = std::move(skin);
}

// crafty.gg has no cape catalog, so browsing means taking the players of the skin catalog and asking
// for the capes each one has. At most a dozen players per page keeps this well inside the request
// budget Crafty::API shares between all of its calls, and "Load more" continues with the next page.
void CapeBrowserDialog::discover(int page)
{
    const int request = ++m_request;
    m_moreBtn->show();
    m_moreBtn->setEnabled(false);
    m_statusLabel->setText(tr("Finding capes..."));

    m_api->fetchSkins(page, QString(), [this, request](const Crafty::SkinPage& result, const QString& error) {
        if (request != m_request)
            return;
        if (!error.isEmpty()) {
            finishDiscovery(error);
            return;
        }
        m_discoverPage = result.page;
        m_discoverMore = result.hasMore;

        QStringList names;
        for (const auto& skin : result.skins) {
            if (names.size() >= kPlayersPerPage)
                break;
            const auto name = skin->username.trimmed();
            if (name.isEmpty() || m_scanned.contains(name.toLower()))
                continue;
            m_scanned.insert(name.toLower());
            names << name;
        }
        lookupNext(request, names, 0);
    });
}

void CapeBrowserDialog::lookupNext(int request, const QStringList& names, int index)
{
    if (request != m_request)
        return;
    if (index >= names.size()) {
        finishDiscovery(QString());
        return;
    }
    m_statusLabel->setText(tr("Looking for capes... (%1 of %2 players)").arg(index + 1).arg(names.size()));

    m_api->fetchPlayerCapes(names.at(index), [this, request, names, index](const QList<Crafty::CapePtr>& capes, const QString&) {
        if (request != m_request)
            return;
        for (const auto& cape : capes)
            addCape(cape);
        // players without capes and failed lookups are simply skipped. The next lookup starts from the
        // event loop, not from inside the callback of the one that just finished
        QTimer::singleShot(0, this, [this, request, names, index] { lookupNext(request, names, index + 1); });
    });
}

void CapeBrowserDialog::finishDiscovery(const QString& error)
{
    m_moreBtn->setEnabled(m_discoverMore);
    if (m_capes.isEmpty()) {
        m_statusLabel->setText(error.isEmpty() ? tr("No capes found yet. Try Load more, or search for a player.") : error);
    } else {
        m_statusLabel->setText(tr("%1 capes found. Load more looks through more players, or search for one.").arg(m_capes.size()));
    }
}

// adds one cape to the grid without disturbing the selection; a cape that is already there (several
// players wear the same Mojang cape) is skipped
bool CapeBrowserDialog::addCape(const Crafty::CapePtr& cape)
{
    if (!cape || cape->texture.isNull())
        return false;
    const auto key = cape->cachePath.isEmpty() ? cape->textureUrl.toString() : cape->cachePath;
    if (!key.isEmpty()) {
        if (m_capeKeys.contains(key))
            return false;
        m_capeKeys.insert(key);
    }
    m_capes << cape;
    appendItem(cape);
    if (m_capeList->currentRow() < 0)
        m_capeList->setCurrentRow(0);
    return true;
}

void CapeBrowserDialog::loadMore()
{
    discover(m_discoverPage + 1);
}

void CapeBrowserDialog::useCape()
{
    if (!m_current || m_current->texture.isNull())
        return;
    emit selected(m_current->username.isEmpty() ? m_player : m_current->username, m_current);
    accept();
}
