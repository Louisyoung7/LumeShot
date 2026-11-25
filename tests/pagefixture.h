// 测试夹具：生成确定的长页面图像，像素由绝对行列决定，便于逐像素校验拼接结果。
#pragma once

#include <QImage>

namespace fixture {

constexpr int kPageWidth = 400;
constexpr int kPageHeight = 1600;
constexpr int kFrameHeight = 300;
constexpr int kHeaderRows = 40;

// 页面纹理：色带按绝对行号编码，色带内用短色块制造纵向结构，
// 同时每个像素带横向变化，横向抖动会产生明显差异。
inline QRgb pageColor(int row, int x)
{
    const int band = row / 20;
    const int local = row % 20;
    const int blockWidth = 6 + (band % 7) * 3;
    const int block = x / blockWidth;
    const int phase = (block * 7 + band * 5) % 20;
    const bool inBlock = ((local - phase + 20) % 20) < 14;

    int r = (band * 53 + x * 3 + block * 17 + (row * 7) % 41 + 23) % 180 + 40;
    int g = (band * 97 + x * 5 + block * 29 + (row * 11) % 43 + 61) % 180 + 40;
    int b = (band * 29 + x * 7 + block * 11 + (row * 13) % 47 + 97) % 180 + 40;
    if (inBlock) {
        r = (r + 70) % 200 + 20;
        g = (g + 90) % 200 + 20;
        b = (b + 50) % 200 + 20;
    }
    return qRgb(r, g, b);
}

// 固定页头：深色条加文字状亮块
inline QRgb headerColor(int row, int x)
{
    int base = 25 + (x * 5 + row * 3) % 20;
    if (row >= 8 && row < 32 && (x / 10) % 4 != 0)
        base += 120;
    return qRgb(base, base + 10, base + 20);
}

// 按绝对行偏移裁剪一屏。pinnedHeader 时顶部 40 行固定，正文从 offset + 行号开始。
inline QImage pageFrame(int offset, int height = kFrameHeight, int width = kPageWidth,
                        bool pinnedHeader = false)
{
    QImage image(width, height, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        const bool useHeader = pinnedHeader && y < kHeaderRows;
        for (int x = 0; x < width; ++x)
            line[x] = useHeader ? headerColor(y, x) : pageColor(offset + y, x);
    }
    return image;
}

// 低纹理页面：大面积浅色渐变，只有左上角一小块纹理，且只落在一列
inline QImage lowTextureFrame(int offset, int height = kFrameHeight)
{
    QImage image(kPageWidth, height, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        const int row = offset + y;
        const int base = 225 - (row / 40) * 3 - (row % 40) / 14;
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < kPageWidth; ++x) {
            if (row >= 200 && row < 240 && x >= 20 && x < 80) {
                const int v = ((x / 8 + (row - 200) / 8) % 2) ? 60 : 200;
                line[x] = qRgb(v, v, v);
            } else {
                line[x] = qRgb(base, base, base + (x % 7 == 0 ? 1 : 0));
            }
        }
    }
    return image;
}

// 周期纹理：每 100 行重复一次，用于制造多解
inline QImage repeatedFrame(int offset, int height = kFrameHeight)
{
    QImage image(kPageWidth, height, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < kPageWidth; ++x)
            line[x] = pageColor((offset + y) % 100, x);
    }
    return image;
}

// 模拟真实文章：白底、窄栏居中文字，每行内容不同
inline QImage articleFrame(int offset, int height = 800, int width = 800)
{
    QImage image(width, height, QImage::Format_RGB32);
    const int left = width / 2 - 60;
    const int right = width / 2 + 60;

    for (int y = 0; y < height; ++y) {
        const int row = offset + y;
        const int line = row / 28;
        const bool textRow = row % 28 < 16;
        QRgb* pixels = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < width; ++x) {
            int value = 245 - (row % 5);
            if (textRow && x >= left && x < right && (x + line * 37) % 11 < 6)
                value = 40 + (row % 17);
            pixels[x] = qRgb(value, value, value);
        }
    }
    return image;
}

inline QImage withBrightness(const QImage& source, int delta)
{
    QImage result = source;
    for (int y = 0; y < result.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            line[x] = qRgb(qBound(0, qRed(line[x]) + delta, 255),
                           qBound(0, qGreen(line[x]) + delta, 255),
                           qBound(0, qBlue(line[x]) + delta, 255));
        }
    }
    return result;
}

inline QImage withNoise(const QImage& source, int amplitude)
{
    QImage result = source;
    for (int y = 0; y < result.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const int delta = (y * 131 + x * 17) % (2 * amplitude + 1) - amplitude;
            line[x] = qRgb(qBound(0, qRed(line[x]) + delta, 255),
                           qBound(0, qGreen(line[x]) + delta, 255),
                           qBound(0, qBlue(line[x]) + delta, 255));
        }
    }
    return result;
}

inline QImage shiftedHorizontally(const QImage& source, int dx)
{
    QImage result(source.size(), QImage::Format_RGB32);
    result.fill(Qt::black);
    for (int y = 0; y < result.height(); ++y) {
        QRgb* dst = reinterpret_cast<QRgb*>(result.scanLine(y));
        const QRgb* src = reinterpret_cast<const QRgb*>(source.constScanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const int sx = x - dx;
            if (sx >= 0 && sx < source.width())
                dst[x] = src[sx];
        }
    }
    return result;
}

// 动画条：指定行范围内画随 phase 变化的纹理，模拟轮播或加载动画
inline QImage withAnimation(const QImage& source, int phase, int startRow = 100, int rows = 30)
{
    QImage result = source;
    for (int y = startRow; y < startRow + rows && y < result.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const int v = ((x / 6 + y + phase * 13) % 2) ? 30 : 230;
            line[x] = qRgb(v, 255 - v, (x * 3 + phase * 7) % 256);
        }
    }
    return result;
}

// 底部若干行填充为背景色，模拟页面到底后回弹露出的空白
inline QImage withBlankBottom(const QImage& source, int rows,
                              QRgb color = qRgb(252, 252, 252))
{
    QImage result = source;
    const int start = qMax(0, result.height() - rows);
    for (int y = start; y < result.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x)
            line[x] = color;
    }
    return result;
}

// 右侧滚动条：最右 8 列随 phase 改变位置和亮度
inline QImage withScrollbar(const QImage& source, int phase)
{
    QImage result = source;
    const int width = result.width();
    for (int y = 0; y < result.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        const int thumbTop = 30 + phase * 25;
        for (int x = width - 8; x < width; ++x) {
            const bool thumb = y >= thumbTop && y < thumbTop + 80;
            const int v = thumb ? 180 : 70;
            line[x] = qRgb(v, v, v);
        }
    }
    return result;
}

} // namespace fixture
