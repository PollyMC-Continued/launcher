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
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSet>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSysInfo>
#include <QTextCursor>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include "Application.h"
#include "BuildConfig.h"
#include "DesktopServices.h"
#include "FileSystem.h"
#include "logs/AnonymizeLog.h"
#include "settings/SettingsObject.h"
#include "ui/dialogs/CustomMessageBox.h"

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
// An unsent draft older than this is silently dropped instead of restored.
constexpr int DraftMaxAgeDays = 7;

QDateTime s_lastSent;  // per run, stops accidental double submits
// Descriptions already sent this session (normalized) - used to warn about duplicates.
QSet<QString> s_sentThisSession;

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
    setAcceptDrops(true);

    // Size it like the settings window: a sensible default that is never taller or wider than the screen, resizable, and
    // remembered between runs.
    QScreen* targetScreen = (parent && parent->screen()) ? parent->screen() : QGuiApplication::primaryScreen();
    const QSize available = targetScreen ? targetScreen->availableGeometry().size() : QSize(1280, 720);
    setMinimumSize(qMin(460, available.width()), qMin(360, available.height()));
    const QSize maxSize(available.width() * 9 / 10, available.height() * 9 / 10);
    if (!restoreGeometry(QByteArray::fromBase64(APPLICATION->settings()->get("BugReportGeometry").toString().toUtf8())))
        resize(QSize(720, 640).boundedTo(maxSize));
    else
        resize(size().boundedTo(maxSize));  // a saved size from a bigger monitor must not run off this one

    auto outer = new QVBoxLayout(this);

    // ---- header (always visible) ----
    m_intro = new QLabel(tr("Tell us what went wrong or what you would like to see. Your report goes <b>privately</b> to the PollyMC team on Discord. "
                            "Please don't include passwords or other personal information."),
                         this);
    m_intro->setWordWrap(true);
    outer->addWidget(m_intro);

    // ---- scrollable form ----
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(m_scroll, 1);

    m_form = new QWidget(m_scroll);
    auto layout = new QVBoxLayout(m_form);
    layout->setContentsMargins(0, 0, 8, 0);  // leave room for the scroll bar
    m_scroll->setWidget(m_form);

    layout->addWidget(new QLabel(tr("What kind of report is this?"), m_form));
    m_type = new QComboBox(m_form);
    m_type->addItem(tr("Bug"), QStringLiteral("bug"));
    m_type->addItem(tr("Crash"), QStringLiteral("crash"));
    m_type->addItem(tr("Suggestion"), QStringLiteral("feature"));
    m_type->addItem(tr("Something else"), QStringLiteral("other"));
    layout->addWidget(m_type);

    // name and contact side by side: saves a lot of height
    auto whoGrid = new QGridLayout();
    whoGrid->setContentsMargins(0, 0, 0, 0);
    m_name = new QLineEdit(m_form);
    m_name->setMaxLength(MaxNameChars);
    m_name->setPlaceholderText(tr("Anonymous"));
    m_contact = new QLineEdit(m_form);
    m_contact->setMaxLength(MaxContactChars);
    m_contact->setPlaceholderText(tr("Discord username or email"));
    m_contact->setToolTip(tr("Only fill this in if you want a reply."));
    whoGrid->addWidget(new QLabel(tr("Your name (optional)"), m_form), 0, 0);
    whoGrid->addWidget(new QLabel(tr("Contact (optional)"), m_form), 0, 1);
    whoGrid->addWidget(m_name, 1, 0);
    whoGrid->addWidget(m_contact, 1, 1);
    layout->addLayout(whoGrid);

    m_descriptionLabel = new QLabel(m_form);
    layout->addWidget(m_descriptionLabel);
    m_description = new QPlainTextEdit(m_form);
    m_description->setMinimumHeight(140);
    m_description->viewport()->setAcceptDrops(false);  // let image drops reach the dialog instead of being pasted as text
    layout->addWidget(m_description, 1);
    m_counter = new QLabel(m_form);
    m_counter->setAlignment(Qt::AlignRight);
    layout->addWidget(m_counter);

    layout->addWidget(new QLabel(tr("Screenshots (optional, up to %1). You can also drag images into this window.").arg(MaxScreenshots), m_form));
    m_shots = new QListWidget(m_form);
    m_shots->setViewMode(QListView::IconMode);
    m_shots->setFlow(QListView::LeftToRight);
    m_shots->setWrapping(false);
    m_shots->setMovement(QListView::Static);
    m_shots->setIconSize(QSize(96, 96));
    m_shots->setFixedHeight(130);
    layout->addWidget(m_shots);

    auto shotButtons = new QHBoxLayout();
    m_addShot = new QPushButton(tr("Add image…"), m_form);
    m_pasteShot = new QPushButton(tr("Paste from clipboard"), m_form);
    m_removeShot = new QPushButton(tr("Remove"), m_form);
    shotButtons->addWidget(m_addShot);
    shotButtons->addWidget(m_pasteShot);
    shotButtons->addWidget(m_removeShot);
    shotButtons->addStretch(1);
    layout->addLayout(shotButtons);

    m_includeLog = new QCheckBox(tr("Include the launcher log (recommended, personal paths are hidden)"), m_form);
    m_includeLog->setChecked(true);
    layout->addWidget(m_includeLog);

    // what is sent automatically: collapsed by default so it doesn't eat the screen
    m_detailsToggle = new QToolButton(m_form);
    m_detailsToggle->setText(tr("What else is sent automatically?"));
    m_detailsToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_detailsToggle->setArrowType(Qt::RightArrow);
    m_detailsToggle->setAutoRaise(true);
    m_detailsToggle->setCheckable(true);
    layout->addWidget(m_detailsToggle, 0, Qt::AlignLeft);

    m_details = new QLabel(systemInfoText().toHtmlEscaped().replace("\n", "<br>"), m_form);
    m_details->setWordWrap(true);
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_details->setContentsMargins(18, 0, 0, 0);
    m_details->setVisible(false);
    layout->addWidget(m_details);

    // ---- footer (always visible) ----
    auto footer = new QHBoxLayout();
    if (!BuildConfig.BUG_TRACKER_URL.isEmpty()) {
        auto link = new QLabel(tr("Prefer GitHub? <a href=\"%1\">Open the issue tracker</a>.").arg(BuildConfig.BUG_TRACKER_URL), this);
        link->setOpenExternalLinks(true);
        footer->addWidget(link);
    }
    footer->addStretch(1);
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_sendButton = m_buttons->addButton(tr("Send Report"), QDialogButtonBox::AcceptRole);
    m_sendButton->setDefault(true);
    footer->addWidget(m_buttons);
    outer->addLayout(footer);

    connect(m_buttons, &QDialogButtonBox::accepted, this, &BugReportDialog::send);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_description, &QPlainTextEdit::textChanged, this, &BugReportDialog::updateState);
    connect(m_type, &QComboBox::currentIndexChanged, this, &BugReportDialog::updateTypeHints);
    connect(m_addShot, &QPushButton::clicked, this, &BugReportDialog::addScreenshotFromFile);
    connect(m_pasteShot, &QPushButton::clicked, this, &BugReportDialog::addScreenshotFromClipboard);
    connect(m_removeShot, &QPushButton::clicked, this, &BugReportDialog::removeSelectedScreenshot);
    connect(m_shots, &QListWidget::itemSelectionChanged, this, &BugReportDialog::updateState);
    connect(m_detailsToggle, &QToolButton::toggled, this, [this](bool open) {
        m_details->setVisible(open);
        m_detailsToggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    });

    // Ctrl+Enter sends from anywhere in the form (plain Enter must keep adding new lines in the description)
    auto sendShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    connect(sendShortcut, &QShortcut::activated, this, [this]() {
        if (m_sendButton->isEnabled())
            send();
    });

    updateTypeHints();
    updateState();

    restoreDraft();
}

void BugReportDialog::updateTypeHints()
{
    // The label and the placeholder follow the report type, so a suggestion doesn't ask you to "describe the bug".
    const QString type = m_type->currentData().toString();
    if (type == QLatin1String("crash")) {
        m_descriptionLabel->setText(tr("What were you doing when it crashed?"));
        m_description->setPlaceholderText(tr("Tell us what you were doing right before the launcher closed or froze, and whether it happens every time."));
    } else if (type == QLatin1String("feature")) {
        m_descriptionLabel->setText(tr("What would you like to see?"));
        m_description->setPlaceholderText(tr("Describe your idea and what it would make easier for you. Examples from other apps help too."));
    } else if (type == QLatin1String("other")) {
        m_descriptionLabel->setText(tr("What's on your mind?"));
        m_description->setPlaceholderText(tr("Tell us anything that doesn't fit the other types: feedback, questions, or something that just feels off."));
    } else {
        m_descriptionLabel->setText(tr("What happened?"));
        m_description->setPlaceholderText(tr("Describe the bug and, if you can, the steps to make it happen again."));
    }
}

void BugReportDialog::done(int result)
{
    // Keep what was typed if the window is closed by accident (Esc, the X, a stray click on Cancel).
    // A successful send clears the draft; a deliberately empty form clears it too.
    if (!m_sentThisSession && m_description->toPlainText().trimmed().size() >= MinDescriptionChars) {
        saveDraft();
    } else {
        clearDraft();
    }

    APPLICATION->settings()->set("BugReportGeometry", QString::fromUtf8(saveGeometry().toBase64()));
    QDialog::done(result);
}

QString BugReportDialog::draftPath() const
{
    // relative on purpose: the launcher's working directory is its data folder
    return QStringLiteral("bugreport_draft.json");
}

void BugReportDialog::saveDraft() const
{
    QJsonObject obj;
    obj["type"] = m_type->currentData().toString();
    obj["name"] = m_name->text();
    obj["contact"] = m_contact->text();
    obj["description"] = m_description->toPlainText();
    obj["includeLog"] = m_includeLog->isChecked();
    obj["saved"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    try {
        FS::write(draftPath(), QJsonDocument(obj).toJson(QJsonDocument::Compact));
        qDebug() << "Saved an unsent bug report draft";
    } catch ([[maybe_unused]] const Exception& e) {
        qWarning() << "Could not save the bug report draft";
    }
}

void BugReportDialog::restoreDraft()
{
    const QString path = draftPath();
    if (!QFileInfo::exists(path))
        return;

    // a draft from weeks ago is more confusing than helpful
    const QFileInfo info(path);
    if (info.lastModified().daysTo(QDateTime::currentDateTime()) > DraftMaxAgeDays) {
        clearDraft();
        return;
    }

    QJsonObject obj;
    try {
        obj = QJsonDocument::fromJson(FS::read(path)).object();
    } catch ([[maybe_unused]] const Exception& e) {
        qWarning() << "Could not read the bug report draft";
        return;
    }

    const QString description = obj.value("description").toString().trimmed();
    if (description.size() < MinDescriptionChars)
        return;

    const int typeIndex = m_type->findData(obj.value("type").toString());
    if (typeIndex >= 0)
        m_type->setCurrentIndex(typeIndex);
    m_name->setText(obj.value("name").toString());
    m_contact->setText(obj.value("contact").toString());
    m_description->setPlainText(description);
    m_includeLog->setChecked(obj.value("includeLog").toBool());

    m_intro->setText(m_intro->text() + QStringLiteral("<br><br>") +
                     tr("<i>An unsent report you started earlier was restored - it is ready to finish and send whenever you are.</i>"));

    updateTypeHints();
    updateState();
    qDebug() << "Restored an unsent bug report draft from" << info.lastModified().toString(Qt::ISODate);
}

void BugReportDialog::clearDraft() const
{
    if (QFileInfo::exists(draftPath()))
        FS::deletePath(draftPath());
}

QString BugReportDialog::normalizeForDuplicateCheck(const QString& text)
{
    // same words in the same order, ignoring whitespace and case, counts as the same report
    return text.simplified().toLower();
}

void BugReportDialog::dragEnterEvent(QDragEnterEvent* event)
{
    const QMimeData* mime = event->mimeData();
    if (m_form->isEnabled() && m_shotData.size() < MaxScreenshots && (mime->hasImage() || mime->hasUrls()))
        event->acceptProposedAction();
}

void BugReportDialog::dropEvent(QDropEvent* event)
{
    if (!m_form->isEnabled())
        return;
    const QMimeData* mime = event->mimeData();
    if (mime->hasUrls()) {
        for (const QUrl& url : mime->urls()) {
            if (m_shotData.size() >= MaxScreenshots)
                break;
            if (url.isLocalFile())
                addScreenshot(QImage(url.toLocalFile()));  // silently ignores files that aren't images
        }
        event->acceptProposedAction();
    } else if (mime->hasImage()) {
        addScreenshot(qvariant_cast<QImage>(mime->imageData()));
        event->acceptProposedAction();
    }
}

void BugReportDialog::setBusy(bool busy)
{
    // lock the form while a report is on its way so nothing changes (or gets sent twice) mid-send
    m_form->setEnabled(!busy);
    m_sendButton->setText(busy ? tr("Sending…") : tr("Send Report"));
    if (busy)
        m_sendButton->setEnabled(false);
    else
        updateState();
}

void BugReportDialog::prefillError(const QString& title, const QString& details)
{
    constexpr int MaxDetailChars = 2000;
    m_intro->setText(tr("We filled in the error for you. Add what you were doing when it happened, then send it. "
                        "Your report goes <b>privately</b> to the PollyMC team on Discord."));
    m_type->setCurrentIndex(m_type->findData(QStringLiteral("bug")));

    QString body = details.trimmed();
    if (body.size() > MaxDetailChars)
        body = body.left(MaxDetailChars) + QStringLiteral("\u2026");
    m_description->setPlainText(tr("Error: %1\n%2\n\nWhat I was doing when this happened:\n").arg(title, body));
    m_description->moveCursor(QTextCursor::End);
    m_description->setFocus();
}

void BugReportDialog::reportError(QWidget* parent, const QString& title, const QString& details)
{
    if (BuildConfig.BUG_REPORT_API_URL.isEmpty()) {
        if (!BuildConfig.BUG_TRACKER_URL.isEmpty())
            DesktopServices::openUrl(QUrl(BuildConfig.BUG_TRACKER_URL));
        return;
    }
    BugReportDialog dialog(parent);
    dialog.prefillError(title, details);
    dialog.exec();
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
    const int trimmedSize = text.trimmed().size();
    m_counter->setText(trimmedSize < MinDescriptionChars
                           ? tr("Write at least %1 characters to send (%2 / %3)").arg(MinDescriptionChars).arg(text.size()).arg(MaxDescriptionChars)
                           : QStringLiteral("%1 / %2").arg(text.size()).arg(MaxDescriptionChars));

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
    auto icon = QIcon(QPixmap::fromImage(image.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
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

    // Sending the same text twice rarely helps the team - say so and let the user decide.
    const QString duplicateKey = normalizeForDuplicateCheck(m_description->toPlainText());
    if (s_sentThisSession.contains(duplicateKey)) {
        const auto answer =
            QMessageBox::question(this, tr("Report a Bug"),
                                  tr("You already sent this exact report in this session.\n\nSend it again anyway?"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    const QByteArray payload = buildPayload();
    if (payload.size() > MaxPayloadBytes) {
        QMessageBox::information(this, tr("Report a Bug"),
                                 tr("This report is too large to send (%1 MB). Remove a screenshot or untick \"Include the launcher log\" and try again.")
                                     .arg(payload.size() / 1000000.0, 0, 'f', 1));
        return;
    }

    // A single plain POST: no NetJob, because NetJob retries failures up to 3 times (which trips the relay's rate limit and
    // can post duplicate reports) and hides the real error behind a generic "Aborted".
    QNetworkRequest request{ QUrl(BuildConfig.BUG_REPORT_API_URL) };
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, APPLICATION->getUserAgent().toUtf8());
    request.setTransferTimeout(30000);

    setBusy(true);

    QNetworkReply* reply = APPLICATION->network()->post(request, payload);
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);  // safe even if the dialog is closed while sending
    connect(reply, &QNetworkReply::finished, this, [this, reply, duplicateKey]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto obj = QJsonDocument::fromJson(reply->readAll()).object();
        const bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300 && obj.value("ok").toBool();

        setBusy(false);

        if (!ok) {
            // The relay sends a short human-readable "error" string; fall back to the network error.
            QString reason = obj.value("error").toString();
            if (reason.isEmpty())
                reason = status == 429 ? tr("Too many reports, please try again later.") : reply->errorString();
            // reportable = false: no "Report Bug" button on the error window of the report sender itself
            CustomMessageBox::selectable(this, tr("Failed to send the report"),
                                         tr("The report could not be sent (HTTP %1): %2").arg(status).arg(reason), QMessageBox::Critical,
                                         QMessageBox::Ok, QMessageBox::NoButton, nullptr, false)
                ->show();
            return;
        }

        s_lastSent = QDateTime::currentDateTime();
        m_sentThisSession = true;
        s_sentThisSession.insert(duplicateKey);
        const bool gaveContact = !m_contact->text().trimmed().isEmpty();
        const bool hasDiscord = !BuildConfig.DISCORD_URL.isEmpty();

        QString thanks = tr("Thank you! Your report is on its way to the PollyMC team, and we read every one.");
        if (hasDiscord) {
            thanks += gaveContact ? tr("\n\nWe'll use the contact you gave if we need to ask anything. Want faster help, or to follow what happens next? "
                                       "Come say hi on our Discord.")
                                  : tr("\n\nSince you sent this anonymously we can't write back. Want help with this problem, or to see what happens next? "
                                       "Come say hi on our Discord.");
        }

        QMessageBox sent(this);
        sent.setIcon(QMessageBox::Information);
        sent.setWindowTitle(tr("Report sent"));
        sent.setText(thanks);
        QPushButton* discordButton = hasDiscord ? sent.addButton(tr("Join our Discord"), QMessageBox::ActionRole) : nullptr;
        sent.addButton(tr("Close"), QMessageBox::AcceptRole);
        sent.exec();
        if (discordButton && sent.clickedButton() == discordButton)
            DesktopServices::openUrl(QUrl(BuildConfig.DISCORD_URL));
        accept();
    });
}
