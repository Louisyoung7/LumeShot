// 模块 update：检查 GitHub Release 是否有新版本，只负责网络请求与版本比对。
#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class UpdateChecker : public QObject {
    Q_OBJECT

public:
    explicit UpdateChecker(QObject* parent = nullptr);
    ~UpdateChecker() override;

    // 发起一次检查；已有请求进行中时忽略
    void check();

signals:
    void updateAvailable(const QString& version, const QString& notes,
                         const QString& releaseUrl, const QString& downloadUrl);
    void upToDate(const QString& version);
    void failed(const QString& message);

private:
    void handleReply(QNetworkReply* reply);

    QNetworkAccessManager* m_network = nullptr;
    QNetworkReply* m_reply = nullptr;
};
