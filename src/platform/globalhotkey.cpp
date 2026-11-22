#include "globalhotkey.h"

#include <QDateTime>
#include <QKeyCombination>
#include <QSocketNotifier>
#include <QTimer>

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/keysym.h>

#include <initializer_list>

namespace {
bool g_grabDenied = false;

int grabErrorHandler(Display*, XErrorEvent* event)
{
    if (event->error_code == BadAccess)
        g_grabDenied = true;
    return 0;
}

// 重新注册失败后的重试节奏
const int kMaxGrabRetries = 5;
const int kGrabRetryIntervalMs = 2000;
// 被动 grab 与 XI2 原始按键会同时上报同一次按压，这个窗口内只触发一次
const qint64 kActivationDedupMs = 250;

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

// 查询某个 KeySym 当前挂在哪个修饰键位上，返回 0 表示没有映射
unsigned int maskForKeySym(Display* display, KeySym keysym)
{
    const KeyCode keycode = XKeysymToKeycode(display, keysym);
    if (keycode == 0)
        return 0;

    XModifierKeymap* mapping = XGetModifierMapping(display);
    if (!mapping)
        return 0;

    unsigned int mask = 0;
    for (int modifier = 0; modifier < 8 && mask == 0; ++modifier) {
        for (int i = 0; i < mapping->max_keypermod; ++i) {
            if (mapping->modifiermap[modifier * mapping->max_keypermod + i] == keycode) {
                mask = 1u << modifier;
                break;
            }
        }
    }
    XFreeModifiermap(mapping);
    return mask;
}

unsigned int maskForFirstMapped(Display* display, std::initializer_list<KeySym> keysyms)
{
    for (const KeySym keysym : keysyms) {
        const unsigned int mask = maskForKeySym(display, keysym);
        if (mask != 0)
            return mask;
    }
    return 0;
}

// CapsLock、NumLock 会额外带上修饰位，需要把它们的组合一起抓取。
// 这两个锁键的修饰位不是固定的（布局/映射变化可能把它们挪到别的位），
// 所以按当前映射推导，不能写死 LockMask / Mod2Mask。
struct LockMasks {
    unsigned int values[4];
};

LockMasks lockMasks(Display* display)
{
    const unsigned int caps = maskForKeySym(display, XK_Caps_Lock);
    const unsigned int num = maskForKeySym(display, XK_Num_Lock);
    return {{0u, caps, num, caps | num}};
}

// 从当前修饰键映射推导 X 掩码。布局/输入法切换后 Mod1、Mod4 的归属可能变化，
// 沿用注册时的旧值会让 grab 还在、但真实按键对不上，表现为热键“失效”。
unsigned int xModifiers(Display* display, Qt::KeyboardModifiers modifiers)
{
    unsigned int result = 0;

    if (modifiers & Qt::ShiftModifier) {
        const unsigned int mask = maskForFirstMapped(display, {XK_Shift_L, XK_Shift_R});
        result |= mask != 0 ? mask : ShiftMask;
    }
    if (modifiers & Qt::ControlModifier) {
        const unsigned int mask = maskForFirstMapped(display, {XK_Control_L, XK_Control_R});
        result |= mask != 0 ? mask : ControlMask;
    }
    if (modifiers & Qt::AltModifier) {
        unsigned int mask = maskForFirstMapped(display, {XK_Alt_L, XK_Alt_R});
        if (mask == 0)
            mask = maskForFirstMapped(display, {XK_Meta_L, XK_Meta_R});
        result |= mask != 0 ? mask : Mod1Mask;
    }
    if (modifiers & Qt::MetaModifier) {
        unsigned int mask = maskForFirstMapped(display, {XK_Super_L, XK_Super_R});
        if (mask == 0)
            mask = maskForFirstMapped(display, {XK_Hyper_L, XK_Hyper_R});
        if (mask == 0)
            mask = maskForFirstMapped(display, {XK_Meta_L, XK_Meta_R});
        result |= mask != 0 ? mask : Mod4Mask;
    }

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

    // 原始按键通道与被动 grab 互为兜底，只需要初始化一次
    if (m_xiOpcode < 0)
        setupRawEvents();

    const QKeyCombination combination = sequence[0];
    const KeySym keysym = keySymForKey(combination.key());
    if (keysym == NoSymbol)
        return false;

    const KeyCode keycode = XKeysymToKeycode(m_display, keysym);
    if (keycode == 0)
        return false;

    const unsigned int modifiers = xModifiers(m_display, combination.keyboardModifiers());

    m_suspended = false;
    m_retryCount = 0;
    if (m_retryTimer)
        m_retryTimer->stop();

    ungrab();

    if (!installGrab(keycode, modifiers))
        return false;

    m_keycode = keycode;
    m_modifiers = modifiers;
    m_sequence = sequence;

    if (!m_notifier) {
        m_notifier = new QSocketNotifier(ConnectionNumber(m_display), QSocketNotifier::Read, this);
        connect(m_notifier, &QSocketNotifier::activated, this, &GlobalHotKey::pumpEvents);
    } else {
        m_notifier->setEnabled(true);
    }

    XFlush(m_display);
    return true;
}

bool GlobalHotKey::installGrab(unsigned int keycode, unsigned int modifiers)
{
    const Window root = DefaultRootWindow(m_display);
    const LockMasks locks = lockMasks(m_display);
    g_grabDenied = false;
    XErrorHandler previousHandler = XSetErrorHandler(grabErrorHandler);
    for (const unsigned int lockMask : locks.values)
        XGrabKey(m_display, keycode, modifiers | lockMask, root, False,
                 GrabModeAsync, GrabModeAsync);
    XSync(m_display, False);
    XSetErrorHandler(previousHandler);

    if (g_grabDenied) {
        for (const unsigned int lockMask : locks.values)
            XUngrabKey(m_display, keycode, modifiers | lockMask, root);
        XFlush(m_display);
        m_activeMaskCount = 0;
        return false;
    }

    // 记录实际抓取到的完整掩码，释放时按原样松开，避免映射变化后漏掉旧 grab
    for (int i = 0; i < 4; ++i)
        m_activeMasks[i] = modifiers | locks.values[i];
    m_activeMaskCount = 4;
    return true;
}

void GlobalHotKey::pumpEvents()
{
    if (!m_display || m_pumping)
        return;

    m_pumping = true;
    for (;;) {
        bool mappingChanged = false;
        // QueuedAfterReading 会在队列为空时从 socket 非阻塞读取，
        // 不依赖调用方一定先把事件读进队列
        while (XEventsQueued(m_display, QueuedAfterReading) > 0) {
            XEvent event;
            XNextEvent(m_display, &event);
            if (event.type == KeyPress) {
                if (claimActivation())
                    emit activated();
            } else if (event.type == GenericEvent && m_xiOpcode >= 0
                       && event.xcookie.extension == m_xiOpcode) {
                if (XGetEventData(m_display, &event.xcookie)) {
                    if (event.xcookie.evtype == XI_RawKeyPress) {
                        const auto* raw = static_cast<const XIRawEvent*>(event.xcookie.data);
                        handleRawKeyPress(raw->detail);
                    }
                    XFreeEventData(m_display, &event.xcookie);
                }
            } else if (event.type == MappingNotify) {
                XRefreshKeyboardMapping(&event.xmapping);
                if (event.xmapping.request != MappingPointer)
                    mappingChanged = true;
            }
        }

        if (!mappingChanged || m_suspended)
            break;

        // 重新注册会调用 XSync，期间到达的事件可能被读进 Xlib 队列而不再触发
        // QSocketNotifier，因此注册完再排空一次队列
        reapplyGrab();
    }
    m_pumping = false;
}

void GlobalHotKey::reapplyGrab()
{
    // 设置界面挂起期间不抢按键，等 resume() 时再注册
    if (!m_display || m_suspended || m_keycode == 0 || m_sequence.isEmpty())
        return;

    const QKeyCombination combination = m_sequence[0];
    const KeySym keysym = keySymForKey(combination.key());
    if (keysym == NoSymbol)
        return;

    // 映射变化后旧 keycode 可能失效，按当前布局重新解析
    const KeyCode keycode = XKeysymToKeycode(m_display, keysym);
    if (keycode == 0)
        return; // 当前布局里没有这个键，等下一次映射变化再处理

    const unsigned int modifiers = xModifiers(m_display, combination.keyboardModifiers());

    // 旧 grab 可能挂在已经失效的 keycode/修饰位上，先释放，避免残留；
    // 同时保证重新注册时不会与自己残留的 grab 互相干扰
    ungrab();

    if (installGrab(keycode, modifiers)) {
        m_keycode = keycode;
        m_modifiers = modifiers;
        m_retryCount = 0;
        if (m_retryTimer)
            m_retryTimer->stop();
        return;
    }

    // 一般是别的程序临时占用了同一组合：保留最新键位，稍后自动重试
    m_keycode = keycode;
    m_modifiers = modifiers;
    scheduleRetry();
}

void GlobalHotKey::scheduleRetry()
{
    if (m_suspended || m_retryCount >= kMaxGrabRetries)
        return;

    if (!m_retryTimer) {
        m_retryTimer = new QTimer(this);
        m_retryTimer->setSingleShot(true);
        connect(m_retryTimer, &QTimer::timeout, this, [this] {
            ++m_retryCount;
            reapplyGrab();
        });
    }
    m_retryTimer->start(kGrabRetryIntervalMs);
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

    const unsigned int modifiers = xModifiers(m_display, combination.keyboardModifiers());

    // 正好是当前自己注册的组合，直接视为可用，避免把已有 grab 松开
    if (m_keycode == keycode && m_modifiers == modifiers)
        return true;

    const Window root = DefaultRootWindow(m_display);
    const LockMasks locks = lockMasks(m_display);

    g_grabDenied = false;
    XErrorHandler previousHandler = XSetErrorHandler(grabErrorHandler);
    for (const unsigned int lockMask : locks.values)
        XGrabKey(m_display, keycode, modifiers | lockMask, root, False,
                 GrabModeAsync, GrabModeAsync);
    XSync(m_display, False);
    XSetErrorHandler(previousHandler);

    // 无论成功与否都立刻松开，保持原有注册状态不变
    for (const unsigned int lockMask : locks.values)
        XUngrabKey(m_display, keycode, modifiers | lockMask, root);
    XFlush(m_display);

    return !g_grabDenied;
}

void GlobalHotKey::suspend()
{
    m_suspended = true;
    m_retryCount = 0;
    if (m_retryTimer)
        m_retryTimer->stop();
    ungrab();
}

bool GlobalHotKey::resume()
{
    m_suspended = false;
    m_retryCount = 0;
    if (m_retryTimer)
        m_retryTimer->stop();
    return grab(m_sequence);
}

void GlobalHotKey::ungrab()
{
    if (!m_display || m_keycode == 0)
        return;
    const Window root = DefaultRootWindow(m_display);
    // 按注册时记录的完整掩码释放：映射可能已经变了，
    // 用新推导的锁定位去 ungrab 会漏掉旧 grab，留下收不到按键的残留
    for (int i = 0; i < m_activeMaskCount; ++i)
        XUngrabKey(m_display, m_keycode, m_activeMasks[i], root);
    XFlush(m_display);
    m_activeMaskCount = 0;
    m_keycode = 0;
    m_modifiers = 0;
}

bool GlobalHotKey::setupRawEvents()
{
    int eventBase = 0;
    int errorBase = 0;
    if (!XQueryExtension(m_display, "XInputExtension", &m_xiOpcode, &eventBase, &errorBase)) {
        m_xiOpcode = -1;
        return false;
    }

    int major = 2;
    int minor = 0;
    if (XIQueryVersion(m_display, &major, &minor) != Success) {
        m_xiOpcode = -1;
        return false;
    }

    unsigned char mask[XIMaskLen(XI_LASTEVENT)] = {};
    XISetMask(mask, XI_RawKeyPress);
    XIEventMask eventMask;
    eventMask.deviceid = XIAllMasterDevices;
    eventMask.mask_len = sizeof(mask);
    eventMask.mask = mask;
    const int selectResult = XISelectEvents(m_display, DefaultRootWindow(m_display),
                                            &eventMask, 1);
    XFlush(m_display);
    if (selectResult != Success) {
        m_xiOpcode = -1;
        return false;
    }
    return true;
}

void GlobalHotKey::handleRawKeyPress(int keycode)
{
    if (m_suspended || m_keycode == 0 || m_sequence.isEmpty())
        return;
    if (static_cast<unsigned int>(keycode) != m_keycode)
        return;

    // 原始事件不带修饰位，向服务器查询当前键盘状态
    Window rootRet = None;
    Window childRet = None;
    int rootX = 0;
    int rootY = 0;
    int winX = 0;
    int winY = 0;
    unsigned int state = 0;
    if (!XQueryPointer(m_display, DefaultRootWindow(m_display), &rootRet, &childRet, &rootX,
                       &rootY, &winX, &winY, &state)) {
        return;
    }

    // 只比对 Shift/Ctrl/Alt/Super，忽略 CapsLock/NumLock 与其他扩展修饰位
    const unsigned int expected = xModifiers(m_display, m_sequence[0].keyboardModifiers());
    const unsigned int watched = xModifiers(m_display, Qt::ShiftModifier)
        | xModifiers(m_display, Qt::ControlModifier)
        | xModifiers(m_display, Qt::AltModifier) | xModifiers(m_display, Qt::MetaModifier);
    if ((state & watched) != expected)
        return;

    if (!claimActivation())
        return;
    emit activated();
}

bool GlobalHotKey::claimActivation()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastActivationMs > 0 && now - m_lastActivationMs < kActivationDedupMs)
        return false;
    m_lastActivationMs = now;
    return true;
}
