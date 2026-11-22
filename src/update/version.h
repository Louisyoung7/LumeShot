// 模块 update：版本号比较，与网络请求解耦，便于单独测试。
#pragma once

#include <QString>

namespace update {

// remote 是否比 current 新；按数字段逐段比较，缺失的段按 0 处理
bool isNewerVersion(const QString& remote, const QString& current);

} // namespace update
