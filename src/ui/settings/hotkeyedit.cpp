#include "ui/settings/hotkeyedit.h"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QMessageBox>

namespace {
// 只按下修饰键时不算完成录制，继续等待主键
bool isModifierKey(int key)
{
    switch (key) {
    case Qt::Key_Control:
    case Qt::Key_Shift:
    case Qt::Key_Alt:
    case Qt::Key_Meta:
    case Qt::Key_AltGr:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_Hyper_L:
    case Qt::Key_Hyper_R:
        return true;
    default:
        return false;
    }
}

// 去掉录制时不需要的小键盘、输入法修饰位
Qt::KeyboardModifiers normalizeModifiers(Qt::KeyboardModifiers modifiers)
{
    return modifiers & ~(Qt::KeypadModifier | Qt::GroupSwitchModifier);
}

QString modifiersText(Qt::KeyboardModifiers modifiers)
{
    QStringList parts;
    if (modifiers & Qt::ControlModifier)
        parts << QStringLiteral("Ctrl");
    if (modifiers & Qt::AltModifier)
        parts << QStringLiteral("Alt");
    if (modifiers & Qt::ShiftModifier)
        parts << QStringLiteral("Shift");
    if (modifiers & Qt::MetaModifier)
        parts << QStringLiteral("Super");
    return parts.join(QLatin1Char('+'));
}
} // namespace

HotkeyEdit::HotkeyEdit(QWidget* parent)
    : QPushButton(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    // 避免对话框里按 Enter 顺手触发本按钮
    setAutoDefault(false);
    setDefault(false);
    setToolTip(QStringLiteral("点击后按下新的快捷键，Esc 取消，Backspace 清除"));
    refreshText();

    connect(this, &QPushButton::clicked, this, [this] {
        if (m_recording)
            stopRecording();
        else
            startRecording();
    });
}

QKeySequence HotkeyEdit::keySequence() const
{
    return m_sequence;
}

void HotkeyEdit::setKeySequence(const QKeySequence& sequence)
{
    m_sequence = sequence;
    refreshText();
}

void HotkeyEdit::setValidator(HotkeyValidator validator)
{
    m_validator = std::move(validator);
}

bool HotkeyEdit::event(QEvent* event)
{
    if (m_recording) {
        switch (event->type()) {
        case QEvent::ShortcutOverride:
            // 吞掉快捷键分发，避免录制时触发对话框里的 QAction
            event->accept();
            return true;
        case QEvent::KeyPress: {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (!keyEvent->isAutoRepeat())
                handleKeyPress(keyEvent);
            return true;
        }
        case QEvent::KeyRelease:
            return true;
        default:
            break;
        }
    }
    return QPushButton::event(event);
}

void HotkeyEdit::focusOutEvent(QFocusEvent* event)
{
    if (m_recording && !m_ignoreFocusOut)
        stopRecording();
    QPushButton::focusOutEvent(event);
}

void HotkeyEdit::startRecording()
{
    if (m_recording)
        return;

    m_recording = true;
    m_pendingText.clear();
    setFocus(Qt::MouseFocusReason);
    grabKeyboard();
    refreshText();
}

void HotkeyEdit::stopRecording()
{
    if (!m_recording)
        return;

    m_recording = false;
    releaseKeyboard();
    m_pendingText.clear();
    refreshText();
}

void HotkeyEdit::handleKeyPress(QKeyEvent* event)
{
    const int key = event->key();
    const Qt::KeyboardModifiers modifiers = normalizeModifiers(event->modifiers());

    if (key == Qt::Key_Escape) {
        stopRecording();
        return;
    }

    if ((key == Qt::Key_Backspace || key == Qt::Key_Delete)
        && modifiers == Qt::NoModifier) {
        m_sequence = QKeySequence();
        emit sequenceChanged(m_sequence);
        stopRecording();
        return;
    }

    if (isModifierKey(key)) {
        const QString text = modifiersText(modifiers);
        m_pendingText = text.isEmpty() ? QString()
                                       : text + QLatin1Char('+');
        refreshText();
        return;
    }

    if (key == Qt::Key_unknown)
        return;

    if (!(modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        m_pendingText = QStringLiteral("需要包含 Ctrl、Alt 或 Super");
        refreshText();
        return;
    }

    applySequence(QKeySequence(QKeyCombination(modifiers, Qt::Key(key))));
}

void HotkeyEdit::applySequence(const QKeySequence& sequence)
{
    if (m_validator) {
        const QString error = m_validator(sequence);
        if (!error.isEmpty()) {
            // 弹窗会抢走焦点，先标记忽略，关闭后回到录制态
            m_ignoreFocusOut = true;
            releaseKeyboard();
            QMessageBox::warning(this, QStringLiteral("快捷键不可用"), error);
            m_ignoreFocusOut = false;
            setFocus(Qt::OtherFocusReason);
            grabKeyboard();
            m_pendingText.clear();
            refreshText();
            return;
        }
    }

    m_sequence = sequence;
    emit sequenceChanged(m_sequence);
    stopRecording();
}

void HotkeyEdit::refreshText()
{
    if (!m_recording) {
        setText(m_sequence.isEmpty()
                    ? QStringLiteral("点击设置快捷键")
                    : m_sequence.toString(QKeySequence::NativeText));
        return;
    }

    setText(m_pendingText.isEmpty()
                ? QStringLiteral("请按下快捷键…（Esc 取消）")
                : m_pendingText);
}
