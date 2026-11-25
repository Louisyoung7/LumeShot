// 模块 update：下载 deb 更新包，并交给 pkexec 弹出授权窗口安装。
#pragma once

#include <QObject>
#include <QString>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

class UpdateInstaller : public QObject {
    Q_OBJECT

public:
    explicit UpdateInstaller(QObject* parent = nullptr);
    ~UpdateInstaller() override;

    // 下载 release 里的 deb，下载完成后调用 pkexec 安装
    void installFromUrl(const QString& url);

    // 取消下载；安装阶段已有独立的授权窗口，不再处理
    void cancel();

signals:
    void downloadProgress(qint64 received, qint64 total);
    void installing();
    void finished(bool success, const QString& message);
    void failed(const QString& message);

private:
    void handleDownloadFinished();
    void startInstall();
    void cleanupDownload();
    void removeDownloadedFile();

    QNetworkAccessManager* m_network = nullptr;
    QNetworkReply* m_reply = nullptr;
    QProcess* m_process = nullptr;
    QFile* m_file = nullptr;
    QString m_targetPath;
    bool m_canceled = false;
    bool m_writeFailed = false;
};
