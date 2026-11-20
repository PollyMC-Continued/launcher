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

    m_autoStart = new QCheckBox("Connect immediately", this);
    m_autoStart->setChecked(true);
    form->addRow("", m_autoStart);

    m_autoReconnect = new QCheckBox("Reconnect automatically", this);
    form->addRow("", m_autoReconnect);

    m_autoRespawn = new QCheckBox("Respawn automatically after death", this);
    form->addRow("", m_autoRespawn);

    m_antiAfk = new QSpinBox(this);
    m_antiAfk->setRange(0, 3600);
    m_antiAfk->setValue(0);
    m_antiAfk->setSpecialValueText("Off");
    form->addRow("Anti-AFK look interval (s):", m_antiAfk);

    m_startDelay = new QSpinBox(this);
    m_startDelay->setRange(0, 86400);
    m_startDelay->setValue(0);
    m_startDelay->setSpecialValueText("Immediate");
    form->addRow("Connect delay (s):", m_startDelay);

    main->addLayout(form);
    main->addSpacing(12);

    auto* buttons = new QDialogButtonBox(this);
    buttons->addButton(QDialogButtonBox::Cancel);
    auto* okButton = buttons->addButton(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    main->addWidget(buttons);

    // A bot must have a name; disabling OK makes "cancelled" unambiguous
    okButton->setEnabled(false);
    connect(m_name, &QLineEdit::textChanged, this,
            [okButton](const QString& text) { okButton->setEnabled(!text.trimmed().isEmpty()); });
}

BotConfig AddBotDialog::config() const
{
    BotConfig c;
    c.name = m_name->text().trimmed();
    c.server = m_server->text().trimmed();
    c.port = m_port->value();
    c.version = m_version->currentText().trimmed();
    c.autoStart = m_autoStart->isChecked();
    c.autoReconnect = m_autoReconnect->isChecked();
    c.autoRespawn = m_autoRespawn->isChecked();
    c.antiAfkSeconds = m_antiAfk->value();
    c.startDelaySeconds = m_startDelay->value();
    return c;
}

void AddBotDialog::setConfig(const BotConfig& cfg)
{
    m_name->setText(cfg.name);
    m_server->setText(cfg.server);
    m_port->setValue(cfg.port);
    m_version->setCurrentText(cfg.version);
    m_autoStart->setChecked(cfg.autoStart);
    m_autoReconnect->setChecked(cfg.autoReconnect);
    m_autoRespawn->setChecked(cfg.autoRespawn);
    m_antiAfk->setValue(cfg.antiAfkSeconds);
    m_startDelay->setValue(cfg.startDelaySeconds);
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
