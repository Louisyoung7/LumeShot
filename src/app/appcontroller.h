// 模块 app：应用编排层——连接托盘、全局热键与截图流程，协调输出和长截图会话。
#pragma once

#include <QObject>
#include <QRect>
#include <QString>

#include "config/appsettings.h"

class GlobalHotKey;
class LongCaptureSession;
class ScreenshotOutput;
class SelectionOverlay;
class TrayController;

class AppController : public QObject {
    Q_OBJECT

public:
    explicit AppController(QObject* parent = nullptr);

    bool start();

private:
    void captureRegion();
    void captureFullScreen();
    void captureActiveWindow();
    void startLongCapture(const QRect& region);
    void openSettings();
    void updateHotkeyLabel();
    void notify(const QString& title, const QString& message) const;
    bool busy() const;

    AppSettings m_settings;
    TrayController* m_tray = nullptr;
    GlobalHotKey* m_hotkey = nullptr;
    ScreenshotOutput* m_output = nullptr;
    LongCaptureSession* m_longCapture = nullptr;
    SelectionOverlay* m_overlay = nullptr;
    bool m_capturing = false;
};
