// 模块 capture：长截图——定时抓取固定区域，与已拼接结果的底部缓冲区做重叠匹配。
#pragma once

#include <QImage>
#include <QObject>
#include <QRect>
#include <QVector>

class QTimer;

class ScrollCapture : public QObject {
    Q_OBJECT

public:
    explicit ScrollCapture(QObject* parent = nullptr);

    void start(const QRect& region);
    void finish(); // 停止抓取并输出拼接结果
    void cancel();

    bool isRunning() const { return m_running; }

signals:
    // 每次抓到新内容后发出：缩略图 + 已截取的逻辑高度
    void previewChanged(const QImage& thumbnail, int logicalHeight);
    void finished(const QImage& image);
    void canceled();

private:
    void scheduleNext();
    void captureFrame();
    // 返回与帧高度的重叠行数；-1 表示没滚动，0 表示没有可靠匹配
    int detectOverlap(const QImage& tail, const QImage& frame) const;
    void updateTail(const QImage& frame, int overlap);
    void appendThumbnail(const QImage& piece);
    QImage compose() const;

    QTimer* m_timer = nullptr;
    QRect m_region;
    QImage m_tail;            // 已拼接结果底部最多一屏的画面
    QImage m_thumbnail;       // 供侧边栏预览的缩略图，宽度固定
    QVector<QImage> m_pieces; // 每帧新增的行
    int m_totalHeight = 0;    // 物理像素
    qreal m_devicePixelRatio = 1.0;
    bool m_running = false;
};
