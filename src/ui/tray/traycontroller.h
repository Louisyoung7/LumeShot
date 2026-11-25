// 模块 ui：系统托盘与无托盘时的独立入口窗口，只负责界面与动作转发。
#pragma once

#include <QObject>
#include <QString>

class QAction;
class QKeySequence;
class QSystemTrayIcon;
class QWidget;

class TrayController : public QObject {
    Q_OBJECT

public:
    explicit TrayController(QObject* parent = nullptr);

    // 创建托盘；没有系统托盘时创建独立入口窗口。返回系统托盘是否可用
    bool start();

    // 设置对话框的父窗口：有托盘时为空，回退窗口时为该窗口
    QWidget* dialogParent() const { return m_fallbackWindow; }

    void updateRegionLabel(const QKeySequence& hotkey);

    // 长截图进行中时，在托盘菜单里显示完成/取消入口。
    // 侧边栏被移出屏幕、键盘焦点丢失时，这是唯一可用的结束方式。
    void setLongCaptureActive(bool active);

    void notify(const QString& title, const QString& message) const;

signals:
    void regionRequested();
    void fullScreenRequested();
    void activeWindowRequested();
    void openFolderRequested();
    void settingsRequested();
    void longCaptureFinishRequested();
    void longCaptureCancelRequested();

private:
    void showFallbackWindow();

    QSystemTrayIcon* m_tray = nullptr;
    QWidget* m_fallbackWindow = nullptr;
    QAction* m_regionAction = nullptr;
    QAction* m_longCaptureSeparator = nullptr;
    QAction* m_longCaptureFinishAction = nullptr;
    QAction* m_longCaptureCancelAction = nullptr;
};
