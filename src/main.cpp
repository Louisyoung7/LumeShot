#include "app/appcontroller.h"
#include "log/filelogger.h"

#include <QApplication>
#include <QDebug>
#include <QMessageBox>

#include <cstdio>
#include <cstring>

int main(int argc, char* argv[])
{
    // 在创建 QApplication 之前处理，保证无显示环境下也能打印版本
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--version") == 0) {
            std::printf("LumeShot %s\n", APP_VERSION);
            return 0;
        }
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("LumeShot"));
    QApplication::setOrganizationName(QStringLiteral("LumeShot"));
    QApplication::setApplicationDisplayName(QStringLiteral("LumeShot"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setQuitOnLastWindowClosed(false);
    FileLogger::install();

    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        qWarning() << "Wayland platform detected; LumeShot requires X11.";
        QMessageBox::critical(
            nullptr, QStringLiteral("仅支持 X11"),
            QStringLiteral("本工具依赖 X11 的抓屏与全局热键接口。\n"
                           "请在 X11 会话中运行，或使用 QT_QPA_PLATFORM=xcb 启动。"));
        return 1;
    }

    AppController controller;
    if (!controller.start()) {
        qWarning() << "LumeShot failed to start: tray and global hotkey are both unavailable.";
        QMessageBox::warning(nullptr, QStringLiteral("LumeShot"),
                             QStringLiteral("系统托盘和全局热键均不可用，程序无法运行。"));
        return 1;
    }

    return app.exec();
}
