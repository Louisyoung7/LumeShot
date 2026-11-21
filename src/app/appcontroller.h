// 模块 app：应用编排层——连接托盘、全局热键与截图流程，负责保存和剪贴板。
#pragma once

#include <QObject>
#include <QPixmap>
#include <QPointer>
#include <QRect>
#include <QString>

#include "config/appsettings.h"

class GlobalHotKey;
class LongCaptureFrame;
class LongCapturePreview;
class LongCaptureSidebar;
class QAction;
class QImage;
class QSystemTrayIcon;
class QWidget;
class ScrollCapture;
class SelectionOverlay;

class AppController : public QObject {
    Q_OBJECT

public:
    explicit AppController(QObject* parent = nullptr);

    bool start();

private:
    void captureRegion();
    void captureFullScreen();
    void captureActiveWindow();
    void deliver(const QPixmap& capture, const QString& title);
    void startLongCapture(const QRect& region);
    void showLongCapturePreview(const QImage& image, const QRect& region);
    void pinImage(const QImage& image, const QPoint& preferred, const QRect& available);
    void closeLongCaptureWindows();
    void cleanupLongCapture();
    void openSettings();
    void updateHotkeyLabel();
    void notify(const QString& title, const QString& message) const;
    void showFallbackWindow();

    AppSettings m_settings;
    QSystemTrayIcon* m_tray = nullptr;
    GlobalHotKey* m_hotkey = nullptr;
    SelectionOverlay* m_overlay = nullptr;
    QWidget* m_fallbackWindow = nullptr;
    QAction* m_regionAction = nullptr;
    QPointer<LongCaptureFrame> m_longFrame;
    QPointer<LongCaptureSidebar> m_longSidebar;
    QPointer<ScrollCapture> m_scrollCapture;
    QPointer<LongCapturePreview> m_longPreview;
    bool m_capturing = false;
    bool m_longCapturing = false;
};
