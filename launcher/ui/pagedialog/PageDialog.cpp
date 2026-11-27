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

#include "PageDialog.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFocusEvent>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTextEdit>
#include <QTreeView>
#include <QVBoxLayout>

#include "Application.h"
#include "settings/SettingsObject.h"

#include "ui/widgets/DialogButton.h"
#include "ui/widgets/LanguageSelectionWidget.h"
#include "ui/widgets/PageContainer.h"

namespace {
bool isInsideLanguagePicker(const QWidget* widget)
{
    for (const QWidget* w = widget; w; w = w->parentWidget()) {
        if (qobject_cast<const LanguageSelectionWidget*>(w))
            return true;
    }
    return false;
}
}  // namespace

PageDialog::PageDialog(BasePageProvider* pageProvider, QString defaultId, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(pageProvider->dialogTitle());
    m_container = new PageContainer(pageProvider, std::move(defaultId), this);

    auto* mainLayout = new QVBoxLayout(this);

    auto* focusStealer = new QPushButton(this);
    mainLayout->addWidget(focusStealer);
    focusStealer->setDefault(true);
    focusStealer->hide();

    mainLayout->addWidget(m_container);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    setLayout(mainLayout);

    // [Help] ........................ [Cancel] [OK]
    m_helpButton = new DialogButton(QString(), DialogButtonRole::Ghost, this);
    m_cancelButton = new DialogButton(QString(), DialogButtonRole::Secondary, this);
    m_okButton = new DialogButton(QString(), DialogButtonRole::Primary, this);
    updateButtonTexts();

    auto* buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(style()->pixelMetric(QStyle::PM_LayoutLeftMargin), 8, style()->pixelMetric(QStyle::PM_LayoutRightMargin),
                                  style()->pixelMetric(QStyle::PM_LayoutBottomMargin));
    buttonRow->setSpacing(8);
    buttonRow->addWidget(m_helpButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(m_cancelButton);
    buttonRow->addWidget(m_okButton);
    m_container->addButtons(buttonRow);

    connect(m_okButton, &QPushButton::clicked, this, &PageDialog::okClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &PageDialog::reject);
    connect(m_helpButton, &QPushButton::clicked, m_container, &PageContainer::help);

    watchPageWidgets();

    restoreGeometry(QByteArray::fromBase64(APPLICATION->settings()->get("PagedGeometry").toString().toUtf8()));
}

void PageDialog::updateButtonTexts()
{
    m_okButton->setText(m_dirty ? tr("&Apply") : tr("&OK"));
    m_cancelButton->setText(tr("&Cancel"));
    m_helpButton->setText(tr("Help"));
}

void PageDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange && m_okButton)
        updateButtonTexts();
    QDialog::changeEvent(event);
}

// Pages have no common "something changed" signal, so hook the editing widgets of every page. Only changes made
// by the user count: signals that code can also trigger (spin boxes, text edits) are checked for focus / mouse.
void PageDialog::watchPageWidgets()
{
    for (auto* page : m_container->getPages()) {
        auto* root = dynamic_cast<QWidget*>(page);
        if (!root)
            continue;

        const auto widgets = root->findChildren<QWidget*>();
        for (auto* widget : widgets) {
            if (auto* button = qobject_cast<QAbstractButton*>(widget)) {
                if (button->isCheckable())
                    connect(button, &QAbstractButton::clicked, this, [this] { markDirty(); });
                else
                    connect(button, &QAbstractButton::clicked, this, [this] { m_lastButtonClick.restart(); });
            } else if (auto* group = qobject_cast<QGroupBox*>(widget)) {
                if (group->isCheckable())
                    connect(group, &QGroupBox::clicked, this, [this] { markDirty(); });
            } else if (auto* combo = qobject_cast<QComboBox*>(widget)) {
                connect(combo, &QComboBox::activated, this, [this] { markDirty(); });
            } else if (auto* edit = qobject_cast<QLineEdit*>(widget)) {
                connect(edit, &QLineEdit::textEdited, this, [this] { markDirty(); });
                // a Browse button filled the field in: counts as an edit too
                connect(edit, &QLineEdit::textChanged, this, [this] {
                    constexpr qint64 BrowseWindowMs = 120 * 1000;
                    if (m_lastButtonClick.isValid() && m_lastButtonClick.elapsed() < BrowseWindowMs)
                        markDirty();
                });
            } else if (auto* spin = qobject_cast<QSpinBox*>(widget)) {
                connect(spin, &QSpinBox::valueChanged, this, [this, spin] {
                    if (spin->hasFocus() || spin->underMouse())
                        markDirty();
                });
            } else if (auto* dspin = qobject_cast<QDoubleSpinBox*>(widget)) {
                connect(dspin, &QDoubleSpinBox::valueChanged, this, [this, dspin] {
                    if (dspin->hasFocus() || dspin->underMouse())
                        markDirty();
                });
            } else if (auto* slider = qobject_cast<QAbstractSlider*>(widget)) {
                connect(slider, &QAbstractSlider::actionTriggered, this, [this] { markDirty(); });
            } else if (auto* text = qobject_cast<QPlainTextEdit*>(widget)) {
                connect(text, &QPlainTextEdit::textChanged, this, [this, text] {
                    if (text->hasFocus() && !text->isReadOnly())
                        markDirty();
                });
            } else if (auto* richText = qobject_cast<QTextEdit*>(widget)) {
                connect(richText, &QTextEdit::textChanged, this, [this, richText] {
                    if (richText->hasFocus() && !richText->isReadOnly())
                        markDirty();
                });
            } else if (auto* keys = qobject_cast<QKeySequenceEdit*>(widget)) {
                connect(keys, &QKeySequenceEdit::keySequenceChanged, this, [this, keys] {
                    if (keys->hasFocus())
                        markDirty();
                });
            } else if (auto* tree = qobject_cast<QTreeView*>(widget)) {
                // the language list: picking a language is a setting; other lists (accounts, ...) are not
                if (isInsideLanguagePicker(tree)) {
                    connect(tree, &QAbstractItemView::clicked, this, [this] { markDirty(); });
                    connect(tree, &QAbstractItemView::activated, this, [this] { markDirty(); });
                }
            }
        }
    }
}

void PageDialog::markDirty()
{
    setDirty(true);
}

void PageDialog::setDirty(bool dirty)
{
    if (m_dirty == dirty)
        return;
    m_dirty = dirty;
    updateButtonTexts();
}

void PageDialog::okClicked()
{
    if (m_dirty)
        applyChanges();
    else
        accept();
}

// "Apply": save everything but keep the dialog open.
void PageDialog::applyChanges()
{
    qDebug() << "Paged dialog apply requested";
    if (!m_container->saveAll())
        return;  // a page refused (it shows its own message), stay dirty

    emit applied();
    setDirty(false);  // after the signal, so changes caused by applying itself do not re-dirty the dialog
}

void PageDialog::accept()
{
    if (handleClose())
        QDialog::accept();
}

void PageDialog::closeEvent(QCloseEvent* event)
{
    if (handleClose())
        QDialog::closeEvent(event);
}

bool PageDialog::handleClose()
{
    qDebug() << "Paged dialog close requested";
    if (!m_container->prepareToClose())
        return false;

    qDebug() << "Paged dialog close approved";
    APPLICATION->settings()->set("PagedGeometry", QString::fromUtf8(saveGeometry().toBase64()));
    qDebug() << "Paged dialog geometry saved";

    emit applied();
    return true;
}
