#include "BotTerminalPage.h"
#include <QDateTime>
#include <QJsonObject>
#include <QScrollBar>

BotTerminalPage::BotTerminalPage(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    m_statusLabel = new QLabel("Bot server: stopped", this);
    m_statusLabel->setStyleSheet("color: #f87171; font-size: 12px;");

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(1000);
    m_log->setStyleSheet(
        "QPlainTextEdit { background: #111; color: #dde1e7; font-family: 'Courier New', monospace; font-size: 12px; border: 1px solid #333; padding: 8px; }"
    );

    auto* inputLayout = new QHBoxLayout();
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText("/join <server> [username] | /quit <username> | /list");
    m_input->setStyleSheet(
        "QLineEdit { background: #111; color: #dde1e7; font-family: 'Courier New', monospace; font-size: 12px; border: 1px solid #333; padding: 6px 8px; }"
    );

    m_sendBtn = new QPushButton("Send", this);
    m_sendBtn->setStyleSheet(
        "QPushButton { background: #a8ff78; color: #000; border: none; padding: 6px 16px; font-size: 12px; }"
        "QPushButton:hover { background: #c8ffaa; }"
    );

    inputLayout->addWidget(m_input);
    inputLayout->addWidget(m_sendBtn);

    layout->addWidget(m_statusLabel);
    layout->addWidget(m_log, 1);
    layout->addLayout(inputLayout);

    connect(m_sendBtn, &QPushButton::clicked, this, &BotTerminalPage::onSendCommand);
    connect(m_input, &QLineEdit::returnPressed, this, &BotTerminalPage::onSendCommand);

    m_bot = new BotProcess(this);
    connect(m_bot, &BotProcess::logMessage, this, &BotTerminalPage::appendLog);
    connect(m_bot, &BotProcess::errorMessage, this, &BotTerminalPage::appendError);
    connect(m_bot, &BotProcess::ready, this, &BotTerminalPage::onBotReady);
    connect(m_bot, &BotProcess::botConnected, this, &BotTerminalPage::onBotConnected);
    connect(m_bot, &BotProcess::processExited, this, &BotTerminalPage::onProcessExited);

    startBotServer();
}

BotTerminalPage::~BotTerminalPage() = default;

void BotTerminalPage::startBotServer()
{
    appendLog("Starting bot server...");
    m_bot->start();
}

void BotTerminalPage::onSendCommand()
{
    QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;
    m_input->clear();

    appendLog("> " + text);

    QStringList parts = text.split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return;

    QString cmd = parts[0].toLower();

    if (cmd == "/join") {
        if (parts.size() < 2) {
            appendLog("Usage: /join <server> [username]");
            return;
        }
        QString server = parts[1];
        QString username = (parts.size() > 2) ? parts[2] : ("Bot" + QString::number(QDateTime::currentSecsSinceEpoch() % 10000));
        QJsonObject p;
        p["server"] = server;
        p["username"] = username;
        m_bot->sendCommand("join", p);
    } else if (cmd == "/quit") {
        if (parts.size() < 2) {
            appendLog("Usage: /quit <username>");
            return;
        }
        QJsonObject p;
        p["username"] = parts[1];
        m_bot->sendCommand("quit", p);
    } else if (cmd == "/list") {
        m_bot->sendCommand("list");
    } else if (cmd == "/say") {
        QString rest = text.mid(5).trimmed();
        int idx = rest.indexOf(' ');
        if (idx < 0) {
            appendLog("Usage: /say <username> <message>");
            return;
        }
        QString username = rest.left(idx);
        QString message = rest.mid(idx + 1);
        QJsonObject p;
        p["username"] = username;
        p["message"] = message;
        m_bot->sendCommand("say", p);
    } else {
        appendLog("Unknown command: " + cmd);
    }
}

void BotTerminalPage::appendLog(const QString& text)
{
    m_log->appendPlainText(text);
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void BotTerminalPage::appendError(const QString& text)
{
    m_log->appendHtml("<span style='color:#f87171;'>[ERROR] " + text.toHtmlEscaped() + "</span>");
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void BotTerminalPage::onBotReady()
{
    m_statusLabel->setText("Bot server: running");
    m_statusLabel->setStyleSheet("color: #6ee7b7; font-size: 12px;");
    appendLog("Bot server ready. Use /join <server> [username] to connect.");
}

void BotTerminalPage::onBotConnected(const QString& username, const QString& server)
{
    appendLog(username + " connected to " + server);
}

void BotTerminalPage::onProcessExited(int code)
{
    m_statusLabel->setText("Bot server: exited (" + QString::number(code) + ")");
    m_statusLabel->setStyleSheet("color: #f87171; font-size: 12px;");
    appendLog("Bot server exited with code " + QString::number(code));
}
