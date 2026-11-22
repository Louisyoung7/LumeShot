// 模块 ui：快捷键录制控件——点击后独占键盘，收集单个组合键。
#pragma once

#include <QKeySequence>
#include <QPushButton>

#include <functional>

// 校验录制结果：返回空字符串表示可用，否则返回失败原因
using HotkeyValidator = std::function<QString(const QKeySequence&)>;

class HotkeyEdit : public QPushButton {
    Q_OBJECT

public:
    explicit HotkeyEdit(QWidget* parent = nullptr);

    QKeySequence keySequence() const;
    void setKeySequence(const QKeySequence& sequence);
    void setValidator(HotkeyValidator validator);

signals:
    void sequenceChanged(const QKeySequence& sequence);

protected:
    bool event(QEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void startRecording();
    void stopRecording();
    void handleKeyPress(QKeyEvent* event);
    void applySequence(const QKeySequence& sequence);
    void refreshText();

    QKeySequence m_sequence;
    HotkeyValidator m_validator;
    QString m_pendingText;
    bool m_recording = false;
    bool m_ignoreFocusOut = false;
};
