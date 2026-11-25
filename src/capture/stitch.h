// 模块 capture：长截图拼接的核心判定——比较上一帧底部与当前帧顶部的重叠行数。
#pragma once

#include <QImage>

namespace stitch {

// 返回当前帧相对已有内容的重叠行数：
//   -1 画面没有滚动；
//    0 没有可靠匹配，应丢弃该帧；
//   >0 当前帧顶部与上一帧底部重合的行数。
//
// 判定会横向分栏比对，并抵消整体亮度变化，容忍编码噪声、固定页头、
// 小面积动画和滚动条。低纹理画面、横向抖动和反向滚动都会返回 0。
// 嵌在长图中无法与旧内容重叠的大段空白同样返回 0，这部分内容会被压缩，
// 属于当前版本的已知限制。
int detectOverlap(const QImage& tail, const QImage& frame);

} // namespace stitch
