// 离线复现长截图拼接：把 LUMESHOT_SCROLL_DEBUG 导出的帧序列重新跑一遍。
// 这样定位问题时不需要再手动滚动页面，算法改动也能直接对比前后结果。
#include "capture/scrollsession.h"

#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QTextStream>

#include <cstdio>

namespace {

const char* outcomeName(ScrollSession::Outcome outcome)
{
    switch (outcome) {
    case ScrollSession::Outcome::Ignored:
        return "ignored";
    case ScrollSession::Outcome::NoScroll:
        return "no-scroll";
    case ScrollSession::Outcome::NoMatch:
        return "no-match";
    case ScrollSession::Outcome::HeightLimit:
        return "height-limit";
    case ScrollSession::Outcome::Appended:
        return "appended";
    }
    return "unknown";
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QStringList args = app.arguments();
    if (args.size() < 2) {
        QTextStream(stderr) << "用法: lumeshot-replay <导出目录>\n"
                               "导出目录由 LUMESHOT_SCROLL_DEBUG=1 运行时自动生成。\n";
        return 1;
    }

    QDir sessionDir(args.at(1));
    QDir frameDir(sessionDir.filePath(QStringLiteral("frames")));
    if (!frameDir.exists()) {
        QTextStream(stderr) << "找不到帧目录: " << frameDir.absolutePath() << '\n';
        return 1;
    }

    const QStringList names =
        frameDir.entryList({QStringLiteral("*.png")}, QDir::Files, QDir::Name);
    if (names.isEmpty()) {
        QTextStream(stderr) << "帧目录为空: " << frameDir.absolutePath() << '\n';
        return 1;
    }

    ScrollSession session;
    QTextStream out(stdout);

    int index = 0;
    for (const QString& name : names) {
        const QImage frame =
            QImage(frameDir.filePath(name)).convertToFormat(QImage::Format_RGB32);
        ++index;
        if (frame.isNull()) {
            out << "frame=" << index << " 读取失败: " << name << '\n';
            continue;
        }

        const ScrollSession::Result result = session.addFrame(frame);
        out << "frame=" << index
            << " outcome=" << outcomeName(result.outcome)
            << " overlap=" << result.overlap
            << " misses=" << session.consecutiveMisses()
            << " size=" << frame.width() << 'x' << frame.height()
            << " total=" << session.totalHeight() << '\n';
    }

    const QImage composed = session.compose();
    const QString output = sessionDir.filePath(QStringLiteral("replay.png"));
    if (!composed.isNull() && composed.save(output, "PNG"))
        out << "结果已写入 " << output << '\n';

    out.flush();
    return 0;
}
