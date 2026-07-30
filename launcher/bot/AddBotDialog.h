#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>

struct BotConfig {
    QString name;
    QString server;
    int port = 25565;
    QString version = "1.20.4";
    int loginType = 0; // 0 = Offline, 1 = Microsoft
    bool autoStart = true;
};

class AddBotDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddBotDialog(QWidget* parent = nullptr);

    BotConfig config() const;
    void setConfig(const BotConfig& cfg);

    static BotConfig showDialog(QWidget* parent, const BotConfig& defaults = {}, bool editing = false);

private:
    QLineEdit* m_name;
    QLineEdit* m_server;
    QSpinBox* m_port;
    QComboBox* m_version;
    QComboBox* m_loginType;
    QCheckBox* m_autoStart;
};
