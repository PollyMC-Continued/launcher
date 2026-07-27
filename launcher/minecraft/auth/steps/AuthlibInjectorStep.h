#pragma once
#include <QObject>

#include "minecraft/auth/AuthStep.h"
#include "net/NetJob.h"
#include "net/Upload.h"

class AuthlibInjectorStep : public AuthStep {
    Q_OBJECT

   public:
    explicit AuthlibInjectorStep(AccountData* data);
    virtual ~AuthlibInjectorStep() noexcept = default;

    void perform() override;
    QString describe() override;

   private slots:
    void onRequestDone(QByteArray* response);

   private:
    void authenticate();
    void refresh();

    Net::Upload::Ptr m_request;
    NetJob::Ptr m_task;
};
