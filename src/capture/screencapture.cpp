#include "capture/screencapture.h"

#include <QGuiApplication>
#include <QPainter>
#include <QScreen>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

QRect ScreenCapture::virtualGeometry()
{
    QRect geometry;
    for (QScreen* screen : QGuiApplication::screens())
        geometry = geometry.united(screen->geometry());
    return geometry;
}

QPixmap ScreenCapture::grabDesktop()
{
    const QRect geometry = virtualGeometry();
    if (geometry.isEmpty())
        return {};

    // X11 下 grabWindow(0) 只返回单块屏幕，需要逐屏抓取后拼接
    qreal scale = 1.0;
    for (QScreen* screen : QGuiApplication::screens())
        scale = qMax(scale, screen->devicePixelRatio());

    QPixmap desktop(QSize(qRound(geometry.width() * scale), qRound(geometry.height() * scale)));
    desktop.setDevicePixelRatio(scale);
    desktop.fill(Qt::black);

    QPainter painter(&desktop);
    for (QScreen* screen : QGuiApplication::screens()) {
        const QPixmap shot = screen->grabWindow(0);
        if (shot.isNull())
            continue;
        painter.drawPixmap(screen->geometry().topLeft() - geometry.topLeft(), shot);
    }
    return desktop;
}

QPixmap ScreenCapture::crop(const QPixmap& desktop, const QRect& rect)
{
    if (desktop.isNull() || rect.isEmpty())
        return {};

    const qreal scale = desktop.devicePixelRatio();
    const QRect deviceRect(qRound(rect.x() * scale),
                           qRound(rect.y() * scale),
                           qRound(rect.width() * scale),
                           qRound(rect.height() * scale));
    return desktop.copy(deviceRect);
}

bool ScreenCapture::activeWindowRect(QRect* rect)
{
    if (!rect)
        return false;

    Display* display = XOpenDisplay(nullptr);
    if (!display)
        return false;

    bool ok = false;
    const Window root = DefaultRootWindow(display);

    Window focused = None;
    int revertTo = RevertToNone;
    XGetInputFocus(display, &focused, &revertTo);

    // 从焦点窗口一路向上，找到根窗口的直接子窗口
    Window window = focused;
    while (window != None && window != PointerRoot) {
        Window rootReturn = None;
        Window parent = None;
        Window* children = nullptr;
        unsigned int childCount = 0;
        if (!XQueryTree(display, window, &rootReturn, &parent, &children, &childCount))
            break;
        if (children)
            XFree(children);
        if (parent == None || parent == root)
            break;
        window = parent;
    }

    if (window != None && window != PointerRoot) {
        XWindowAttributes attributes;
        int x = 0;
        int y = 0;
        Window child = None;
        if (XGetWindowAttributes(display, window, &attributes)
            && XTranslateCoordinates(display, window, root, 0, 0, &x, &y, &child)) {
            // 窗口管理器绘制的边框不计入窗口几何，需要单独读取
            int left = 0, right = 0, top = 0, bottom = 0;
            const Atom frameExtents = XInternAtom(display, "_NET_FRAME_EXTENTS", True);
            if (frameExtents != None) {
                Atom type = None;
                int format = 0;
                unsigned long count = 0, remaining = 0;
                unsigned char* data = nullptr;
                if (XGetWindowProperty(display, window, frameExtents, 0, 4, False, XA_CARDINAL,
                                       &type, &format, &count, &remaining, &data)
                        == Success
                    && data != nullptr && count == 4) {
                    const long* extents = reinterpret_cast<long*>(data);
                    left = int(extents[0]);
                    right = int(extents[1]);
                    top = int(extents[2]);
                    bottom = int(extents[3]);
                }
                if (data)
                    XFree(data);
            }

            const QRect rootRect(x - left, y - top,
                                 attributes.width + left + right,
                                 attributes.height + top + bottom);

            // X11 根窗口坐标以物理像素计，换算成 Qt 逻辑坐标后平移到桌面坐标系
            QScreen* primary = QGuiApplication::primaryScreen();
            const qreal scale = primary ? primary->devicePixelRatio() : 1.0;
            *rect = QRect(qRound(rootRect.x() / scale),
                          qRound(rootRect.y() / scale),
                          qRound(rootRect.width() / scale),
                          qRound(rootRect.height() / scale))
                        .translated(-virtualGeometry().topLeft());
            ok = rect->isValid();
        }
    }

    XCloseDisplay(display);
    return ok;
}
