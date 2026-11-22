// 模块 platform：平台适配层——XDG 开机自启，通过 ~/.config/autostart 下的 .desktop 文件实现。
#pragma once

#include <QString>

class Autostart {
public:
    // 自启文件路径
    static QString desktopFilePath();

    // 是否已开启（自启文件是否存在）
    static bool isEnabled();

    // 开启时写入自启文件，关闭时删除；失败返回 false
    static bool setEnabled(bool enabled);
};
