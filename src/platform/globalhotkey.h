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

    // 注册组合键（支持字母、数字、F1-F24、空格、方向键、编辑键与常用标点），失败返回 false
    bool grab(const QKeySequence& sequence);

    // 组合键是否在可注册范围内（不访问 X11，可在界面层直接调用）
    static bool isSupported(const QKeySequence& sequence);

    // 试注册检测是否被占用，成功后立即释放，不改变当前已注册的快捷键
    bool probe(const QKeySequence& sequence);

    // 临时释放快捷键（打开设置界面时避免自己的热键抢走按键）
    void suspend();
    // 释放后按原组合键重新注册
    bool resume();

signals:
    void activated();

private:
    void ungrab();
    // 发起 XGrabKey 请求，失败时回滚并返回 false（不修改 m_keycode/m_modifiers）
    bool installGrab(unsigned int keycode, unsigned int modifiers);
    // 键盘映射变化后按当前布局重新解析 keycode 并重新注册
    void reapplyGrab();

    _XDisplay* m_display = nullptr;
    QSocketNotifier* m_notifier = nullptr;
    QKeySequence m_sequence;
    unsigned int m_keycode = 0;
    unsigned int m_modifiers = 0;
};
