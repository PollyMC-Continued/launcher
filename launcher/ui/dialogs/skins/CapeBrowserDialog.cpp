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

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPixmap>
#include <QPushButton>
#include <QShowEvent>
#include <QUrl>
#include <QVBoxLayout>

#include "Application.h"
#include "DiscordRichPresence.h"
#include "settings/SettingsObject.h"
#include "ui/dialogs/CustomMessageBox.h"

static const QLatin1String geometryKey("CapeBrowserGeometry");

CapeBrowserDialog::CapeBrowserDialog(QWidget* parent) : QDialog(parent), m_api(new Crafty::API(this))
{
    setWindowModality(Qt::WindowModal);
    setWindowTitle(tr("Browse capes"));

    auto geometry = APPLICATION->settings()->get(geometryKey).toString();
    if (geometry.isEmpty()) {
        const auto base = parent ? parent->size() : QSize(1024, 768);
        resize(qMax(qRound(base.width() * 0.5), 560), qMax(qRound(base.height() * 0.6), 420));
    } else {
        restoreGeometry(QByteArray::fromBase64(geometry.toUtf8()));
    }

    auto rootLayout = new QVBoxLayout(this);

    // the honest description of what this dialog can do
    m_helpLabel = new QLabel(tr("crafty.gg's public API does not document a cape catalog; capes are looked up from "
                                "the player's profile where crafty.gg tracks Mojang and OptiFine capes. Cape "
                                "textures are typically 462x217 or 512x256 pixel PNG files."),
                             this);
    m_helpLabel->setWordWrap(true);
    rootLayout->addWidget(m_helpLabel);

    auto searchRow = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Player name on crafty.gg..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchBtn = new QPushButton(tr("Search"), this);
    searchRow->addWidget(m_searchEdit, 1);
    searchRow->addWidget(m_searchBtn);
    rootLayout->addLayout(searchRow);

    m_capeList = new QListWidget(this);
    m_capeList->setIconSize(QSize(128, 60));
    m_capeList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_capeList->setSpacing(6);
    rootLayout->addWidget(m_capeList, 1);

    auto urlRow = new QHBoxLayout();
    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText(tr("Import a cape directly from a PNG URL..."));
    m_urlEdit->setClearButtonEnabled(true);
    m_importBtn = new QPushButton(tr("Import"), this);
    urlRow->addWidget(m_urlEdit, 1);
    urlRow->addWidget(m_importBtn);
    rootLayout->addLayout(urlRow);

    m_sourceLabel = new QLabel(this);
    m_sourceLabel->setAlignment(Qt::AlignCenter);
    m_sourceLabel->setWordWrap(true);
    rootLayout->addWidget(m_sourceLabel);

    m_linkLabel = new QLabel(this);
    m_linkLabel->setOpenExternalLinks(true);
    m_linkLabel->setAlignment(Qt::AlignCenter);
    m_linkLabel->hide();
    rootLayout->addWidget(m_linkLabel);

    m_statusLabel = new QLabel(tr("Enter a player name to look up their capes."), this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    rootLayout->addWidget(m_statusLabel);

    auto bottomRow = new QHBoxLayout();
    m_closeBtn = new QPushButton(tr("Close"), this);
    bottomRow->addWidget(m_closeBtn);
    bottomRow->addStretch(1);
    m_useBtn = new QPushButton(tr("Use cape"), this);
    m_useBtn->setEnabled(false);
    m_useBtn->setToolTip(tr("Apply this cape to the selected account"));
    bottomRow->addWidget(m_useBtn);
    rootLayout->addLayout(bottomRow);

    connect(m_searchBtn, &QPushButton::clicked, this, &CapeBrowserDialog::search);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &CapeBrowserDialog::search);
    connect(m_importBtn, &QPushButton::clicked, this, &CapeBrowserDialog::importUrl);
    connect(m_urlEdit, &QLineEdit::returnPressed, this, &CapeBrowserDialog::importUrl);
    connect(m_capeList, &QListWidget::currentRowChanged, this, &CapeBrowserDialog::currentRowChanged);
    connect(m_capeList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { useCape(); });
    connect(m_closeBtn, &QPushButton::clicked, this, &CapeBrowserDialog::reject);
}

CapeBrowserDialog::~CapeBrowserDialog()
{
    // invalidates every callback that is still in flight
    m_request++;
    m_api->abortAll();
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
    if (name.isEmpty())
        return;
    m_player = name;

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
    auto urlText = m_urlEdit->text().trimmed();
    if (urlText.isEmpty())
        return;
    auto url = QUrl::fromUserInput(urlText);
    if (!url.isValid() || (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http"))) {
        showError(tr("%1 is not a valid http(s) URL.").arg(urlText));
        return;
    }

    const int request = ++m_request;
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
    return QStringLiteral("%1 (%2)").arg(title, cape->source);
}

void CapeBrowserDialog::fillList()
{
    m_capeList->clear();
    m_current.clear();

    for (const auto& cape : m_capes) {
        auto item = new QListWidgetItem(m_capeList);
        item->setText(displayName(cape));
        if (!cape->preview.isNull())
            item->setIcon(QIcon(QPixmap::fromImage(cape->preview)));
        item->setToolTip(cape->textureUrl.isEmpty() ? tr("Source: %1").arg(cape->source)
                                                    : tr("Source: %1\n%2").arg(cape->source, cape->textureUrl.toString()));
    }
    if (!m_capes.isEmpty())
        m_capeList->setCurrentRow(0);
    updateDetails();
}

void CapeBrowserDialog::currentRowChanged(int row)
{
    if (row < 0 || row >= m_capes.size()) {
        m_current.clear();
        updateDetails();
        return;
    }
    m_current = m_capes.at(row);
    updateDetails();
}

void CapeBrowserDialog::updateDetails()
{
    m_useBtn->setEnabled(m_current && !m_current->texture.isNull());

    if (!m_current) {
        m_sourceLabel->clear();
        m_linkLabel->hide();
        return;
    }

    m_sourceLabel->setText(tr("Source: %1").arg(m_current->source));
    const auto player = m_current->username.isEmpty() ? m_player : m_current->username;
    if (player.isEmpty()) {
        m_linkLabel->hide();
    } else {
        m_linkLabel->setText(QStringLiteral("<a href=\"https://crafty.gg/@%1\">%2</a>")
                                 .arg(QString::fromUtf8(QUrl::toPercentEncoding(player)), tr("Open crafty.gg page")));
        m_linkLabel->show();
    }
}

void CapeBrowserDialog::useCape()
{
    if (!m_current || m_current->texture.isNull())
        return;
    emit selected(m_current->username.isEmpty() ? m_player : m_current->username, m_current);
    accept();
}
