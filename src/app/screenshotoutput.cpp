#include "app/screenshotoutput.h"

#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QPixmap>
#include <QStringList>

ScreenshotOutput::ScreenshotOutput(QObject* parent)
    : QObject(parent)
{
}

void ScreenshotOutput::setSettings(const AppSettings& settings)
{
    m_settings = settings;
}

void ScreenshotOutput::deliver(const QPixmap& capture, const QString& title)
{
    if (capture.isNull())
        return;

    QStringList results;

    if (m_settings.autoCopy) {
        QGuiApplication::clipboard()->setPixmap(capture);
        results << QStringLiteral("已复制到剪贴板");
    }

    if (m_settings.autoSave) {
        const QDir directory(m_settings.effectiveSaveDirectory());
        if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
            results << QStringLiteral("保存失败：无法创建目录 %1").arg(directory.path());
        } else {
            const QString path = directory.filePath(
                QStringLiteral("Screenshot_%1.png")
                    .arg(QDateTime::currentDateTime().toString(
                        QStringLiteral("yyyyMMdd_HHmmss_zzz"))));
            if (capture.save(path, "PNG")) {
                qInfo("Saved screenshot: %s", qUtf8Printable(path));
                results << QStringLiteral("已保存：%1").arg(path);
            } else {
                results << QStringLiteral("保存失败：%1").arg(path);
            }
        }
    }

    if (results.isEmpty())
        results << QStringLiteral("未开启自动复制或自动保存");

    const QSize logicalSize = capture.size() / capture.devicePixelRatio();
    emit messageReady(title, QStringLiteral("%1 × %2\n%3")
                                 .arg(logicalSize.width())
                                 .arg(logicalSize.height())
                                 .arg(results.join(QLatin1Char('\n'))));
}
