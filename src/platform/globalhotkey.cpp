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
    const int key = combination.key();
    const Qt::KeyboardModifiers qtModifiers = combination.keyboardModifiers();

    KeySym keysym = NoSymbol;
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        keysym = XK_a + (key - Qt::Key_A);
    else if (key >= Qt::Key_0 && key <= Qt::Key_9)
        keysym = XK_0 + (key - Qt::Key_0);
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F12)
        keysym = XK_F1 + (key - Qt::Key_F1);
    else if (key == Qt::Key_Print)
        keysym = XK_Print;
    else if (key == Qt::Key_Space)
        keysym = XK_space;

    if (keysym == NoSymbol)
        return false;

    const KeyCode keycode = XKeysymToKeycode(m_display, keysym);
    if (keycode == 0)
        return false;

    unsigned int modifiers = 0;
    if (qtModifiers & Qt::ShiftModifier)
        modifiers |= ShiftMask;
    if (qtModifiers & Qt::ControlModifier)
        modifiers |= ControlMask;
    if (qtModifiers & Qt::AltModifier)
        modifiers |= Mod1Mask;
    if (qtModifiers & Qt::MetaModifier)
        modifiers |= Mod4Mask;

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
