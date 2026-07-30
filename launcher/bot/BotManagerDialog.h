#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QJsonObject>
#include <QVector>

#include "BotProcess.h"
#include "AddBotDialog.h"

struct BotEntry {
    BotConfig config;
    bool connected = false;
    int colorIndex = 0;
};

class BotManagerDialog : public QWidget {
    Q_OBJECT
public:
    explicit BotManagerDialog(QWidget* parent = nullptr);
    ~BotManagerDialog() override;

private slots:
    void onAddBot();
    void onEditBot();
    void onRemoveBot();
    void onStartAll();
    void onStopAll();
    void onTableDoubleClicked(int row, int column);
    void onSelectionChanged();
    void onSendCommand();
    void appendLog(const QString& text);
    void appendError(const QString& text);
    void onBotReady();
    void onBotConnected(const QString& username, const QString& server);
    void onBotChat(const QString& bot, const QString& from, const QString& message);
    void onProcessExited(int code);

private:
    void startBotServer();
    void saveConfigs();
    void loadConfigs();
    void refreshTable();
    void connectBot(int index);
    void disconnectBot(int index);
    BotEntry* currentBot();

    BotProcess* m_bot = nullptr;
    QVector<BotEntry> m_bots;

    QTableWidget* m_table;
    QPlainTextEdit* m_log;
    QLineEdit* m_input;
    QPushButton* m_sendBtn;
    QLabel* m_statusLabel;
    QPushButton* m_addBtn;
    QPushButton* m_editBtn;
    QPushButton* m_removeBtn;
    QPushButton* m_startAllBtn;
    QPushButton* m_stopAllBtn;

    QString m_configPath;
};
