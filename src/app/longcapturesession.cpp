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
#include <QTimer>

namespace {
constexpr int kSidebarGap = 12;
constexpr int kSidebarHintMs = 500; // 外侧放不下时先短暂提示，再把侧边栏移出屏幕

// 尽量把侧边栏放在选区外侧，依次尝试右、左、下、上。
// 四面都放不下（选区几乎占满桌面）时返回 false。
bool placeSidebarOutside(const QRect& region, const QSize& size, QPoint* position)
{
    const QRect desktop = ScreenCapture::virtualGeometry();

    const int centeredY = qBound(desktop.top(), region.center().y() - size.height() / 2,
                                 desktop.bottom() - size.height() + 1);
    const int centeredX = qBound(desktop.left(), region.center().x() - size.width() / 2,
                                 desktop.right() - size.width() + 1);

    const int right = region.right() + kSidebarGap;
    if (right + size.width() <= desktop.right() + 1) {
        *position = QPoint(right, centeredY);
        return true;
    }

    const int left = region.left() - size.width() - kSidebarGap;
    if (left >= desktop.left()) {
        *position = QPoint(left, centeredY);
        return true;
    }

    const int bottom = region.bottom() + kSidebarGap;
    if (bottom + size.height() <= desktop.bottom() + 1) {
        *position = QPoint(centeredX, bottom);
        return true;
    }

    const int top = region.top() - size.height() - kSidebarGap;
    if (top >= desktop.top()) {
        *position = QPoint(centeredX, top);
        return true;
    }

    return false;
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
    connect(m_sidebar, &LongCaptureSidebar::finishRequested, this, &LongCaptureSession::finish);
    connect(m_sidebar, &LongCaptureSidebar::cancelRequested, this, &LongCaptureSession::cancel);
    connect(m_scroll, &ScrollCapture::finished, this, [this](const QImage& image) {
        closeWindows();
        cleanup();
        emit captureEnded();
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
        emit captureEnded();
        emit canceled();
    });

    // 侧边栏的尺寸由布局决定，show 之前必须先算出来，否则会按错误尺寸定位
    m_sidebar->adjustSize();
    QPoint sidebarPosition;
    const bool sidebarOutside = placeSidebarOutside(region, m_sidebar->size(), &sidebarPosition);
    if (sidebarOutside) {
        m_sidebar->move(sidebarPosition);
    } else {
        // 选区几乎占满桌面，外侧放不下侧边栏。先在角落短暂提示，
        // 随后把窗口移出屏幕：不遮挡截图内容，但仍保留键盘焦点，
        // 用 Enter 完成、Esc 取消。
        m_sidebar->move(region.topLeft() + QPoint(kSidebarGap, kSidebarGap));
        QTimer::singleShot(kSidebarHintMs, this, [this] {
            if (!m_sidebar)
                return;
            const QRect desktop = ScreenCapture::virtualGeometry();
            m_sidebar->move(desktop.right() + 40, m_sidebar->y());
        });
    }
    m_sidebar->show();
    m_sidebar->raise();
    m_sidebar->activateWindow();
    m_sidebar->setFocus();

    // 提示期间不抓帧，避免把侧边栏截进首屏
    const int firstDelayMs = sidebarOutside
        ? ScrollCapture::kFirstDelayMs
        : kSidebarHintMs + ScrollCapture::kFirstDelayMs;
    m_scroll->start(region, firstDelayMs);
}

void LongCaptureSession::finish()
{
    if (m_scroll)
        m_scroll->finish();
}

void LongCaptureSession::cancel()
{
    if (m_scroll)
        m_scroll->cancel();
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
    connect(preview, &LongCapturePreview::saveRequested, this, [this, image] {
        m_active = false;
        emit saveRequested(image);
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
    // 保留拼接原图，只把初始显示尺寸适配到屏幕；滚轮缩放基于原图，细节可放大查看
    const QPixmap pixmap = QPixmap::fromImage(image);
    const QSizeF logical = pixmap.deviceIndependentSize();
    const QSize limit(int(available.width() * 0.9), int(available.height() * 0.9));
    qreal fitScale = 1.0;
    if (!logical.isEmpty()) {
        fitScale = qMin(1.0, qMin(limit.width() / logical.width(),
                                  limit.height() / logical.height()));
    }

    auto* pin = new PinWindow(pixmap, fitScale);
    const QPoint position(
        qBound(available.left(), preferred.x(), available.right() - pin->width() + 1),
        qBound(available.top(), preferred.y(), available.bottom() - pin->height() + 1));
    pin->move(position);
    pin->show();
    pin->raise();
}
