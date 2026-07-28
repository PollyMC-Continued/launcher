#include "AuthlibInjectorStep.h"

#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>

#include "Application.h"
#include "Logging.h"
#include "net/Download.h"
#include "net/NetUtils.h"
#include "net/RawHeaderProxy.h"
#include "net/Upload.h"

AuthlibInjectorStep::AuthlibInjectorStep(AccountData* data) : AuthStep(data) {}

QString AuthlibInjectorStep::describe()
{
    return tr("Authenticating with Yggdrasil auth server");
}

void AuthlibInjectorStep::perform()
{
    if (m_data->authServerUrl.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT,
                      tr("No auth server URL set. Remove this account and re-add it with the correct URL."));
        return;
    }

    QString password = m_data->yggdrasilToken.extra.value("password").toString();
    if (password.isEmpty()) {
        refresh();
    } else {
        authenticate();
    }
}

void AuthlibInjectorStep::authenticate()
{
    QUrl url(m_data->authServerUrl + "/authserver/authenticate");

    QString username = m_data->yggdrasilToken.extra.value("userName").toString();
    QString password = m_data->yggdrasilToken.extra.value("password").toString();

    QJsonObject agent;
    agent["name"] = QString("Minecraft");
    agent["version"] = 1;

    QJsonObject req;
    req["agent"] = agent;
    req["username"] = username;
    req["password"] = password;
    req["clientToken"] = m_data->yggdrasilToken.extra.value("clientToken").toString();
    req["requestUser"] = false;

    QJsonDocument doc(req);
    auto requestBody = doc.toJson(QJsonDocument::Compact);

    auto headers = QList<Net::HeaderPair>{
        { "Content-Type", "application/json" },
        { "Accept", "application/json" },
    };

    auto [request, response] = Net::Upload::makeByteArray(url, requestBody);
    m_upload = request;
    m_upload->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_upload->enableAutoRetry(true);

    m_task.reset(new NetJob("AuthlibInjectorStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_upload);

    connect(m_task.get(), &Task::finished, this, [this, response] { onAuthDone(response); });

    m_task->start();
}

void AuthlibInjectorStep::refresh()
{
    QUrl url(m_data->authServerUrl + "/authserver/refresh");

    QJsonObject req;
    req["accessToken"] = m_data->yggdrasilToken.token;
    req["clientToken"] = m_data->yggdrasilToken.extra.value("clientToken").toString();
    req["requestUser"] = false;

    QJsonDocument doc(req);
    auto requestBody = doc.toJson(QJsonDocument::Compact);

    auto headers = QList<Net::HeaderPair>{
        { "Content-Type", "application/json" },
        { "Accept", "application/json" },
    };

    auto [request, response] = Net::Upload::makeByteArray(url, requestBody);
    m_upload = request;
    m_upload->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_upload->enableAutoRetry(true);

    m_task.reset(new NetJob("AuthlibInjectorStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_upload);

    connect(m_task.get(), &Task::finished, this, [this, response] { onAuthDone(response); });

    m_task->start();
}

void AuthlibInjectorStep::onAuthDone(QByteArray* response)
{
    if (m_upload->error() != QNetworkReply::NoError) {
        qWarning() << "Reply error:" << m_upload->error();
        if (Net::isApplicationError(m_upload->error())) {
            emit finished(AccountTaskState::STATE_FAILED_SOFT,
                          tr("Auth request failed: %1").arg(m_upload->errorString()));
        } else {
            emit finished(AccountTaskState::STATE_OFFLINE,
                          tr("Could not reach auth server: %1").arg(m_upload->errorString()));
        }
        return;
    }

    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(*response, &jsonError);
    if (jsonError.error != QJsonParseError::NoError) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Could not parse auth server response."));
        return;
    }

    auto obj = doc.object();

    auto error = obj.value("error").toString();
    auto errorMessage = obj.value("errorMessage").toString();
    if (!error.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT,
                      errorMessage.isEmpty() ? tr("Auth server error: %1").arg(error) : errorMessage);
        return;
    }

    auto accessToken = obj.value("accessToken").toString();
    auto clientToken = obj.value("clientToken").toString();
    auto profile = obj.value("selectedProfile").toObject();

    if (accessToken.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Auth server did not return an access token."));
        return;
    }
    if (profile.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Auth server did not return a profile."));
        return;
    }

    QString uuid = profile.value("id").toString();
    QString name = profile.value("name").toString();
    if (uuid.isEmpty() || name.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Auth server returned an incomplete profile."));
        return;
    }

    auto availableProfiles = obj.value("availableProfiles").toArray();
    if (availableProfiles.size() > 1) {
        QStringList items;
        int defaultIdx = 0;
        for (int i = 0; i < availableProfiles.size(); i++) {
            auto p = availableProfiles[i].toObject();
            items << p["name"].toString();
            if (p["id"].toString() == uuid)
                defaultIdx = i;
        }
        bool ok;
        QString chosen = QInputDialog::getItem(nullptr, tr("Choose Profile"),
            tr("Multiple profiles found. Select one:"), items, defaultIdx, false, &ok);
        if (ok) {
            int idx = items.indexOf(chosen);
            if (idx >= 0) {
                auto p = availableProfiles[idx].toObject();
                uuid = p["id"].toString();
                name = p["name"].toString();
            }
        }
    }

    m_data->yggdrasilToken.token = accessToken;
    m_data->yggdrasilToken.validity = Validity::Certain;
    m_data->yggdrasilToken.issueInstant = QDateTime::currentDateTimeUtc();
    m_data->yggdrasilToken.extra["clientToken"] = clientToken;
    m_data->yggdrasilToken.extra.remove("password");

    m_data->minecraftProfile.id = uuid;
    m_data->minecraftProfile.name = name;
    m_data->minecraftProfile.validity = Validity::Certain;
    m_data->minecraftEntitlement.canPlayMinecraft = true;
    m_data->minecraftEntitlement.ownsMinecraft = true;
    m_data->minecraftEntitlement.validity = Validity::Certain;

    fetchProfile();
}

void AuthlibInjectorStep::fetchProfile()
{
    QUrl url(m_data->authServerUrl + "/sessionserver/session/minecraft/profile/" + m_data->minecraftProfile.id);

    auto [request, response] = Net::Download::makeByteArray(url);
    m_download = request;
    m_download->enableAutoRetry(true);

    auto headers = QList<Net::HeaderPair>{
        { "Accept", "application/json" },
    };
    m_download->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));

    m_task.reset(new NetJob("AuthlibInjectorProfileStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_download);

    connect(m_task.get(), &Task::finished, this, [this, response] { onProfileDone(response); });

    m_task->start();
}

void AuthlibInjectorStep::onProfileDone(QByteArray* response)
{
    if (m_download->error() != QNetworkReply::NoError) {
        qWarning() << "Profile request error:" << m_download->error();
        emit finished(AccountTaskState::STATE_WORKING, tr("Authentication successful (no skin data)"));
        return;
    }

    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(*response, &jsonError);
    if (jsonError.error != QJsonParseError::NoError) {
        emit finished(AccountTaskState::STATE_WORKING, tr("Authentication successful (no skin data)"));
        return;
    }

    auto obj = doc.object();
    auto properties = obj.value("properties").toArray();

    for (const auto& prop : properties) {
        auto propObj = prop.toObject();
        if (propObj.value("name").toString() == "textures") {
            QByteArray decoded = QByteArray::fromBase64(propObj.value("value").toString().toUtf8());
            QJsonDocument textureDoc = QJsonDocument::fromJson(decoded);
            if (textureDoc.isObject()) {
                auto textures = textureDoc.object().value("textures").toObject();
                auto skin = textures.value("SKIN").toObject();
                QString skinUrl = skin.value("url").toString();
                if (!skinUrl.isEmpty()) {
                    m_data->minecraftProfile.skin.url = skinUrl;
                    m_data->minecraftProfile.skin.id = m_data->minecraftProfile.id;
                    m_data->minecraftProfile.skin.variant = "slim";

                    auto [skinRequest, skinResponse] = Net::Download::makeByteArray(QUrl(skinUrl));
                    auto skinDownload = skinRequest;
                    skinDownload->enableAutoRetry(true);

                    m_skinTask.reset(new NetJob("AuthlibInjectorSkinStep", APPLICATION->network()));
                    m_skinTask->setAskRetry(false);
                    m_skinTask->addNetAction(skinDownload);
                    connect(m_skinTask.get(), &Task::finished, this,
                            [this, skinResponse]() {
                                if (skinResponse)
                                    m_data->minecraftProfile.skin.data = *skinResponse;
                                emit finished(AccountTaskState::STATE_WORKING, tr("Authentication successful"));
                            });
                    m_skinTask->start();
                    return;
                }
            }
        }
    }

    emit finished(AccountTaskState::STATE_WORKING, tr("Authentication successful"));
}
