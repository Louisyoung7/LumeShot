#include "platform/autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
QString autostartDirectory()
{
    QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (config.isEmpty())
        config = QDir::homePath() + QStringLiteral("/.config");
    return config + QStringLiteral("/autostart");
}

// Desktop Entry 的 Exec 值：路径可能含空格，统一用双引号包裹并转义
QString quoteExec(const QString& path)
{
    QString escaped = path;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

// deb 包里的二进制依赖 /usr/bin/lumeshot 启动脚本来设置 LD_LIBRARY_PATH
QString launchTarget()
{
    const QString appPath = QCoreApplication::applicationFilePath();
    const QString wrapper = QStringLiteral("/usr/bin/lumeshot");
    if (QFileInfo(appPath).absolutePath() == QStringLiteral("/opt/lumeshot/bin")
        && QFileInfo::exists(wrapper))
        return wrapper;
    return appPath;
}
} // namespace

QString Autostart::desktopFilePath()
{
    return autostartDirectory() + QStringLiteral("/lumeshot.desktop");
}

bool Autostart::isEnabled()
{
    return QFileInfo::exists(desktopFilePath());
}

bool Autostart::setEnabled(bool enabled)
{
    const QString path = desktopFilePath();

    if (!enabled) {
        if (!QFileInfo::exists(path))
            return true;
        return QFile::remove(path);
    }

    if (!QDir().mkpath(autostartDirectory()))
        return false;

    QString entry;
    entry += QStringLiteral("[Desktop Entry]\n");
    entry += QStringLiteral("Type=Application\n");
    entry += QStringLiteral("Name=LumeShot\n");
    entry += QStringLiteral("Comment=截图工具\n");
    entry += QStringLiteral("Exec=%1\n").arg(quoteExec(launchTarget()));
    entry += QStringLiteral("Icon=lumeshot\n");
    entry += QStringLiteral("Terminal=false\n");
    entry += QStringLiteral("X-GNOME-Autostart-enabled=true\n");
    entry += QStringLiteral("StartupNotify=false\n");

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    if (file.write(entry.toUtf8()) < 0)
        return false;
    return file.commit();
}
