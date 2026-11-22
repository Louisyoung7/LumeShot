#include "update/updatechecker.h"

#include "update/version.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {
constexpr auto kLatestReleaseUrl =
    "https://api.github.com/repos/Louisyoung7/LumeShot/releases/latest";
}

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

UpdateChecker::~UpdateChecker()
{
    if (m_reply) {
        // 先断开再中止，避免析构过程中回调到正在销毁的对话框
        m_reply->disconnect(this);
        m_reply->abort();
    }
}

void UpdateChecker::check()
{
    if (m_reply)
        return;

    QNetworkRequest request{QUrl(QString::fromLatin1(kLatestReleaseUrl))};
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("LumeShot/%1")
                          .arg(QCoreApplication::applicationVersion()));
    request.setTransferTimeout(10000);

    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this] { handleReply(m_reply); });
}

void UpdateChecker::handleReply(QNetworkReply* reply)
{
    if (!reply)
        return;

    const QByteArray payload = reply->readAll();
    const QNetworkReply::NetworkError error = reply->error();
    const QString errorText = reply->errorString();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    m_reply = nullptr;

    if (error != QNetworkReply::NoError) {
        // 仓库尚未发布任何 Release 时 GitHub 返回 404
        emit failed(status == 404 ? QStringLiteral("上游还没有发布过版本。") : errorText);
        return;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit failed(QStringLiteral("更新信息解析失败。"));
        return;
    }

    const QJsonObject release = document.object();
    QString version = release.value(QStringLiteral("tag_name")).toString();
    if (version.isEmpty()) {
        emit failed(QStringLiteral("更新信息里缺少版本号。"));
        return;
    }
    if (version.startsWith(QLatin1Char('v')))
        version.remove(0, 1);

    if (update::isNewerVersion(version, QCoreApplication::applicationVersion())) {
        QString downloadUrl;
        const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue& value : assets) {
            const QJsonObject asset = value.toObject();
            if (asset.value(QStringLiteral("name")).toString().endsWith(QLatin1String(".deb"))) {
                downloadUrl = asset.value(QStringLiteral("browser_download_url")).toString();
                break;
            }
        }

        emit updateAvailable(version,
                             release.value(QStringLiteral("body")).toString(),
                             release.value(QStringLiteral("html_url")).toString(),
                             downloadUrl);
    } else {
        emit upToDate(version);
    }
}
