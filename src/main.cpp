#include "app/appcontroller.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ScreenShot"));
    QApplication::setOrganizationName(QStringLiteral("ScreenShot"));
    QApplication::setApplicationDisplayName(QStringLiteral("ScreenShot"));
    QApplication::setQuitOnLastWindowClosed(false);

    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        QMessageBox::critical(
            nullptr, QStringLiteral("仅支持 X11"),
            QStringLiteral("本工具依赖 X11 的抓屏与全局热键接口。\n"
                           "请在 X11 会话中运行，或使用 QT_QPA_PLATFORM=xcb 启动。"));
        return 1;
    }

    AppController controller;
    if (!controller.start()) {
        QMessageBox::warning(nullptr, QStringLiteral("ScreenShot"),
                             QStringLiteral("系统托盘和全局热键均不可用，程序无法运行。"));
        return 1;
    }

    return app.exec();
}
