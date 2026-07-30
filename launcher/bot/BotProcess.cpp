#include "BotProcess.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

BotProcess::BotProcess(QObject* parent)
    : QObject(parent), m_process(new QProcess(this))
{
    connect(m_process, &QProcess::readyReadStandardOutput, this, &BotProcess::onStdoutReady);
    connect(m_process, &QProcess::readyReadStandardError, this, &BotProcess::onStderrReady);
    connect(m_process, &QProcess::errorOccurred, this, &BotProcess::onProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &BotProcess::onProcessFinished);

    m_process->setProcessChannelMode(QProcess::SeparateChannels);
}

BotProcess::~BotProcess()
{
    stop();
}

QString BotProcess::findNodePath() const
{
#ifdef Q_OS_WIN
    return "node";
#else
    return "node";
#endif
}

QString BotProcess::findBotServerDir() const
{
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/bot-server",
        QCoreApplication::applicationDirPath() + "/../bot-server",
        QCoreApplication::applicationDirPath() + "/../../bot-server",
        QCoreApplication::applicationDirPath() + "/../../../bot-server",
    };

    for (const auto& path : candidates) {
        if (QDir(path).exists())
            return QFileInfo(path).absoluteFilePath();
    }

    return QCoreApplication::applicationDirPath() + "/bot-server";
}

void BotProcess::start()
{
    if (m_process->state() != QProcess::NotRunning)
        return;

    QString nodePath = findNodePath();
    QString serverDir = findBotServerDir();
    QString scriptPath = serverDir + "/index.js";

    if (!QFileInfo::exists(scriptPath)) {
        emit errorMessage("Bot server script not found: " + scriptPath);
        return;
    }

    m_process->setWorkingDirectory(serverDir);
    m_process->start(nodePath, { scriptPath });
}

void BotProcess::stop()
{
    if (m_process->state() == QProcess::NotRunning)
        return;
    m_process->kill();
    m_process->waitForFinished(3000);
}

bool BotProcess::isRunning() const
{
    return m_process->state() == QProcess::Running;
}

void BotProcess::sendCommand(const QString& cmd, const QJsonObject& params)
{
    if (!isRunning()) {
        emit errorMessage("Bot server not running");
        return;
    }
    QJsonObject msg = params;
    msg["cmd"] = cmd;
    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact) + "\n";
    m_process->write(data);
}

void BotProcess::onStdoutReady()
{
    m_buffer += QString::fromUtf8(m_process->readAllStandardOutput());
    while (m_buffer.contains('\n')) {
        int idx = m_buffer.indexOf('\n');
        QString line = m_buffer.left(idx).trimmed();
        m_buffer = m_buffer.mid(idx + 1);
        if (line.isEmpty()) continue;
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (doc.isObject())
            handleMessage(doc.object());
    }
}

void BotProcess::onStderrReady()
{
    QString err = QString::fromUtf8(m_process->readAllStandardError());
    if (!err.trimmed().isEmpty())
        emit errorMessage(err.trimmed());
}

void BotProcess::onProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    emit errorMessage("Process error: " + m_process->errorString());
}

void BotProcess::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status);
    emit processExited(exitCode);
}

void BotProcess::handleMessage(const QJsonObject& msg)
{
    QString event = msg["event"].toString();
    if (event == "ready")
        emit ready();
    else if (event == "log")
        emit logMessage(msg["text"].toString());
    else if (event == "connected")
        emit botConnected(msg["username"].toString(), msg["server"].toString());
    else if (event == "chat")
        emit botChat(msg["username"].toString(), msg["from"].toString(), msg["message"].toString());
    else if (event == "error")
        emit errorMessage(msg["text"].toString());
}
