#include "update/updatechecker.h"

#include "update/releasemetadata.h"
#include "update/version.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {
constexpr auto kReleasesFeedUrl = "https://github.com/Louisyoung7/LumeShot/releases.atom";
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

    QNetworkRequest request{QUrl(QString::fromLatin1(kReleasesFeedUrl))};
    request.setRawHeader("Accept", "application/atom+xml");
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
        emit failed(status == 404 ? QStringLiteral("上游还没有发布过版本。") : errorText);
        return;
    }

    update::ReleaseInfo release;
    if (!update::parseAtomFeed(payload, &release)) {
        emit failed(QStringLiteral("更新信息解析失败。"));
        return;
    }

    if (update::isNewerVersion(release.version, QCoreApplication::applicationVersion())) {
        emit updateAvailable(release.version, release.notes, release.releaseUrl,
                             update::debDownloadUrl(release.version));
    } else {
        emit upToDate(release.version);
    }
}
