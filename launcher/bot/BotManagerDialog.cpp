#include "BotManagerDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QDateTime>
#include <QScrollBar>
#include <QMessageBox>

static const char* BOT_COLORS[] = {
    "#6ee7b7", "#60a5fa", "#fbbf24", "#f472b6",
    "#a78bfa", "#34d399", "#f87171", "#38bdf8"
};

BotManagerDialog::BotManagerDialog(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle("Bot Manager");
    resize(960, 600);

    m_configPath = QCoreApplication::applicationDirPath() + "/bots.json";

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);

    auto* toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(12, 8, 12, 8);

    m_addBtn = new QPushButton("+ Add Bot", this);
    m_editBtn = new QPushButton("Edit", this);
    m_removeBtn = new QPushButton("Remove", this);
    m_startAllBtn = new QPushButton("Start All", this);
    m_stopAllBtn = new QPushButton("Stop All", this);

    m_editBtn->setEnabled(false);
    m_removeBtn->setEnabled(false);

    toolbar->addWidget(m_addBtn);
    toolbar->addWidget(m_editBtn);
    toolbar->addWidget(m_removeBtn);
    toolbar->addSpacing(16);
    toolbar->addWidget(m_startAllBtn);
    toolbar->addWidget(m_stopAllBtn);
    toolbar->addStretch();

    m_statusLabel = new QLabel("Bot server: stopped", this);
    m_statusLabel->setStyleSheet("color: #f87171; padding: 0 12px; font-size: 12px;");
    toolbar->addWidget(m_statusLabel);

    main->addLayout(toolbar);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels({"#", "Name", "Server", "Port", "Version", "Status"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->setMinimumWidth(420);
    m_table->setStyleSheet(
        "QTableWidget { background: #1a1a1a; color: #dde1e7; border: 1px solid #333; font-size: 12px; gridline-color: #2a2a2a; }"
        "QTableWidget::item { padding: 4px 8px; }"
        "QTableWidget::item:selected { background: #2d5a3d; }"
        "QHeaderView::section { background: #222; color: #999; border: 1px solid #333; padding: 4px 8px; font-weight: bold; }"
    );

    splitter->addWidget(m_table);

    auto* rightPanel = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);
    m_log->setStyleSheet(
        "QPlainTextEdit { background: #111; color: #dde1e7; font-family: 'Courier New', monospace; font-size: 12px; border: 1px solid #333; padding: 8px; }"
    );
    rightLayout->addWidget(m_log, 1);

    auto* inputBar = new QHBoxLayout();
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText("Type a command... (targets selected bot)");
    m_input->setStyleSheet(
        "QLineEdit { background: #111; color: #dde1e7; font-family: 'Courier New', monospace; font-size: 12px; border: 1px solid #333; padding: 6px 8px; }"
    );
    m_sendBtn = new QPushButton("Send", this);
    m_sendBtn->setStyleSheet(
        "QPushButton { background: #a8ff78; color: #000; border: none; padding: 6px 16px; font-size: 12px; }"
        "QPushButton:hover { background: #c8ffaa; }"
    );
    inputBar->addWidget(m_input);
    inputBar->addWidget(m_sendBtn);
    rightLayout->addLayout(inputBar);

    splitter->addWidget(rightPanel);
    splitter->setSizes({420, 540});

    main->addWidget(splitter, 1);

    // Wire toolbar
    connect(m_addBtn, &QPushButton::clicked, this, &BotManagerDialog::onAddBot);
    connect(m_editBtn, &QPushButton::clicked, this, &BotManagerDialog::onEditBot);
    connect(m_removeBtn, &QPushButton::clicked, this, &BotManagerDialog::onRemoveBot);
    connect(m_startAllBtn, &QPushButton::clicked, this, &BotManagerDialog::onStartAll);
    connect(m_stopAllBtn, &QPushButton::clicked, this, &BotManagerDialog::onStopAll);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &BotManagerDialog::onSelectionChanged);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &BotManagerDialog::onTableDoubleClicked);

    // Wire input
    connect(m_sendBtn, &QPushButton::clicked, this, &BotManagerDialog::onSendCommand);
    connect(m_input, &QLineEdit::returnPressed, this, &BotManagerDialog::onSendCommand);

    // Bot process
    m_bot = new BotProcess(this);
    connect(m_bot, &BotProcess::logMessage, this, &BotManagerDialog::appendLog);
    connect(m_bot, &BotProcess::errorMessage, this, &BotManagerDialog::appendError);
    connect(m_bot, &BotProcess::ready, this, &BotManagerDialog::onBotReady);
    connect(m_bot, &BotProcess::botConnected, this, &BotManagerDialog::onBotConnected);
    connect(m_bot, &BotProcess::botChat, this, &BotManagerDialog::onBotChat);
    connect(m_bot, &BotProcess::processExited, this, &BotManagerDialog::onProcessExited);

    loadConfigs();
    refreshTable();
    startBotServer();
}

BotManagerDialog::~BotManagerDialog()
{
    saveConfigs();
}

void BotManagerDialog::startBotServer()
{
    appendLog("Starting bot server...");
    m_bot->start();
}

void BotManagerDialog::saveConfigs()
{
    QJsonArray arr;
    for (const auto& e : m_bots) {
        QJsonObject obj;
        obj["name"] = e.config.name;
        obj["server"] = e.config.server;
        obj["port"] = e.config.port;
        obj["version"] = e.config.version;
        obj["loginType"] = e.config.loginType;
        obj["autoStart"] = e.config.autoStart;
        arr.append(obj);
    }
    QFile f(m_configPath);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

void BotManagerDialog::loadConfigs()
{
    m_bots.clear();
    QFile f(m_configPath);
    if (!f.open(QIODevice::ReadOnly))
        return;
    auto arr = QJsonDocument::fromJson(f.readAll()).array();
    for (const auto& v : arr) {
        auto obj = v.toObject();
        BotEntry e;
        e.config.name = obj["name"].toString();
        e.config.server = obj["server"].toString();
        e.config.port = obj["port"].toInt(25565);
        e.config.version = obj["version"].toString("1.20.4");
        e.config.loginType = obj["loginType"].toInt(0);
        e.config.autoStart = obj["autoStart"].toBool(true);
        e.connected = false;
        e.colorIndex = m_bots.size() % 8;
        m_bots.append(e);
    }
}

void BotManagerDialog::refreshTable()
{
    m_table->setRowCount(0);
    for (int i = 0; i < m_bots.size(); i++) {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        auto* numItem = new QTableWidgetItem(QString::number(i + 1));
        numItem->setFlags(numItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 0, numItem);

        auto* nameItem = new QTableWidgetItem(m_bots[i].config.name);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 1, nameItem);

        auto* serverItem = new QTableWidgetItem(m_bots[i].config.server);
        serverItem->setFlags(serverItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 2, serverItem);

        auto* portItem = new QTableWidgetItem(QString::number(m_bots[i].config.port));
        portItem->setFlags(portItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 3, portItem);

        auto* verItem = new QTableWidgetItem(m_bots[i].config.version);
        verItem->setFlags(verItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 4, verItem);

        QString status = m_bots[i].connected ? "Online" : "Offline";
        auto* statusItem = new QTableWidgetItem(status);
        statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
        statusItem->setForeground(m_bots[i].connected ? QColor("#6ee7b7") : QColor("#f87171"));
        m_table->setItem(row, 5, statusItem);
    }
}

BotEntry* BotManagerDialog::currentBot()
{
    int row = m_table->currentRow();
    if (row < 0 || row >= m_bots.size())
        return nullptr;
    return &m_bots[row];
}

void BotManagerDialog::onAddBot()
{
    BotConfig cfg = AddBotDialog::showDialog(this);
    if (cfg.name.isEmpty()) return;

    BotEntry e;
    e.config = cfg;
    e.colorIndex = m_bots.size() % 8;
    m_bots.append(e);
    refreshTable();
    saveConfigs();

    if (e.config.autoStart)
        connectBot(m_bots.size() - 1);
}

void BotManagerDialog::onEditBot()
{
    auto* entry = currentBot();
    if (!entry) return;

    BotConfig cfg = AddBotDialog::showDialog(this, entry->config, true);
    if (cfg.name.isEmpty()) return;

    entry->config = cfg;
    refreshTable();
    saveConfigs();
}

void BotManagerDialog::onRemoveBot()
{
    int row = m_table->currentRow();
    if (row < 0 || row >= m_bots.size()) return;

    if (m_bots[row].connected)
        disconnectBot(row);

    m_bots.removeAt(row);
    refreshTable();
    saveConfigs();
}

void BotManagerDialog::onStartAll()
{
    for (int i = 0; i < m_bots.size(); i++)
        connectBot(i);
}

void BotManagerDialog::onStopAll()
{
    QJsonObject p;
    m_bot->sendCommand("quit_all", p);
    for (auto& e : m_bots)
        e.connected = false;
    refreshTable();
}

void BotManagerDialog::onTableDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row < 0 || row >= m_bots.size()) return;
    onEditBot();
}

void BotManagerDialog::onSelectionChanged()
{
    bool has = m_table->currentRow() >= 0;
    m_editBtn->setEnabled(has);
    m_removeBtn->setEnabled(has);
}

void BotManagerDialog::connectBot(int index)
{
    if (index < 0 || index >= m_bots.size()) return;
    auto& e = m_bots[index];

    QJsonObject p;
    p["server"] = e.config.server;
    p["port"] = e.config.port;
    p["username"] = e.config.name;
    p["version"] = e.config.version;
    p["loginType"] = e.config.loginType;
    m_bot->sendCommand("join", p);

    appendLog(QString("<span style='color:%1'>[%2] Connecting to %3...</span>")
        .arg(BOT_COLORS[e.colorIndex % 8], e.config.name.toHtmlEscaped(), e.config.server.toHtmlEscaped()));
}

void BotManagerDialog::disconnectBot(int index)
{
    if (index < 0 || index >= m_bots.size()) return;
    auto& e = m_bots[index];
    if (!e.connected) return;

    QJsonObject p;
    p["username"] = e.config.name;
    m_bot->sendCommand("quit", p);
    e.connected = false;
    refreshTable();
}

void BotManagerDialog::onSendCommand()
{
    QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;
    m_input->clear();

    m_log->appendHtml("<span style='color:#aaa;'>> " + text.toHtmlEscaped() + "</span>");
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());

    QStringList parts = text.split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return;
    QString cmd = parts[0].toLower();

    auto* entry = currentBot();
    QString targetName = entry ? entry->config.name : QString();

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
        for (auto& e : m_bots) {
            if (e.config.name == parts[1]) {
                e.connected = false;
                break;
            }
        }
        refreshTable();
    } else if (cmd == "/list") {
        m_bot->sendCommand("list");
    } else if (cmd == "/say") {
        if (!entry) { appendLog("Select a bot first"); return; }
        QString msg = text.mid(5).trimmed();
        QJsonObject p;
        p["username"] = targetName;
        p["message"] = msg;
        m_bot->sendCommand("say", p);
    } else if (cmd == "/goto" || cmd == "/follow" || cmd == "/come" || cmd == "/mine" || cmd == "/stop") {
        if (!entry) { appendLog("Select a bot first"); return; }
        QString rest = text.mid(cmd.length()).trimmed();
        QJsonObject p;
        p["username"] = targetName;
        if (!rest.isEmpty()) p["arg"] = rest;
        m_bot->sendCommand(cmd.mid(1), p);
    } else {
        m_log->appendHtml("<span style='color:#f87171;'>Unknown command: " + cmd.toHtmlEscaped() + "</span>");
        m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
    }
}

void BotManagerDialog::appendLog(const QString& text)
{
    m_log->appendHtml(text);
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void BotManagerDialog::appendError(const QString& text)
{
    m_log->appendHtml("<span style='color:#f87171;'>[ERROR] " + text.toHtmlEscaped() + "</span>");
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void BotManagerDialog::onBotReady()
{
    m_statusLabel->setText("Bot server: running");
    m_statusLabel->setStyleSheet("color: #6ee7b7; padding: 0 12px; font-size: 12px;");
    appendLog("Bot server ready.");
}

void BotManagerDialog::onBotConnected(const QString& username, const QString& server)
{
    for (auto& e : m_bots) {
        if (e.config.name == username) {
            e.connected = true;
            break;
        }
    }
    refreshTable();
    appendLog(QString("<span style='color:#6ee7b7;'>[%1] Connected to %2</span>")
        .arg(username.toHtmlEscaped(), server.toHtmlEscaped()));
}

void BotManagerDialog::onBotChat(const QString& bot, const QString& from, const QString& message)
{
    appendLog(QString("<span style='color:#fbbf24;'>[%1] %2: %3</span>")
        .arg(bot.toHtmlEscaped(), from.toHtmlEscaped(), message.toHtmlEscaped()));
}

void BotManagerDialog::onProcessExited(int code)
{
    m_statusLabel->setText("Bot server: exited (" + QString::number(code) + ")");
    m_statusLabel->setStyleSheet("color: #f87171; padding: 0 12px; font-size: 12px;");
    appendLog("Bot server exited with code " + QString::number(code));
    for (auto& e : m_bots)
        e.connected = false;
    refreshTable();
}
