#include "AuthlibInjectorStep.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>

#include "Application.h"
#include "Logging.h"
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
    QString password = m_data->yggdrasilToken.extra.value("password").toString();
    if (password.isEmpty()) {
        refresh();
    } else {
        authenticate();
    }
}

void AuthlibInjectorStep::authenticate()
{
    auto baseUrl = m_data->authServerUrl;
    if (baseUrl.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("No auth server URL set."));
        return;
    }
    QUrl url(baseUrl + "/authserver/authenticate");
    if (!url.isValid()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Invalid auth server URL."));
        return;
    }

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
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_request->enableAutoRetry(true);

    m_task.reset(new NetJob("AuthlibInjectorStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
    qDebug() << "AuthlibInjectorStep: authenticating with" << url.toString();
}

void AuthlibInjectorStep::refresh()
{
    auto baseUrl = m_data->authServerUrl;
    if (baseUrl.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("No auth server URL set."));
        return;
    }
    QUrl url(baseUrl + "/authserver/refresh");
    if (!url.isValid()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Invalid auth server URL."));
        return;
    }

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
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_request->enableAutoRetry(true);

    m_task.reset(new NetJob("AuthlibInjectorStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
    qDebug() << "AuthlibInjectorStep: refreshing with" << url.toString();
}

void AuthlibInjectorStep::onRequestDone(QByteArray* response)
{
    qCDebug(authCredentials()) << *response;
    if (m_request->error() != QNetworkReply::NoError) {
        qWarning() << "Reply error:" << m_request->error();
        if (Net::isApplicationError(m_request->error())) {
            emit finished(AccountTaskState::STATE_FAILED_SOFT,
                          tr("Auth request failed: %1").arg(m_request->errorString()));
        } else {
            emit finished(AccountTaskState::STATE_OFFLINE,
                          tr("Could not reach auth server: %1").arg(m_request->errorString()));
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

    emit finished(AccountTaskState::STATE_WORKING, tr("Authentication successful"));
}
