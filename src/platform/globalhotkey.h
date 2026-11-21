// 模块 platform：平台适配层——X11 全局热键，屏蔽 Xlib 细节只暴露 Qt 信号。
#pragma once

#include <QKeySequence>
#include <QObject>

class QSocketNotifier;
struct _XDisplay;

// 基于 X11 XGrabKey 的全局热键。
class GlobalHotKey : public QObject {
    Q_OBJECT

public:
    explicit GlobalHotKey(QObject* parent = nullptr);
    ~GlobalHotKey() override;

    // 注册组合键（仅支持字母、数字、F1-F12、Print、空格），失败返回 false
    bool grab(const QKeySequence& sequence);

    // 临时释放快捷键（打开设置界面时避免自己的热键抢走按键）
    void suspend();
    // 释放后按原组合键重新注册
    bool resume();

signals:
    void activated();

private:
    void ungrab();

    _XDisplay* m_display = nullptr;
    QSocketNotifier* m_notifier = nullptr;
    QKeySequence m_sequence;
    unsigned int m_keycode = 0;
    unsigned int m_modifiers = 0;
};
