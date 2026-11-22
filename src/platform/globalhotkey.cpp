#include "globalhotkey.h"

#include <QKeyCombination>
#include <QSocketNotifier>

#include <X11/Xlib.h>
#include <X11/keysym.h>

namespace {
bool g_grabDenied = false;

int grabErrorHandler(Display*, XErrorEvent* event)
{
    if (event->error_code == BadAccess)
        g_grabDenied = true;
    return 0;
}

// NumLock、CapsLock 会额外带上修饰位，需要把它们的组合一起抓取
const unsigned int kLockMasks[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};

// Qt 键值到 X11 KeySym 的映射，返回 NoSymbol 表示不支持
KeySym keySymForKey(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return XK_a + (key - Qt::Key_A);
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return XK_0 + (key - Qt::Key_0);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return XK_F1 + (key - Qt::Key_F1);

    switch (key) {
    case Qt::Key_Print:
        return XK_Print;
    case Qt::Key_Space:
        return XK_space;
    case Qt::Key_Tab:
        return XK_Tab;
    case Qt::Key_Backtab:
        return XK_ISO_Left_Tab;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return XK_Return;
    case Qt::Key_Backspace:
        return XK_BackSpace;
    case Qt::Key_Insert:
        return XK_Insert;
    case Qt::Key_Delete:
        return XK_Delete;
    case Qt::Key_Home:
        return XK_Home;
    case Qt::Key_End:
        return XK_End;
    case Qt::Key_Left:
        return XK_Left;
    case Qt::Key_Up:
        return XK_Up;
    case Qt::Key_Right:
        return XK_Right;
    case Qt::Key_Down:
        return XK_Down;
    case Qt::Key_PageUp:
        return XK_Prior;
    case Qt::Key_PageDown:
        return XK_Next;
    case Qt::Key_Minus:
        return XK_minus;
    case Qt::Key_Equal:
        return XK_equal;
    case Qt::Key_Comma:
        return XK_comma;
    case Qt::Key_Period:
        return XK_period;
    case Qt::Key_Slash:
        return XK_slash;
    case Qt::Key_Semicolon:
        return XK_semicolon;
    case Qt::Key_Apostrophe:
        return XK_apostrophe;
    case Qt::Key_BracketLeft:
        return XK_bracketleft;
    case Qt::Key_BracketRight:
        return XK_bracketright;
    case Qt::Key_Backslash:
        return XK_backslash;
    case Qt::Key_QuoteLeft:
        return XK_grave;
    default:
        return NoSymbol;
    }
}

unsigned int xModifiers(Qt::KeyboardModifiers modifiers)
{
    unsigned int result = 0;
    if (modifiers & Qt::ShiftModifier)
        result |= ShiftMask;
    if (modifiers & Qt::ControlModifier)
        result |= ControlMask;
    if (modifiers & Qt::AltModifier)
        result |= Mod1Mask;
    if (modifiers & Qt::MetaModifier)
        result |= Mod4Mask;
    return result;
}
} // namespace

GlobalHotKey::GlobalHotKey(QObject* parent)
    : QObject(parent)
{
}

GlobalHotKey::~GlobalHotKey()
{
    if (!m_display)
        return;
    ungrab();
    if (m_notifier) {
        m_notifier->setEnabled(false);
        delete m_notifier;
        m_notifier = nullptr;
    }
    XCloseDisplay(m_display);
    m_display = nullptr;
}

bool GlobalHotKey::grab(const QKeySequence& sequence)
{
    if (sequence.isEmpty())
        return false;

    if (!m_display) {
        m_display = XOpenDisplay(nullptr);
        if (!m_display)
            return false;
    }

    const QKeyCombination combination = sequence[0];
    const KeySym keysym = keySymForKey(combination.key());
    if (keysym == NoSymbol)
        return false;

    const KeyCode keycode = XKeysymToKeycode(m_display, keysym);
    if (keycode == 0)
        return false;

    const unsigned int modifiers = xModifiers(combination.keyboardModifiers());

    ungrab();

    const Window root = DefaultRootWindow(m_display);
    g_grabDenied = false;
    XErrorHandler previousHandler = XSetErrorHandler(grabErrorHandler);
    for (const unsigned int lockMask : kLockMasks)
        XGrabKey(m_display, keycode, modifiers | lockMask, root, False,
                 GrabModeAsync, GrabModeAsync);
    XSync(m_display, False);
    XSetErrorHandler(previousHandler);

    if (g_grabDenied) {
        for (const unsigned int lockMask : kLockMasks)
            XUngrabKey(m_display, keycode, modifiers | lockMask, root);
        XFlush(m_display);
        return false;
    }

    m_keycode = keycode;
    m_modifiers = modifiers;
    m_sequence = sequence;

    if (!m_notifier) {
        m_notifier = new QSocketNotifier(ConnectionNumber(m_display), QSocketNotifier::Read, this);
        connect(m_notifier, &QSocketNotifier::activated, this, [this] {
            while (XPending(m_display) > 0) {
                XEvent event;
                XNextEvent(m_display, &event);
                if (event.type == KeyPress)
                    emit activated();
            }
        });
    } else {
        m_notifier->setEnabled(true);
    }

    XFlush(m_display);
    return true;
}

bool GlobalHotKey::isSupported(const QKeySequence& sequence)
{
    if (sequence.isEmpty() || sequence.count() != 1)
        return false;

    const QKeyCombination combination = sequence[0];
    if (keySymForKey(combination.key()) == NoSymbol)
        return false;

    // 只按字母或功能键会抢走正常输入，必须带 Ctrl/Alt/Super
    return combination.keyboardModifiers()
        & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
}

bool GlobalHotKey::probe(const QKeySequence& sequence)
{
    if (!isSupported(sequence))
        return false;

    if (!m_display) {
        m_display = XOpenDisplay(nullptr);
        if (!m_display)
            return false;
    }

    const QKeyCombination combination = sequence[0];
    const KeyCode keycode = XKeysymToKeycode(m_display, keySymForKey(combination.key()));
    if (keycode == 0)
        return false;

    const unsigned int modifiers = xModifiers(combination.keyboardModifiers());

    // 正好是当前自己注册的组合，直接视为可用，避免把已有 grab 松开
    if (m_keycode == keycode && m_modifiers == modifiers)
        return true;

    const Window root = DefaultRootWindow(m_display);

    g_grabDenied = false;
    XErrorHandler previousHandler = XSetErrorHandler(grabErrorHandler);
    for (const unsigned int lockMask : kLockMasks)
        XGrabKey(m_display, keycode, modifiers | lockMask, root, False,
                 GrabModeAsync, GrabModeAsync);
    XSync(m_display, False);
    XSetErrorHandler(previousHandler);

    // 无论成功与否都立刻松开，保持原有注册状态不变
    for (const unsigned int lockMask : kLockMasks)
        XUngrabKey(m_display, keycode, modifiers | lockMask, root);
    XFlush(m_display);

    return !g_grabDenied;
}

void GlobalHotKey::suspend()
{
    ungrab();
}

bool GlobalHotKey::resume()
{
    return grab(m_sequence);
}

void GlobalHotKey::ungrab()
{
    if (!m_display || m_keycode == 0)
        return;
    const Window root = DefaultRootWindow(m_display);
    for (const unsigned int lockMask : kLockMasks)
        XUngrabKey(m_display, m_keycode, m_modifiers | lockMask, root);
    XFlush(m_display);
    m_keycode = 0;
    m_modifiers = 0;
}
