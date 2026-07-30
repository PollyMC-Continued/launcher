#include "AddBotDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>

AddBotDialog::AddBotDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Add Bot");
    setMinimumWidth(400);

    auto* main = new QVBoxLayout(this);

    auto* form = new QFormLayout();

    m_name = new QLineEdit(this);
    m_name->setPlaceholderText("Bot1");
    form->addRow("Bot Name:", m_name);

    m_server = new QLineEdit(this);
    m_server->setPlaceholderText("hypixel.net");
    form->addRow("Server IP:", m_server);

    m_port = new QSpinBox(this);
    m_port->setRange(1, 65535);
    m_port->setValue(25565);
    form->addRow("Port:", m_port);

    m_version = new QComboBox(this);
    m_version->setEditable(true);
    m_version->addItems({"1.21", "1.20.4", "1.20.1", "1.19.4", "1.18.2", "1.17.1", "1.16.5", "1.12.2"});
    form->addRow("Version:", m_version);

    m_loginType = new QComboBox(this);
    m_loginType->addItems({"Offline (cracked)", "Microsoft account"});
    form->addRow("Login:", m_loginType);

    m_autoStart = new QCheckBox("Connect immediately", this);
    m_autoStart->setChecked(true);
    form->addRow("", m_autoStart);

    main->addLayout(form);
    main->addSpacing(12);

    auto* buttons = new QDialogButtonBox(this);
    buttons->addButton(QDialogButtonBox::Cancel);
    buttons->addButton(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    main->addWidget(buttons);
}

BotConfig AddBotDialog::config() const
{
    BotConfig c;
    c.name = m_name->text().trimmed();
    c.server = m_server->text().trimmed();
    c.port = m_port->value();
    c.version = m_version->currentText().trimmed();
    c.loginType = m_loginType->currentIndex();
    c.autoStart = m_autoStart->isChecked();
    return c;
}

void AddBotDialog::setConfig(const BotConfig& cfg)
{
    m_name->setText(cfg.name);
    m_server->setText(cfg.server);
    m_port->setValue(cfg.port);
    m_version->setCurrentText(cfg.version);
    m_loginType->setCurrentIndex(cfg.loginType);
    m_autoStart->setChecked(cfg.autoStart);
}

BotConfig AddBotDialog::showDialog(QWidget* parent, const BotConfig& defaults, bool editing)
{
    AddBotDialog dlg(parent);
    if (editing)
        dlg.setWindowTitle("Edit Bot");
    if (!defaults.name.isEmpty())
        dlg.setConfig(defaults);
    if (dlg.exec() == QDialog::Accepted)
        return dlg.config();
    return {};
}
