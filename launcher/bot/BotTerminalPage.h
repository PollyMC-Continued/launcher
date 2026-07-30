#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

#include "BotProcess.h"

class BotTerminalPage : public QWidget {
    Q_OBJECT
public:
    explicit BotTerminalPage(QWidget* parent = nullptr);
    ~BotTerminalPage() override;

private slots:
    void onSendCommand();
    void appendLog(const QString& text);
    void appendError(const QString& text);
    void onBotReady();
    void onBotConnected(const QString& username, const QString& server);
    void onProcessExited(int code);

private:
    void startBotServer();

    BotProcess* m_bot = nullptr;
    QPlainTextEdit* m_log = nullptr;
    QLineEdit* m_input = nullptr;
    QPushButton* m_sendBtn = nullptr;
    QLabel* m_statusLabel = nullptr;
};
