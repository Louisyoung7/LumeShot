#include "app/appcontroller.h"

#include "app/longcapturesession.h"
#include "app/screenshotoutput.h"
#include "app/traycontroller.h"
#include "capture/screencapture.h"
#include "platform/autostart.h"
#include "platform/globalhotkey.h"
#include "ui/pin/pinwindow.h"
#include "ui/selection/selectionoverlay.h"
#include "ui/settings/settingsdialog.h"

#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QImage>
#include <QMessageBox>
#include <QPixmap>
#include <QTimer>
#include <QUrl>

AppController::AppController(QObject* parent)
    : QObject(parent)
{
}

bool AppController::start()
{
    m_settings = AppSettings::load();

    m_output = new ScreenshotOutput(this);
    m_output->setSettings(m_settings);
    connect(m_output, &ScreenshotOutput::messageReady, this, &AppController::notify);

    m_longCapture = new LongCaptureSession(this);
    connect(m_longCapture, &LongCaptureSession::finished, this,
            [this](const QImage& image, const QRect&) {
                m_output->deliver(QPixmap::fromImage(image), QStringLiteral("长截图"));
            });

    m_tray = new TrayController(this);
    connect(m_tray, &TrayController::regionRequested, this, &AppController::captureRegion);
    connect(m_tray, &TrayController::fullScreenRequested, this, &AppController::captureFullScreen);
    connect(m_tray, &TrayController::activeWindowRequested, this,
            &AppController::captureActiveWindow);
    connect(m_tray, &TrayController::openFolderRequested, this, [this] {
        const QString directory = m_settings.effectiveSaveDirectory();
        QDir().mkpath(directory);
        QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
    });
    connect(m_tray, &TrayController::settingsRequested, this, &AppController::openSettings);

    const bool trayAvailable = m_tray->start();
    updateHotkeyLabel();

    m_hotkey = new GlobalHotKey(this);
    connect(m_hotkey, &GlobalHotKey::activated, this, &AppController::captureRegion);
    const QString hotkeyText = m_settings.hotkey.toString(QKeySequence::NativeText);
    const bool hotkeyReady = m_hotkey->grab(m_settings.hotkey);

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
    if (busy())
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
        m_output->deliver(capture, QStringLiteral("区域截图"));
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
    if (busy())
        return;

    const QPixmap desktop = ScreenCapture::grabDesktop();
    if (desktop.isNull()) {
        notify(QStringLiteral("截图失败"), QStringLiteral("无法读取屏幕内容"));
        return;
    }
    m_output->deliver(desktop, QStringLiteral("全屏截图"));
}

void AppController::captureActiveWindow()
{
    if (busy())
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
    m_output->deliver(capture, QStringLiteral("窗口截图"));
}

void AppController::startLongCapture(const QRect& region)
{
    if (!m_longCapture || m_longCapture->isActive())
        return;
    m_longCapture->start(region);
}

void AppController::openSettings()
{
    // 先释放全局热键，否则在输入框里按下旧快捷键会被自己的 grab 拦截，录不进新值
    m_hotkey->suspend();

    // 以实际的自启文件为准，避免配置与实际状态不一致
    m_settings.autostart = Autostart::isEnabled();

    SettingsDialog dialog(m_settings, m_tray->dialogParent());
    // 录制时立即试注册，冲突就地提示，不必等到关闭对话框
    dialog.setHotkeyValidator([this](const QKeySequence& sequence) -> QString {
        if (!GlobalHotKey::isSupported(sequence))
            return QStringLiteral("该按键组合暂不支持。\n"
                                  "请使用字母、数字、F1-F24、方向键、编辑键或常用标点，"
                                  "并包含 Ctrl、Alt 或 Super。");
        if (sequence == m_settings.hotkey)
            return QString();
        if (!m_hotkey->probe(sequence)) {
            return QStringLiteral("%1 已被其他程序或系统占用，请换一个组合。")
                .arg(sequence.toString(QKeySequence::NativeText));
        }
        return QString();
    });

    if (dialog.exec() != QDialog::Accepted) {
        m_hotkey->resume();
        return;
    }

    const AppSettings updated = dialog.settings();
    if (!m_hotkey->grab(updated.hotkey)) {
        // 新快捷键不可用时恢复原来的，避免用户失去全局热键
        m_hotkey->grab(m_settings.hotkey);
        QMessageBox::warning(
            m_tray->dialogParent(), QStringLiteral("快捷键不可用"),
            QStringLiteral("%1 无法注册，可能已被其他程序占用。\n设置未保存。")
                .arg(updated.hotkey.toString(QKeySequence::NativeText)));
        return;
    }

    m_settings = updated;
    m_output->setSettings(m_settings);

    if (updated.autostart != Autostart::isEnabled()
        && !Autostart::setEnabled(updated.autostart)) {
        m_settings.autostart = Autostart::isEnabled();
        m_hotkey->grab(m_settings.hotkey);
        QMessageBox::warning(
            m_tray->dialogParent(), QStringLiteral("开机自启设置失败"),
            QStringLiteral("无法写入自启文件：\n%1\n设置未保存。")
                .arg(Autostart::desktopFilePath()));
        return;
    }

    m_settings.save();
    updateHotkeyLabel();

    notify(QStringLiteral("设置已保存"),
           QStringLiteral("全局快捷键：%1\n自动复制：%2　自动保存：%3\n开机自启：%4")
               .arg(m_settings.hotkey.toString(QKeySequence::NativeText),
                    m_settings.autoCopy ? QStringLiteral("开") : QStringLiteral("关"),
                    m_settings.autoSave ? QStringLiteral("开") : QStringLiteral("关"),
                    m_settings.autostart ? QStringLiteral("开") : QStringLiteral("关")));
}

void AppController::updateHotkeyLabel()
{
    if (m_tray)
        m_tray->updateRegionLabel(m_settings.hotkey);
}

void AppController::notify(const QString& title, const QString& message) const
{
    if (m_tray)
        m_tray->notify(title, message);
    else
        qInfo("%s: %s", qUtf8Printable(title), qUtf8Printable(message));
}

bool AppController::busy() const
{
    return m_capturing || (m_longCapture && m_longCapture->isActive());
}
