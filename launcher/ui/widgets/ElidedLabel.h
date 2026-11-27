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

#pragma once

#include <QFontMetrics>
#include <QLabel>
#include <QPainter>
#include <QString>

/** A label that elides its text to the available width, for pack names in detail headers. */
class ElidedLabel : public QLabel {
   public:
    explicit ElidedLabel(QWidget* parent = nullptr) : QLabel(parent)
    {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        setMinimumWidth(0);
    }

    QSize minimumSizeHint() const override { return QSize(0, QLabel::minimumSizeHint().height()); }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        const QFontMetrics fm = fontMetrics();
        const QString text = fm.elidedText(this->text(), Qt::ElideRight, width());
        painter.setFont(font());
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(rect(), alignment() | Qt::AlignVCenter | Qt::TextSingleLine, text);
    }
};
