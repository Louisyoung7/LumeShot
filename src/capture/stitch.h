// 模块 capture：长截图拼接的核心判定——比较上一帧底部与当前帧顶部的重叠行数。
#pragma once

#include <QImage>

namespace stitch {

// 返回当前帧相对已有内容的重叠行数；
// -1 表示画面没有滚动，0 表示没有可靠匹配、应丢弃该帧。
int detectOverlap(const QImage& tail, const QImage& frame);

} // namespace stitch
