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
    void onAuthDone(QByteArray* response);
    void onProfileDone(QByteArray* response);

   private:
    void authenticate();
    void refresh();
    void fetchProfile();

    Net::Upload::Ptr m_upload;
    Net::Download::Ptr m_download;
    NetJob::Ptr m_task;
    NetJob::Ptr m_skinTask;
};
