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

#include "ModpackSearchBox.h"

#include <QApplication>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QStyle>
#include <QStyledItemDelegate>

#include "ui/widgets/ProjectItem.h"

namespace {
constexpr int SourceRowRole = Qt::UserRole + 1;  // row in the source model
constexpr int SourceIdRole = Qt::UserRole + 2;   // id of the source ("modrinth" / "flame")
constexpr int SecondaryRole = Qt::UserRole + 3;  // second line: the source's display name
constexpr int MaxRowsPerSource = 10;

// Two-line result: the pack name, and the source it comes from underneath.
class ResultDelegate : public QStyledItemDelegate {
   public:
    explicit ResultDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override
    {
        return QSize(0, QFontMetrics(option.font).height() * 2 + 10);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const QString primary = opt.text;
        const QString secondary = index.data(SecondaryRole).toString();
        opt.text.clear();
        const QWidget* widget = opt.widget;
        const QStyle* style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        const bool selected = opt.state & QStyle::State_Selected;
        const QColor mainColor = opt.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
        QColor subColor = mainColor;
        subColor.setAlphaF(0.65);

        const QFontMetrics fm(opt.font);
        const QRect area = opt.rect.adjusted(8, 4, -8, -4);
        painter->save();
        painter->setClipRect(area);
        painter->setFont(opt.font);
        painter->setPen(mainColor);
        painter->drawText(QRect(area.left(), area.top(), area.width(), fm.height()), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                          fm.elidedText(primary, Qt::ElideRight, area.width()));
        painter->setPen(subColor);
        const QRect subRect(area.left(), area.top() + fm.height() + 2, area.width(), fm.height());
        painter->drawText(subRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                          fm.elidedText(secondary, Qt::ElideRight, subRect.width()));
        painter->restore();
    }
};
}  // namespace

ModpackSearchBox::ModpackSearchBox(QWidget* parent) : QWidget(parent)
{
    // the box lives inside the sidebar panel: PageContainer paints it with the panel colour
    // (rounded on the top corners), which needs the styled-background attribute to show up
    setAttribute(Qt::WA_StyledBackground, true);

    // same padding the built-in settings search box had, so the sidebar looks unchanged
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 6);
    layout->setSpacing(0);

    m_box = new QLineEdit(this);
    m_box->setClearButtonEnabled(true);
    m_box->setPlaceholderText(tr("Search all modpacks…"));
    m_box->setToolTip(tr("Search Modrinth and CurseForge at once (Ctrl+F)"));
    m_box->setMinimumHeight(34);
    m_box->installEventFilter(this);
    layout->addWidget(m_box);
    setFocusProxy(m_box);
    setFocusPolicy(Qt::StrongFocus);

    m_resultsModel = new QStandardItemModel(this);

    m_popup = new QListView(this);
    m_popup->setWindowFlags(Qt::Popup);
    m_popup->setModel(m_resultsModel);
    m_popup->setItemDelegate(new ResultDelegate(m_popup));
    m_popup->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_popup->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_popup->setMouseTracking(true);
    m_popup->installEventFilter(this);
    connect(m_popup, &QListView::activated, this, [this](const QModelIndex& index) { activateRow(index); });
    connect(m_popup, &QListView::clicked, this, [this](const QModelIndex& index) { activateRow(index); });
    m_popup->hide();

    m_timer.setTimerType(Qt::TimerType::CoarseTimer);
    m_timer.setSingleShot(true);
    m_timer.setInterval(400);
    connect(&m_timer, &QTimer::timeout, this, &ModpackSearchBox::startSearch);
    connect(m_box, &QLineEdit::textChanged, this, &ModpackSearchBox::scheduleSearch);
}

void ModpackSearchBox::addSource(const QString& id, const QString& displayName, QAbstractItemModel* model)
{
    Source source;
    source.id = id;
    source.displayName = displayName;
    source.model = model;
    // the sources update asynchronously; re-merge whatever they currently hold
    connect(model, &QAbstractItemModel::modelReset, this, [this] { rebuildResults(); });
    connect(model, &QAbstractItemModel::rowsInserted, this, [this] { rebuildResults(); });
    connect(model, &QAbstractItemModel::rowsRemoved, this, [this] { rebuildResults(); });
    m_sources.append(source);
}

void ModpackSearchBox::scheduleSearch()
{
    if (m_box->text().trimmed().isEmpty()) {
        m_timer.stop();
        m_searching = false;
        hidePopup();
        m_resultsModel->clear();
        return;
    }
    m_timer.start();
}

void ModpackSearchBox::startSearch()
{
    const QString term = m_box->text().trimmed();
    if (term.size() < 2)
        return;
    m_searching = true;
    emit searchRequested(term);
    showPopup();
}

void ModpackSearchBox::rebuildResults()
{
    m_resultsModel->clear();
    int rows = 0;
    for (const auto& source : m_sources) {
        const int count = qMin(source.model->rowCount(), MaxRowsPerSource);
        for (int row = 0; row < count; ++row) {
            const QModelIndex idx = source.model->index(row, 0);
            QString title = idx.data(UserDataTypes::TITLE).toString();
            if (title.isEmpty())
                title = idx.data(Qt::DisplayRole).toString();
            auto* item = new QStandardItem(title);
            item->setData(source.displayName, SecondaryRole);
            item->setData(row, SourceRowRole);
            item->setData(source.id, SourceIdRole);
            m_resultsModel->appendRow(item);
            ++rows;
        }
    }

    if (rows > 0) {
        m_searching = false;
    } else if (m_searching) {
        auto* status = new QStandardItem(tr("Searching Modrinth and CurseForge…"));
        status->setData(tr("Results appear as each platform answers."), SecondaryRole);
        status->setFlags(Qt::NoItemFlags);
        m_resultsModel->appendRow(status);
    }

    if (m_popup->isVisible())
        resizePopup();
}

void ModpackSearchBox::showPopup()
{
    rebuildResults();
    if (m_popup->isVisible())
        return;
    resizePopup();
    // below the box, or above it when there is no room on the screen
    QPoint below = m_box->mapToGlobal(QPoint(0, m_box->height() + 4));
    if (auto* screen = QApplication::screenAt(below)) {
        if (below.y() + m_popup->height() > screen->availableGeometry().bottom())
            below = m_box->mapToGlobal(QPoint(0, -m_popup->height() - 4));
    }
    m_popup->move(below);
    m_popup->show();
    if (m_resultsModel->rowCount() != 0)
        m_popup->setCurrentIndex(m_resultsModel->index(0, 0));
}

void ModpackSearchBox::hidePopup()
{
    m_popup->hide();
}

void ModpackSearchBox::resizePopup()
{
    const int width = qMax(m_box->width(), 320);
    const int rowHeight = qMax(QFontMetrics(m_popup->font()).height() * 2 + 10, 44);
    const int height = qBound(60, m_resultsModel->rowCount() * rowHeight + 8, 400);
    m_popup->resize(width, height);
}

void ModpackSearchBox::activateRow(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    const QString id = index.data(SourceIdRole).toString();
    const int row = index.data(SourceRowRole).toInt();
    hidePopup();
    for (const auto& source : m_sources) {
        if (source.id == id) {
            const QModelIndex sourceIndex = source.model->index(row, 0);
            if (sourceIndex.isValid())
                emit resultActivated(id, sourceIndex);
            return;
        }
    }
}

bool ModpackSearchBox::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_box && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Down && m_popup->isVisible() && m_resultsModel->rowCount() > 0) {
            m_popup->setFocus();
            m_popup->setCurrentIndex(m_resultsModel->index(0, 0));
            return true;
        }
    }
    if (watched == m_popup) {
        if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                hidePopup();
                return true;
            }
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                activateRow(m_popup->currentIndex());
                return true;
            }
            if (keyEvent->key() != Qt::Key_Down && keyEvent->key() != Qt::Key_Up && keyEvent->key() != Qt::Key_PageUp &&
                keyEvent->key() != Qt::Key_PageDown) {
                // typing while the dropdown is open: send the characters to the search box
                QApplication::sendEvent(m_box, event);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
