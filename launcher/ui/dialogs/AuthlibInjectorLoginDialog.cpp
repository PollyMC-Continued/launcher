#include "AuthlibInjectorLoginDialog.h"
#include "ui_AuthlibInjectorLoginDialog.h"

#include <QDebug>
#include <QDialogButtonBox>
#include <QPushButton>

AuthlibInjectorLoginDialog::AuthlibInjectorLoginDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::AuthlibInjectorLoginDialog)
{
    ui->setupUi(this);

    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(ui->loginButton, &QPushButton::clicked, this, [this] {
        ui->loginButton->setEnabled(false);
        ui->statusLabel->setText(tr("Logging in..."));

        auto serverUrl = ui->serverUrlEdit->text().trimmed();
        auto username = ui->usernameEdit->text().trimmed();
        auto password = ui->passwordEdit->text();

        if (serverUrl.isEmpty() || username.isEmpty() || password.isEmpty()) {
            ui->statusLabel->setText(tr("Please fill in all fields."));
            ui->loginButton->setEnabled(true);
            return;
        }

        m_account = MinecraftAccount::createAuthlibInjector(username, serverUrl);
        m_account->accountData()->yggdrasilToken.extra["password"] = password;

        m_authflow_task = m_account->login();
        connect(m_authflow_task.get(), &Task::succeeded, this, &AuthlibInjectorLoginDialog::accept);
        connect(m_authflow_task.get(), &Task::failed, this, &AuthlibInjectorLoginDialog::onTaskFailed);
        connect(m_authflow_task.get(), &Task::status, this, &AuthlibInjectorLoginDialog::onAuthFlowStatus);

        m_authflow_task->start();
    });

    auto enableLogin = [this] {
        ui->loginButton->setEnabled(
            !ui->serverUrlEdit->text().trimmed().isEmpty() &&
            !ui->usernameEdit->text().trimmed().isEmpty() &&
            !ui->passwordEdit->text().isEmpty());
    };
    connect(ui->serverUrlEdit, &QLineEdit::textChanged, this, enableLogin);
    connect(ui->usernameEdit, &QLineEdit::textChanged, this, enableLogin);
    connect(ui->passwordEdit, &QLineEdit::textChanged, this, enableLogin);
}

AuthlibInjectorLoginDialog::~AuthlibInjectorLoginDialog()
{
    delete ui;
}

MinecraftAccountPtr AuthlibInjectorLoginDialog::newAccount(QWidget* parent)
{
    AuthlibInjectorLoginDialog dlg(parent);
    if (dlg.exec() == QDialog::Accepted)
        return dlg.m_account;
    return nullptr;
}

int AuthlibInjectorLoginDialog::exec()
{
    return QDialog::exec();
}

void AuthlibInjectorLoginDialog::onTaskFailed(QString reason)
{
    qWarning() << "AuthlibInjector login failed:" << reason;
    ui->statusLabel->setText(reason);
    ui->loginButton->setEnabled(true);
    m_account.reset();
}

void AuthlibInjectorLoginDialog::onAuthFlowStatus(QString status)
{
    ui->statusLabel->setText(status);
}
