#include "ui/tray/traycontroller.h"

#include <QAction>
#include <QCoreApplication>
#include <QDebug>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QVBoxLayout>
#include <QWidget>

namespace {
QIcon createTrayIcon()
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(0x33, 0xaa, 0xff), 5));
    painter.drawRoundedRect(QRectF(10, 18, 44, 36), 6, 6);
    painter.drawEllipse(QRectF(25, 24, 14, 14));
    painter.drawLine(QPointF(24, 12), QPointF(40, 12));
    return QIcon(pixmap);
}
} // namespace

TrayController::TrayController(QObject* parent)
    : QObject(parent)
{
}

bool TrayController::start()
{
    const bool trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();

    if (trayAvailable) {
        m_tray = new QSystemTrayIcon(createTrayIcon(), this);
        m_tray->setToolTip(QStringLiteral("LumeShot 截屏工具"));

        auto* menu = new QMenu();
        m_regionAction = menu->addAction(QString());
        QAction* fullScreenAction = menu->addAction(QStringLiteral("全屏截图"));
        QAction* windowAction = menu->addAction(QStringLiteral("当前窗口截图"));
        menu->addSeparator();
        QAction* folderAction = menu->addAction(QStringLiteral("打开截图目录"));
        QAction* settingsAction = menu->addAction(QStringLiteral("设置…"));
        menu->addSeparator();
        QAction* quitAction = menu->addAction(QStringLiteral("退出"));

        connect(m_regionAction, &QAction::triggered, this, &TrayController::regionRequested);
        connect(fullScreenAction, &QAction::triggered, this,
                &TrayController::fullScreenRequested);
        connect(windowAction, &QAction::triggered, this,
                &TrayController::activeWindowRequested);
        connect(folderAction, &QAction::triggered, this,
                &TrayController::openFolderRequested);
        connect(settingsAction, &QAction::triggered, this, &TrayController::settingsRequested);
        connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

        m_tray->setContextMenu(menu);
        m_tray->show();
    } else {
        showFallbackWindow();
    }

    return trayAvailable;
}

void TrayController::updateRegionLabel(const QKeySequence& hotkey)
{
    if (!m_regionAction)
        return;
    m_regionAction->setText(QStringLiteral("区域截图（%1）")
                                .arg(hotkey.toString(QKeySequence::NativeText)));
}

void TrayController::notify(const QString& title, const QString& message) const
{
    if (m_tray && m_tray->isVisible())
        m_tray->showMessage(title, message, QSystemTrayIcon::Information, 3000);
    else
        qInfo("%s: %s", qUtf8Printable(title), qUtf8Printable(message));
}

void TrayController::showFallbackWindow()
{
    m_fallbackWindow = new QWidget;
    m_fallbackWindow->setWindowTitle(QStringLiteral("LumeShot"));
    m_fallbackWindow->setAttribute(Qt::WA_DeleteOnClose);

    auto* layout = new QVBoxLayout(m_fallbackWindow);
    auto* hint = new QLabel(
        QStringLiteral("当前桌面没有可用的系统托盘，可以直接使用下面的按钮截图。\n"
                       "快捷键与保存方式可在“设置…”中修改。"));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* regionButton = new QPushButton(QStringLiteral("区域截图"), m_fallbackWindow);
    auto* fullScreenButton = new QPushButton(QStringLiteral("全屏截图"), m_fallbackWindow);
    auto* windowButton = new QPushButton(QStringLiteral("当前窗口截图"), m_fallbackWindow);
    auto* settingsButton = new QPushButton(QStringLiteral("设置…"), m_fallbackWindow);
    layout->addWidget(regionButton);
    layout->addWidget(fullScreenButton);
    layout->addWidget(windowButton);
    layout->addWidget(settingsButton);

    connect(regionButton, &QPushButton::clicked, this, &TrayController::regionRequested);
    connect(fullScreenButton, &QPushButton::clicked, this, &TrayController::fullScreenRequested);
    connect(windowButton, &QPushButton::clicked, this, &TrayController::activeWindowRequested);
    connect(settingsButton, &QPushButton::clicked, this, &TrayController::settingsRequested);
    connect(m_fallbackWindow, &QObject::destroyed, qApp, &QCoreApplication::quit);

    m_fallbackWindow->resize(340, 220);
    m_fallbackWindow->show();
}
