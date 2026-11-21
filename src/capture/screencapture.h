// 模块 capture：抓屏能力层——整屏抓取、区域裁剪、活动窗口几何（不依赖 UI）。
#pragma once

#include <QPixmap>
#include <QRect>

// X11 下的屏幕抓取工具：整屏抓取、区域裁剪、活动窗口几何。
namespace ScreenCapture {

// 所有显示器拼起来的虚拟桌面范围（Qt 逻辑坐标）
QRect virtualGeometry();

// 抓取整个虚拟桌面，返回的 pixmap 覆盖 virtualGeometry
QPixmap grabDesktop();

// 从桌面图里裁剪一块区域，rect 使用桌面坐标（相对 virtualGeometry 左上角）
QPixmap crop(const QPixmap& desktop, const QRect& rect);

// 当前活动窗口在桌面坐标下的矩形（含窗口边框），失败返回 false
bool activeWindowRect(QRect* rect);

} // namespace ScreenCapture
