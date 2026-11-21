#include "app/appcontroller.h"

#include "capture/screencapture.h"
#include "capture/scrollcapture.h"
#include "config/appsettings.h"
#include "platform/globalhotkey.h"
#include "ui/longcaptureframe.h"
#include "ui/longcapturepreview.h"
#include "ui/longcapturesidebar.h"
#include "ui/pinwindow.h"
#include "ui/selectionoverlay.h"
#include "ui/settingsdialog.h"

#include <QAction>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

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

// 尽量把侧边栏放在选区右侧，放不下就翻到左侧
QPoint placeSidebarOutside(const QRect& region, const QSize& size)
{
    const QRect desktop = ScreenCapture::virtualGeometry();

    int x = region.right() + 12;
    if (x + size.width() > desktop.right() + 1)
        x = region.left() - size.width() - 12;
    x = qBound(desktop.left(), x, desktop.right() - size.width() + 1);

    int y = region.center().y() - size.height() / 2;
    y = qBound(desktop.top(), y, desktop.bottom() - size.height() + 1);
    return QPoint(x, y);
}
} // namespace

AppController::AppController(QObject* parent)
    : QObject(parent)
{
}

bool AppController::start()
{
    m_settings = AppSettings::load();

    const bool trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();

    if (trayAvailable) {
        m_tray = new QSystemTrayIcon(createTrayIcon(), this);
        m_tray->setToolTip(QStringLiteral("ScreenShot 截屏工具"));

        auto* menu = new QMenu();
        m_regionAction = menu->addAction(QString());
        QAction* fullScreenAction = menu->addAction(QStringLiteral("全屏截图"));
        QAction* windowAction = menu->addAction(QStringLiteral("当前窗口截图"));
        menu->addSeparator();
        QAction* folderAction = menu->addAction(QStringLiteral("打开截图目录"));
        QAction* settingsAction = menu->addAction(QStringLiteral("设置…"));
        menu->addSeparator();
        QAction* quitAction = menu->addAction(QStringLiteral("退出"));

        updateHotkeyLabel();

        connect(m_regionAction, &QAction::triggered, this, &AppController::captureRegion);
        connect(fullScreenAction, &QAction::triggered, this, &AppController::captureFullScreen);
        connect(windowAction, &QAction::triggered, this, &AppController::captureActiveWindow);
        connect(folderAction, &QAction::triggered, this, [this] {
            const QString directory = m_settings.effectiveSaveDirectory();
            QDir().mkpath(directory);
            QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
        });
        connect(settingsAction, &QAction::triggered, this, &AppController::openSettings);
        connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

        m_tray->setContextMenu(menu);
        m_tray->show();
    }

    m_hotkey = new GlobalHotKey(this);
    connect(m_hotkey, &GlobalHotKey::activated, this, &AppController::captureRegion);
    const QString hotkeyText = m_settings.hotkey.toString(QKeySequence::NativeText);
    const bool hotkeyReady = m_hotkey->grab(m_settings.hotkey);

    if (!trayAvailable)
        showFallbackWindow();

    qInfo("ScreenShot started: tray=%d hotkey=%d", int(trayAvailable), int(hotkeyReady));

    if (!trayAvailable && !hotkeyReady)
        return false;

    QTimer::singleShot(500, this, [this, hotkeyReady, hotkeyText] {
        notify(QStringLiteral("ScreenShot 已启动"),
               hotkeyReady
                   ? QStringLiteral("按 %1 开始区域截图").arg(hotkeyText)
                   : QStringLiteral("全局热键注册失败，可在“设置…”里更换快捷键"));
    });
    return true;
}

void AppController::captureRegion()
{
    if (m_capturing || m_longCapturing)
        return;

    const QPixmap desktop = ScreenCapture::grabDesktop();
    if (desktop.isNull()) {
        notify(QStringLiteral("截图失败"), QStringLiteral("无法读取屏幕内容"));
        return;
    }

    m_capturing = true;
    m_overlay = new SelectionOverlay(desktop, ScreenCapture::virtualGeometry());
    connect(m_overlay, &SelectionOverlay::regionSelected, this, [this](const QPixmap& capture) {
        m_capturing = false;
        deliver(capture, QStringLiteral("区域截图"));
    });
    connect(m_overlay, &SelectionOverlay::pinRequested, this,
            [this](const QPixmap& capture, const QPoint& position) {
                m_capturing = false;
                auto* pin = new PinWindow(capture);
                pin->move(position);
                pin->show();
                pin->raise();
            });
    connect(m_overlay, &SelectionOverlay::longCaptureRequested, this,
            &AppController::startLongCapture);
    connect(m_overlay, &SelectionOverlay::canceled, this, [this] { m_capturing = false; });
    connect(m_overlay, &QObject::destroyed, this, [this] {
        m_capturing = false;
        m_overlay = nullptr;
    });
    m_overlay->show();
}

void AppController::captureFullScreen()
{
    if (m_capturing || m_longCapturing)
        return;

    const QPixmap desktop = ScreenCapture::grabDesktop();
    if (desktop.isNull()) {
        notify(QStringLiteral("截图失败"), QStringLiteral("无法读取屏幕内容"));
        return;
    }
    deliver(desktop, QStringLiteral("全屏截图"));
}

void AppController::captureActiveWindow()
{
    if (m_capturing || m_longCapturing)
        return;

    QRect windowRect;
    if (!ScreenCapture::activeWindowRect(&windowRect)) {
        notify(QStringLiteral("窗口截图失败"), QStringLiteral("未找到活动窗口"));
        return;
    }

    const QPixmap desktop = ScreenCapture::grabDesktop();
    const QPixmap capture = ScreenCapture::crop(desktop, windowRect);
    if (capture.isNull()) {
        notify(QStringLiteral("窗口截图失败"), QStringLiteral("无法读取窗口内容"));
        return;
    }
    deliver(capture, QStringLiteral("窗口截图"));
}

void AppController::deliver(const QPixmap& capture, const QString& title)
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
    notify(title, QStringLiteral("%1 × %2\n%3")
                      .arg(logicalSize.width())
                      .arg(logicalSize.height())
                      .arg(results.join(QLatin1Char('\n'))));
}

void AppController::startLongCapture(const QRect& region)
{
    if (m_longCapturing)
        return;
    m_longCapturing = true;

    // 区域边框：点击穿透的独立窗口，保证滚动仍然落到下面的页面
    m_longFrame = new LongCaptureFrame(region, ScreenCapture::virtualGeometry());
    m_longFrame->show();
    m_longFrame->raise();

    m_longSidebar = new LongCaptureSidebar;
    m_scrollCapture = new ScrollCapture(this);

    connect(m_scrollCapture, &ScrollCapture::previewChanged, this,
            [this](const QImage& thumbnail, int logicalHeight) {
                if (m_longSidebar)
                    m_longSidebar->setCapture(thumbnail, logicalHeight);
            });
    connect(m_longSidebar, &LongCaptureSidebar::finishRequested, this, [this] {
        if (m_scrollCapture)
            m_scrollCapture->finish();
    });
    connect(m_longSidebar, &LongCaptureSidebar::cancelRequested, this, [this] {
        if (m_scrollCapture)
            m_scrollCapture->cancel();
    });
    connect(m_scrollCapture, &ScrollCapture::finished, this, [this, region](const QImage& image) {
        closeLongCaptureWindows();
        cleanupLongCapture();
        if (image.isNull()) {
            m_longCapturing = false;
            return;
        }
        // 保持 m_longCapturing，直到预览被确认、钉图或取消
        showLongCapturePreview(image, region);
    });
    connect(m_scrollCapture, &ScrollCapture::canceled, this, [this] {
        m_longCapturing = false;
        closeLongCaptureWindows();
        cleanupLongCapture();
    });

    m_longSidebar->move(placeSidebarOutside(region, m_longSidebar->size()));
    m_longSidebar->show();
    m_longSidebar->raise();
    m_longSidebar->activateWindow();
    m_longSidebar->setFocus();

    m_scrollCapture->start(region);
}

// 结束后先收起边框和侧边栏，避免挡住预览
void AppController::closeLongCaptureWindows()
{
    if (m_longSidebar)
        m_longSidebar->close();
    if (m_longFrame)
        m_longFrame->close();
}

void AppController::showLongCapturePreview(const QImage& image, const QRect& region)
{
    QScreen* screen = QGuiApplication::screenAt(region.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    auto* preview = new LongCapturePreview(image, available);
    m_longPreview = preview;

    QRect geometry(QPoint(0, 0), preview->size());
    geometry.moveCenter(available.center());
    preview->move(geometry.topLeft());

    connect(preview, &LongCapturePreview::accepted, this, [this, image] {
        m_longCapturing = false;
        deliver(QPixmap::fromImage(image), QStringLiteral("长截图"));
    });
    connect(preview, &LongCapturePreview::pinRequested, this, [this, image, region, available] {
        m_longCapturing = false;
        pinImage(image, region.topLeft(), available);
    });
    connect(preview, &LongCapturePreview::canceled, this, [this] { m_longCapturing = false; });
    connect(preview, &QObject::destroyed, this, [this] {
        m_longCapturing = false;
        m_longPreview = nullptr;
    });

    preview->show();
    preview->raise();
}

void AppController::pinImage(const QImage& image, const QPoint& preferred,
                             const QRect& available)
{
    QPixmap pixmap = QPixmap::fromImage(image);

    // 长图可能比屏幕还高，钉图前等比缩放到可用区域内
    const QSize logical = pixmap.size() / pixmap.devicePixelRatio();
    const QSize limit(int(available.width() * 0.9), int(available.height() * 0.9));
    if (logical.width() > limit.width() || logical.height() > limit.height()) {
        const qreal scale = qMin(qreal(limit.width()) / logical.width(),
                                 qreal(limit.height()) / logical.height());
        pixmap = pixmap.scaled(pixmap.size() * scale, Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
    }

    auto* pin = new PinWindow(pixmap);
    const QPoint position(
        qBound(available.left(), preferred.x(), available.right() - pin->width() + 1),
        qBound(available.top(), preferred.y(), available.bottom() - pin->height() + 1));
    pin->move(position);
    pin->show();
    pin->raise();
}

void AppController::cleanupLongCapture()
{
    if (m_scrollCapture) {
        m_scrollCapture->deleteLater();
        m_scrollCapture = nullptr;
    }
    m_longSidebar = nullptr; // 侧边栏与边框都由 WA_DeleteOnClose 自行释放
    m_longFrame = nullptr;
}

void AppController::openSettings()
{
    // 先释放全局热键，否则在输入框里按下旧快捷键会被自己的 grab 拦截，录不进新值
    m_hotkey->suspend();

    SettingsDialog dialog(m_settings, m_fallbackWindow);
    if (dialog.exec() != QDialog::Accepted) {
        m_hotkey->resume();
        return;
    }

    const AppSettings updated = dialog.settings();
    if (!m_hotkey->grab(updated.hotkey)) {
        // 新快捷键不可用时恢复原来的，避免用户失去全局热键
        m_hotkey->grab(m_settings.hotkey);
        QMessageBox::warning(
            m_fallbackWindow, QStringLiteral("快捷键不可用"),
            QStringLiteral("%1 无法注册，可能已被其他程序占用。\n设置未保存。")
                .arg(updated.hotkey.toString(QKeySequence::NativeText)));
        return;
    }

    m_settings = updated;
    m_settings.save();
    updateHotkeyLabel();

    notify(QStringLiteral("设置已保存"),
           QStringLiteral("全局快捷键：%1\n自动复制：%2　自动保存：%3")
               .arg(m_settings.hotkey.toString(QKeySequence::NativeText),
                    m_settings.autoCopy ? QStringLiteral("开") : QStringLiteral("关"),
                    m_settings.autoSave ? QStringLiteral("开") : QStringLiteral("关")));
}

void AppController::updateHotkeyLabel()
{
    if (!m_regionAction)
        return;
    m_regionAction->setText(
        QStringLiteral("区域截图（%1）").arg(m_settings.hotkey.toString(QKeySequence::NativeText)));
}

void AppController::notify(const QString& title, const QString& message) const
{
    if (m_tray && m_tray->isVisible())
        m_tray->showMessage(title, message, QSystemTrayIcon::Information, 3000);
    else
        qInfo("%s: %s", qUtf8Printable(title), qUtf8Printable(message));
}

void AppController::showFallbackWindow()
{
    m_fallbackWindow = new QWidget;
    m_fallbackWindow->setWindowTitle(QStringLiteral("ScreenShot"));
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

    connect(regionButton, &QPushButton::clicked, this, &AppController::captureRegion);
    connect(fullScreenButton, &QPushButton::clicked, this, &AppController::captureFullScreen);
    connect(windowButton, &QPushButton::clicked, this, &AppController::captureActiveWindow);
    connect(settingsButton, &QPushButton::clicked, this, &AppController::openSettings);
    connect(m_fallbackWindow, &QObject::destroyed, qApp, &QCoreApplication::quit);

    m_fallbackWindow->resize(340, 220);
    m_fallbackWindow->show();
}
