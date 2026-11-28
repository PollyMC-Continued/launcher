/* Copyright 2026 PollyMC-Continued Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ModpackDetailsHeader.h"

#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QStringList>
#include <QVBoxLayout>

#include "ui/widgets/ElidedLabel.h"

// The 48px pack tile: the pack's logo with rounded corners, or - while there is no image -
// a coloured tile with the pack's initial, exactly like the result rows.
class ModpackDetailsHeader::PackTile : public QWidget {
   public:
    explicit PackTile(QWidget* parent = nullptr) : QWidget(parent) { setFixedSize(48, 48); }

    void setIcon(const QIcon& icon)
    {
        m_icon = icon;
        update();
    }
    void setName(const QString& name)
    {
        m_name = name;
        update();
    }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF body = QRectF(rect()).adjusted(1, 1, -1, -1);
        const QPalette& pal = palette();

        if (!m_icon.isNull()) {
            QPainterPath clip;
            clip.addRoundedRect(body, 8, 8);
            painter.setClipPath(clip);
            // a backdrop, so logos with transparent edges still read as a tile
            painter.fillRect(body, pal.color(QPalette::Base));
            m_icon.paint(&painter, body.toRect(), Qt::AlignCenter, QIcon::Normal, QIcon::Off);
            return;
        }

        // a deterministic palette colour, the same rule as ModpackItemDelegate: themes stay
        // consistent and packs get varied tiles
        static const QPalette::ColorRole tileRoles[] = { QPalette::Highlight, QPalette::Link, QPalette::LinkVisited,
                                                          QPalette::Mid };
        QColor tile = pal.color(tileRoles[qHash(m_name) % (sizeof(tileRoles) / sizeof(tileRoles[0]))]);
        tile.setAlpha(70);
        painter.setPen(Qt::NoPen);
        painter.setBrush(tile);
        painter.drawRoundedRect(body, 8, 8);

        if (!m_name.isEmpty()) {
            QFont font = this->font();
            font.setWeight(QFont::DemiBold);
            font.setPixelSize(20);
            painter.setFont(font);
            painter.setPen(pal.color(QPalette::Active, QPalette::WindowText));
            painter.drawText(body, Qt::AlignCenter, m_name.left(1).toUpper());
        }
    }

   private:
    QIcon m_icon;
    QString m_name;
};

ModpackDetailsHeader::ModpackDetailsHeader(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    m_tile = new PackTile(this);
    layout->addWidget(m_tile, 0, Qt::AlignTop);

    auto* textColumn = new QVBoxLayout;
    textColumn->setContentsMargins(0, 0, 0, 0);
    textColumn->setSpacing(2);

    m_nameLabel = new ElidedLabel(this);
    QFont nameFont = m_nameLabel->font();
    nameFont.setWeight(QFont::DemiBold);
    nameFont.setPixelSize(16);
    m_nameLabel->setFont(nameFont);
    textColumn->addWidget(m_nameLabel);

    m_bylineLabel = new QLabel(this);
    {
        // muted text straight from the palette, like the hint label of the dialog
        QPalette pal = m_bylineLabel->palette();
        QColor muted = pal.color(QPalette::WindowText);
        muted.setAlphaF(0.65);
        pal.setColor(QPalette::WindowText, muted);
        m_bylineLabel->setPalette(pal);
    }
    textColumn->addWidget(m_bylineLabel);

    layout->addLayout(textColumn, 1);

    clearPack();
}

void ModpackDetailsHeader::setPack(const QString& name, const QString& author, const QString& platform)
{
    m_name = name;
    m_tile->setName(name);
    m_tile->setIcon(QIcon());  // back to the initial tile until the logo arrives
    m_nameLabel->setText(name);

    QStringList byline;
    if (!author.isEmpty())
        byline << tr("by %1").arg(author);
    if (!platform.isEmpty())
        byline << platform;
    m_bylineLabel->setText(byline.join(QStringLiteral(" · ")));
}

void ModpackDetailsHeader::setIcon(const QIcon& icon)
{
    m_tile->setIcon(icon);
}

void ModpackDetailsHeader::clearPack()
{
    m_name.clear();
    m_tile->setName(QString());
    m_tile->setIcon(QIcon());
    m_nameLabel->clear();
    m_bylineLabel->clear();
}

void ModpackDetailsHeader::stylePlaceholder(QLabel* label)
{
    if (!label)
        return;
    QPalette pal = label->palette();
    pal.setColor(QPalette::WindowText, pal.color(QPalette::PlaceholderText));
    label->setPalette(pal);
}
