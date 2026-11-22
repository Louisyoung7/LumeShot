#include "update/updateinstaller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QUrl>

UpdateInstaller::UpdateInstaller(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

UpdateInstaller::~UpdateInstaller()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
    }
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
    }
    cleanupDownload();
}

void UpdateInstaller::installFromUrl(const QString& url)
{
    if (m_reply || m_process)
        return;

    const QUrl source(url);
    QString fileName = QFileInfo(source.path()).fileName();
    if (!fileName.endsWith(QLatin1String(".deb")))
        fileName = QStringLiteral("lumeshot-update.deb");

    m_targetPath = QDir(QDir::tempPath()).filePath(fileName);
    m_canceled = false;

    m_file = new QFile(m_targetPath, this);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit failed(QStringLiteral("无法写入临时文件：\n%1").arg(m_targetPath));
        cleanupDownload();
        return;
    }

    QNetworkRequest request{source};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("LumeShot/%1")
                          .arg(QCoreApplication::applicationVersion()));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(120000);

    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (m_file && m_reply && m_reply->isOpen())
            m_file->write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this, &UpdateInstaller::downloadProgress);
    connect(m_reply, &QNetworkReply::finished, this, &UpdateInstaller::handleDownloadFinished);
}

void UpdateInstaller::cancel()
{
    m_canceled = true;
    if (m_reply)
        m_reply->abort();
}

void UpdateInstaller::handleDownloadFinished()
{
    if (!m_reply)
        return;

    if (m_file) {
        if (m_reply->isOpen())
            m_file->write(m_reply->readAll());
        m_file->close();
    }

    const QNetworkReply::NetworkError error = m_reply->error();
    const QString errorText = m_reply->errorString();
    m_reply->deleteLater();
    m_reply = nullptr;

    if (m_canceled) {
        cleanupDownload();
        removeDownloadedFile();
        return;
    }

    if (error != QNetworkReply::NoError) {
        cleanupDownload();
        removeDownloadedFile();
        emit failed(QStringLiteral("下载安装包失败：\n%1").arg(errorText));
        return;
    }

    if (!m_file || m_file->size() == 0) {
        cleanupDownload();
        removeDownloadedFile();
        emit failed(QStringLiteral("下载的安装包为空。"));
        return;
    }

    startInstall();
}

void UpdateInstaller::startInstall()
{
    const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    if (pkexec.isEmpty()) {
        cleanupDownload();
        emit failed(QStringLiteral("未找到 pkexec，无法弹出授权窗口。\n"
                                   "请安装 polkit，或手动安装下载好的包：\n%1")
                        .arg(m_targetPath));
        return;
    }

    emit installing();

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("DEBIAN_FRONTEND"), QStringLiteral("noninteractive"));
    environment.insert(QStringLiteral("APT_LISTCHANGES_FRONTEND"), QStringLiteral("none"));

    m_process = new QProcess(this);
    m_process->setProcessEnvironment(environment);
    m_process->setProgram(pkexec);
    // pkexec 会清掉大部分环境变量，用 env 显式带上非交互设置
    m_process->setArguments({QStringLiteral("env"),
                             QStringLiteral("DEBIAN_FRONTEND=noninteractive"),
                             QStringLiteral("APT_LISTCHANGES_FRONTEND=none"),
                             QStringLiteral("apt-get"), QStringLiteral("install"),
                             QStringLiteral("-y"), m_targetPath});

    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_process)
            return;
        const QString message = QStringLiteral("无法启动安装程序：\n%1").arg(m_process->errorString());
        m_process->deleteLater();
        m_process = nullptr;
        cleanupDownload();
        emit failed(message);
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus status) {
                const QByteArray output = m_process->readAllStandardError()
                                              + m_process->readAllStandardOutput();
                m_process->deleteLater();
                m_process = nullptr;
                cleanupDownload();

                if (status != QProcess::NormalExit || exitCode != 0) {
                    QString detail = QString::fromLocal8Bit(output).trimmed();
                    if (detail.isEmpty() && (exitCode == 126 || exitCode == 127))
                        detail = QStringLiteral("授权被取消或未通过。");
                    emit finished(false, detail.isEmpty()
                                             ? QStringLiteral("安装失败（退出码 %1）。").arg(exitCode)
                                             : detail);
                    return;
                }

                removeDownloadedFile();
                emit finished(true, QStringLiteral("新版本已安装，重启 LumeShot 后生效。"));
            });

    m_process->start();
}

void UpdateInstaller::cleanupDownload()
{
    delete m_file;
    m_file = nullptr;
}

void UpdateInstaller::removeDownloadedFile()
{
    if (!m_targetPath.isEmpty())
        QFile::remove(m_targetPath);
}
