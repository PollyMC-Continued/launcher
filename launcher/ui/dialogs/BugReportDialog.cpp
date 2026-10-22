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
 */

#include "BugReportDialog.h"

#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QScreen>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSysInfo>
#include <QTextCursor>
#include <QVBoxLayout>

#include "Application.h"
#include "BuildConfig.h"
#include "FileSystem.h"
#include "logs/AnonymizeLog.h"
#include "net/BugReportUpload.h"
#include "net/NetJob.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"

namespace {
constexpr int MaxScreenshots = 3;
constexpr int MaxScreenshotEdge = 1280;
constexpr int MaxScreenshotBytes = 800 * 1024;  // the relay (Vercel) rejects request bodies above ~4.5 MB
constexpr int MinDescriptionChars = 10;
constexpr int MaxDescriptionChars = 4000;
constexpr int MaxNameChars = 40;
constexpr int MaxContactChars = 80;
constexpr int MaxPayloadBytes = 4300000;  // stay below the relay's request size limit
constexpr int MaxLogChars = 200000;  // keep the tail of the log: crashes are at the end
constexpr int CooldownSeconds = 60;

QDateTime s_lastSent;  // per run, stops accidental double submits

QByteArray encodeScreenshot(const QImage& source)
{
    QImage image = source;
    if (image.width() > MaxScreenshotEdge || image.height() > MaxScreenshotEdge)
        image = image.scaled(MaxScreenshotEdge, MaxScreenshotEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // JPEG has no alpha: flatten onto white
    QImage flat(image.size(), QImage::Format_RGB32);
    flat.fill(Qt::white);
    {
        QPainter painter(&flat);
        painter.drawImage(0, 0, image);
    }

    for (int quality : { 80, 55, 35 }) {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        flat.save(&buffer, "JPEG", quality);
        if (bytes.size() <= MaxScreenshotBytes || quality == 35)
            return bytes;
    }
    return {};
}
}  // namespace

BugReportDialog::BugReportDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Report a Bug"));
    setSizeGripEnabled(true);

    // Same idea as the settings window: a sensible default size that never exceeds the screen, and the content scrolls.
    setMinimumSize(460, 340);
    QSize wanted(780, 640);
    if (const QScreen* screen = parent && parent->screen() ? parent->screen() : QGuiApplication::primaryScreen()) {
        const QSize avail = screen->availableSize();
        wanted.setWidth(qMin(wanted.width(), int(avail.width() * 0.9)));
        wanted.setHeight(qMin(wanted.height(), int(avail.height() * 0.85)));
    }
    resize(wanted);

    auto outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ---- scrollable body ----
    auto scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll, 1);

    auto body = new QWidget(scroll);
    scroll->setWidget(body);
    auto layout = new QVBoxLayout(body);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(8);

    auto title = new QLabel(tr("Report a Bug"), body);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    if (titleFont.pointSize() > 0)
        titleFont.setPointSize(titleFont.pointSize() + 4);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto intro = new QLabel(tr("Tell us what went wrong or what you would like to see. Your report goes <b>privately</b> to the PollyMC team on Discord. "
                               "Please don't include passwords or other personal information."),
                            body);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto line = [body]() {
        auto f = new QFrame(body);
        f->setFrameShape(QFrame::HLine);
        f->setFrameShadow(QFrame::Sunken);
        return f;
    };
    layout->addWidget(line());

    // type / name / contact in a compact grid
    auto grid = new QGridLayout();
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(6);
    grid->setColumnStretch(1, 1);

    m_type = new QComboBox(body);
    m_type->addItem(tr("Bug"), QStringLiteral("bug"));
    m_type->addItem(tr("Crash"), QStringLiteral("crash"));
    m_type->addItem(tr("Suggestion"), QStringLiteral("feature"));
    m_type->addItem(tr("Something else"), QStringLiteral("other"));
    grid->addWidget(new QLabel(tr("Type of report"), body), 0, 0);
    grid->addWidget(m_type, 0, 1);

    m_name = new QLineEdit(body);
    m_name->setMaxLength(MaxNameChars);
    m_name->setPlaceholderText(tr("Anonymous"));
    grid->addWidget(new QLabel(tr("Your name"), body), 1, 0);
    grid->addWidget(m_name, 1, 1);

    m_contact = new QLineEdit(body);
    m_contact->setMaxLength(MaxContactChars);
    m_contact->setPlaceholderText(tr("Discord username or email, only if you want a reply"));
    grid->addWidget(new QLabel(tr("Contact"), body), 2, 0);
    grid->addWidget(m_contact, 2, 1);
    layout->addLayout(grid);

    auto descLabel = new QLabel(tr("<b>What happened?</b>"), body);
    layout->addWidget(descLabel);
    m_description = new QPlainTextEdit(body);
    m_description->setPlaceholderText(tr("Describe the bug and, if you can, the steps to make it happen again.\n\n"
                                         "What did you expect to happen? What happened instead?"));
    m_description->setMinimumHeight(150);
    layout->addWidget(m_description, 1);
    m_counter = new QLabel(body);
    m_counter->setAlignment(Qt::AlignRight);
    layout->addWidget(m_counter);

    layout->addWidget(line());

    layout->addWidget(new QLabel(tr("<b>Screenshots</b> (optional, up to %1)").arg(MaxScreenshots), body));
    m_shots = new QListWidget(body);
    m_shots->setViewMode(QListView::IconMode);
    m_shots->setFlow(QListView::LeftToRight);
    m_shots->setWrapping(false);
    m_shots->setMovement(QListView::Static);
    m_shots->setIconSize(QSize(80, 80));
    m_shots->setFixedHeight(122);
    layout->addWidget(m_shots);

    auto shotButtons = new QHBoxLayout();
    m_addShot = new QPushButton(tr("Add image…"), body);
    m_pasteShot = new QPushButton(tr("Paste from clipboard"), body);
    m_removeShot = new QPushButton(tr("Remove"), body);
    shotButtons->addWidget(m_addShot);
    shotButtons->addWidget(m_pasteShot);
    shotButtons->addWidget(m_removeShot);
    shotButtons->addStretch(1);
    layout->addLayout(shotButtons);

    layout->addWidget(line());

    m_includeLog = new QCheckBox(tr("Include the launcher log (recommended, personal paths are hidden)"), body);
    m_includeLog->setChecked(true);
    layout->addWidget(m_includeLog);

    auto info = new QLabel(tr("Also sent automatically:<br>%1").arg(systemInfoText().toHtmlEscaped().replace("\n", "<br>")), body);
    info->setWordWrap(true);
    info->setTextInteractionFlags(Qt::TextSelectableByMouse);
    info->setEnabled(false);
    layout->addWidget(info);

    if (!BuildConfig.BUG_TRACKER_URL.isEmpty()) {
        auto link = new QLabel(tr("Prefer GitHub? <a href=\"%1\">Open the issue tracker</a>.").arg(BuildConfig.BUG_TRACKER_URL), body);
        link->setOpenExternalLinks(true);
        layout->addWidget(link);
    }

    // ---- fixed footer: buttons are always visible ----
    outer->addWidget(line());
    auto footer = new QHBoxLayout();
    footer->setContentsMargins(12, 8, 12, 10);
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_sendButton = m_buttons->addButton(tr("Send Report"), QDialogButtonBox::AcceptRole);
    m_sendButton->setDefault(true);
    footer->addWidget(m_buttons);
    outer->addLayout(footer);

    connect(m_buttons, &QDialogButtonBox::accepted, this, &BugReportDialog::send);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_description, &QPlainTextEdit::textChanged, this, &BugReportDialog::updateState);
    connect(m_addShot, &QPushButton::clicked, this, &BugReportDialog::addScreenshotFromFile);
    connect(m_pasteShot, &QPushButton::clicked, this, &BugReportDialog::addScreenshotFromClipboard);
    connect(m_removeShot, &QPushButton::clicked, this, &BugReportDialog::removeSelectedScreenshot);
    connect(m_shots, &QListWidget::itemSelectionChanged, this, &BugReportDialog::updateState);

    updateState();
}

bool BugReportDialog::hasUnsentInput() const
{
    return m_description->toPlainText().trimmed().size() > 0 || !m_shotData.isEmpty();
}

void BugReportDialog::reject()
{
    if (hasUnsentInput()) {
        const auto answer = QMessageBox::question(this, tr("Report a Bug"), tr("Discard this report? What you wrote will be lost."),
                                                  QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Discard)
            return;
    }
    QDialog::reject();
}

QString BugReportDialog::systemInfoText() const
{
    return tr("Launcher: %1 (%2)\nSystem: %3\nCPU architecture: %4\nQt: %5")
        .arg(BuildConfig.printableVersionString(), BuildConfig.BUILD_PLATFORM.isEmpty() ? QStringLiteral("unknown") : BuildConfig.BUILD_PLATFORM,
             QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture(), QString::fromLatin1(qVersion()));
}

void BugReportDialog::updateState()
{
    QString text = m_description->toPlainText();
    if (text.size() > MaxDescriptionChars) {
        QSignalBlocker blocker(m_description);
        m_description->setPlainText(text.left(MaxDescriptionChars));
        m_description->moveCursor(QTextCursor::End);
        text = m_description->toPlainText();
    }
    m_counter->setText(QStringLiteral("%1 / %2").arg(text.size()).arg(MaxDescriptionChars));

    m_sendButton->setEnabled(text.trimmed().size() >= MinDescriptionChars);
    const bool full = m_shotData.size() >= MaxScreenshots;
    m_addShot->setEnabled(!full);
    m_pasteShot->setEnabled(!full);
    m_removeShot->setEnabled(!m_shots->selectedItems().isEmpty());
}

void BugReportDialog::addScreenshot(const QImage& image)
{
    if (image.isNull() || m_shotData.size() >= MaxScreenshots)
        return;
    const QByteArray bytes = encodeScreenshot(image);
    if (bytes.isEmpty())
        return;

    m_shotData.append(bytes);
    auto icon = QIcon(QPixmap::fromImage(image.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    m_shots->addItem(new QListWidgetItem(icon, tr("Screenshot %1").arg(m_shotData.size())));
    updateState();
}

void BugReportDialog::addScreenshotFromFile()
{
    const auto files = QFileDialog::getOpenFileNames(this, tr("Choose screenshots"), QString(), tr("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
    for (const auto& file : files) {
        if (m_shotData.size() >= MaxScreenshots)
            break;
        QImage image(file);
        if (image.isNull()) {
            QMessageBox::warning(this, tr("Report a Bug"), tr("Could not open %1 as an image.").arg(file));
            continue;
        }
        addScreenshot(image);
    }
}

void BugReportDialog::addScreenshotFromClipboard()
{
    const QImage image = QApplication::clipboard()->image();
    if (image.isNull()) {
        QMessageBox::information(this, tr("Report a Bug"), tr("There is no image on the clipboard."));
        return;
    }
    addScreenshot(image);
}

void BugReportDialog::removeSelectedScreenshot()
{
    const auto selected = m_shots->selectedItems();
    if (selected.isEmpty())
        return;
    const int row = m_shots->row(selected.first());
    delete m_shots->takeItem(row);
    m_shotData.removeAt(row);
    for (int i = 0; i < m_shots->count(); ++i)
        m_shots->item(i)->setText(tr("Screenshot %1").arg(i + 1));
    updateState();
}

QByteArray BugReportDialog::buildPayload() const
{
    QJsonObject obj;
    obj["type"] = m_type->currentData().toString();
    obj["name"] = m_name->text().trimmed();
    obj["contact"] = m_contact->text().trimmed();
    obj["description"] = m_description->toPlainText().trimmed();
    obj["launcherVersion"] = BuildConfig.printableVersionString();
    obj["platform"] = BuildConfig.BUILD_PLATFORM;
    obj["system"] = QSysInfo::prettyProductName();
    obj["arch"] = QSysInfo::currentCpuArchitecture();
    obj["qt"] = QString::fromLatin1(qVersion());

    if (m_includeLog->isChecked() && APPLICATION->logFile) {
        APPLICATION->logFile->flush();
        QString log = QString::fromUtf8(FS::read(APPLICATION->logFile->fileName()));
        if (log.size() > MaxLogChars)
            log = log.right(MaxLogChars);
        anonymizeLog(log);
        obj["log"] = log;
    }

    QJsonArray shots;
    for (const auto& bytes : m_shotData)
        shots.append(QString::fromLatin1(bytes.toBase64()));
    obj["screenshots"] = shots;

    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

void BugReportDialog::send()
{
    if (s_lastSent.isValid() && s_lastSent.secsTo(QDateTime::currentDateTime()) < CooldownSeconds) {
        QMessageBox::information(this, tr("Report a Bug"), tr("You just sent a report. Please wait a minute before sending another one."));
        return;
    }

    const QByteArray payload = buildPayload();
    if (payload.size() > MaxPayloadBytes) {
        QMessageBox::information(this, tr("Report a Bug"),
                                 tr("This report is too large to send (%1 MB). Remove a screenshot or untick \"Include the launcher log\" and try again.")
                                     .arg(payload.size() / 1000000.0, 0, 'f', 1));
        return;
    }

    auto upload = new BugReportUpload(QUrl(BuildConfig.BUG_REPORT_API_URL), payload);
    auto job = NetJob::Ptr(new NetJob("Bug Report", APPLICATION->network()));
    job->addNetAction(Net::NetRequest::Ptr(upload));

    QWidget* parentWidget = this;
    connect(job.get(), &Task::failed, this, [parentWidget](QString reason) {
        CustomMessageBox::selectable(parentWidget, QObject::tr("Failed to send the report"), reason, QMessageBox::Critical)->show();
    });

    ProgressDialog progress(this);
    if (progress.execWithTask(job.get()) != QDialog::Accepted)
        return;

    s_lastSent = QDateTime::currentDateTime();
    CustomMessageBox::selectable(this, tr("Report sent"), tr("Thank you! Your report has been sent to the PollyMC team."), QMessageBox::Information)->exec();
    accept();
}
