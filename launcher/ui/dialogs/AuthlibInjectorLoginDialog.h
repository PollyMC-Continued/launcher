#pragma once

#include <QtWidgets/QDialog>

#include "minecraft/auth/AuthFlow.h"
#include "minecraft/auth/MinecraftAccount.h"

namespace Ui {
class AuthlibInjectorLoginDialog;
}

class AuthlibInjectorLoginDialog : public QDialog {
    Q_OBJECT

   public:
    ~AuthlibInjectorLoginDialog();
    static MinecraftAccountPtr newAccount(QWidget* parent);
    int exec() override;

   private:
    explicit AuthlibInjectorLoginDialog(QWidget* parent = 0);

   protected slots:
    void onTaskFailed(QString reason);
    void onAuthFlowStatus(QString status);

   private:
    Ui::AuthlibInjectorLoginDialog* ui;
    MinecraftAccountPtr m_account;
    shared_qobject_ptr<AuthFlow> m_authflow_task;
};
