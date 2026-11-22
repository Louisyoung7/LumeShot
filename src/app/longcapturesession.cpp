#include "app/longcapturesession.h"

#include "capture/screencapture.h"
#include "capture/scrollcapture.h"
#include "ui/longcapture/longcaptureframe.h"
#include "ui/longcapture/longcapturepreview.h"
#include "ui/longcapture/longcapturesidebar.h"
#include "ui/pin/pinwindow.h"

#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>

namespace {
// 尽量把侧边栏放在选区右侧，放不下就翻到左侧
QPoint placeSidebarOutside(const QRect& region, const QSize& size)
{
    const QRect desktop = ScreenCapture::virtualGeometry();

    int x = region.right() + 12;
    if (x + size.width() > desktop.right() + 1)
        x = region.left() - size.width() - 12;
    x = qBound(desktop.left(), x, desktop.right() - size.width() + 1);

    int y = region.center().y() - size.height() / 2;
    y = qBound(desktop.top(), y, desktop.bottom() - size.height() + 1);
    return QPoint(x, y);
}
} // namespace

LongCaptureSession::LongCaptureSession(QObject* parent)
    : QObject(parent)
{
}

void LongCaptureSession::start(const QRect& region)
{
    if (m_active)
        return;
    m_active = true;
    m_region = region;

    // 区域边框：点击穿透的独立窗口，保证滚动仍然落到下面的页面
    m_frame = new LongCaptureFrame(region, ScreenCapture::virtualGeometry());
    m_frame->show();
    m_frame->raise();

    m_sidebar = new LongCaptureSidebar;
    m_scroll = new ScrollCapture(this);

    connect(m_scroll, &ScrollCapture::previewChanged, this,
            [this](const QImage& thumbnail, int logicalHeight) {
                if (m_sidebar)
                    m_sidebar->setCapture(thumbnail, logicalHeight);
            });
    connect(m_sidebar, &LongCaptureSidebar::finishRequested, this, [this] {
        if (m_scroll)
            m_scroll->finish();
    });
    connect(m_sidebar, &LongCaptureSidebar::cancelRequested, this, [this] {
        if (m_scroll)
            m_scroll->cancel();
    });
    connect(m_scroll, &ScrollCapture::finished, this, [this](const QImage& image) {
        closeWindows();
        cleanup();
        if (image.isNull()) {
            m_active = false;
            return;
        }
        // 保持 m_active，直到预览被确认、钉图或取消
        showPreview(image);
    });
    connect(m_scroll, &ScrollCapture::canceled, this, [this] {
        m_active = false;
        closeWindows();
        cleanup();
        emit canceled();
    });

    m_sidebar->move(placeSidebarOutside(region, m_sidebar->size()));
    m_sidebar->show();
    m_sidebar->raise();
    m_sidebar->activateWindow();
    m_sidebar->setFocus();

    m_scroll->start(region);
}

// 结束后先收起边框和侧边栏，避免挡住预览
void LongCaptureSession::closeWindows()
{
    if (m_sidebar)
        m_sidebar->close();
    if (m_frame)
        m_frame->close();
}

void LongCaptureSession::cleanup()
{
    if (m_scroll) {
        m_scroll->deleteLater();
        m_scroll = nullptr;
    }
    m_sidebar = nullptr; // 侧边栏与边框都由 WA_DeleteOnClose 自行释放
    m_frame = nullptr;
}

void LongCaptureSession::showPreview(const QImage& image)
{
    QScreen* screen = QGuiApplication::screenAt(m_region.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    auto* preview = new LongCapturePreview(image, available);
    m_preview = preview;

    QRect geometry(QPoint(0, 0), preview->size());
    geometry.moveCenter(available.center());
    preview->move(geometry.topLeft());

    connect(preview, &LongCapturePreview::accepted, this, [this, image] {
        m_active = false;
        emit finished(image, m_region);
    });
    connect(preview, &LongCapturePreview::pinRequested, this, [this, image, available] {
        m_active = false;
        pinImage(image, m_region.topLeft(), available);
    });
    connect(preview, &LongCapturePreview::canceled, this, [this] {
        m_active = false;
        emit canceled();
    });
    connect(preview, &QObject::destroyed, this, [this] {
        m_active = false;
        m_preview = nullptr;
    });

    preview->show();
    preview->raise();
}

void LongCaptureSession::pinImage(const QImage& image, const QPoint& preferred,
                                  const QRect& available)
{
    QPixmap pixmap = QPixmap::fromImage(image);

    // 长图可能比屏幕还高，钉图前等比缩放到可用区域内
    const QSize logical = pixmap.size() / pixmap.devicePixelRatio();
    const QSize limit(int(available.width() * 0.9), int(available.height() * 0.9));
    if (logical.width() > limit.width() || logical.height() > limit.height()) {
        const qreal scale = qMin(qreal(limit.width()) / logical.width(),
                                 qreal(limit.height()) / logical.height());
        pixmap = pixmap.scaled(pixmap.size() * scale, Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
    }

    auto* pin = new PinWindow(pixmap);
    const QPoint position(
        qBound(available.left(), preferred.x(), available.right() - pin->width() + 1),
        qBound(available.top(), preferred.y(), available.bottom() - pin->height() + 1));
    pin->move(position);
    pin->show();
    pin->raise();
}
