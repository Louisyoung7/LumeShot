// 模块 update：解析 GitHub Releases Atom feed，与网络请求解耦，便于单独测试。
#pragma once

#include <QByteArray>
#include <QString>

namespace update {

struct ReleaseInfo {
    QString version;     // 纯版本号，如 "0.4.1"
    QString notes;       // 发布说明，已转为纯文本
    QString releaseUrl;  // Release 详情页地址
};

// 从 releases.atom 中取出最新一条发布；解析失败或没有发布时返回 false
bool parseAtomFeed(const QByteArray& xml, ReleaseInfo* info);

// 按当前 CPU 架构构造 deb 下载链接
QString debDownloadUrl(const QString& version);

} // namespace update
