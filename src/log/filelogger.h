// 模块 log：把 Qt 消息同时输出到控制台和用户数据目录下的日志文件。
#pragma once

#include <QString>

namespace FileLogger {

// 在 QApplication 创建并完成应用元信息设置后调用；重复调用无副作用。
void install();

// 当前日志文件路径；安装失败时返回空路径。
QString filePath();

} // namespace FileLogger
