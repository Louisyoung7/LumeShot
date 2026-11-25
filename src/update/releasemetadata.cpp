#include "update/releasemetadata.h"

#include <QRegularExpression>
#include <QSysInfo>
#include <QXmlStreamReader>

namespace {

QString decodeHtmlEntities(QString text)
{
    text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    // 最后处理 &amp;，避免把上面解码出来的 & 再次当作实体开头
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));

    static const QRegularExpression numeric(QStringLiteral("&#(x[0-9a-fA-F]+|[0-9]+);"));
    QList<QRegularExpressionMatch> matches;
    auto iterator = numeric.globalMatch(text);
    while (iterator.hasNext())
        matches.append(iterator.next());
    for (int i = matches.size() - 1; i >= 0; --i) {
        const QRegularExpressionMatch& match = matches.at(i);
        const QString value = match.captured(1);
        bool ok = false;
        const int code = value.startsWith(QLatin1Char('x'))
                             ? value.mid(1).toInt(&ok, 16)
                             : value.toInt(&ok);
        if (ok && code > 0 && code <= 0xFFFF)
            text.replace(match.capturedStart(), match.capturedLength(), QChar(code));
    }
    return text;
}

QString htmlToPlainText(const QString& html)
{
    QString text = html;
    text.replace(QRegularExpression(QStringLiteral("<br\\s*/?>")), QStringLiteral("\n"));
    text.replace(QRegularExpression(QStringLiteral("<li\\b[^>]*>")), QStringLiteral("• "));
    text.replace(QRegularExpression(
                     QStringLiteral("</?(?:p|li|h[1-6]|pre|div|tr|blockquote)\\b[^>]*>")),
                 QStringLiteral("\n"));
    text.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    text = decodeHtmlEntities(text);
    text.replace(QRegularExpression(QStringLiteral("[ \\t]+\n")), QStringLiteral("\n"));
    text.replace(QRegularExpression(QStringLiteral("\n{3,}")), QStringLiteral("\n\n"));
    return text.trimmed();
}

QString versionFromTag(const QString& tag)
{
    QString version = tag.trimmed();
    if (version.startsWith(QLatin1Char('v')))
        version.remove(0, 1);
    return version;
}

} // namespace

namespace update {

bool parseAtomFeed(const QByteArray& xml, ReleaseInfo* info)
{
    if (!info)
        return false;

    *info = ReleaseInfo{};
    QXmlStreamReader reader(xml);
    QString title;
    bool inEntry = false;

    while (!reader.atEnd() && !reader.hasError()) {
        reader.readNext();

        if (reader.isEndElement()) {
            if (inEntry && reader.name() == QLatin1String("entry"))
                break;
            continue;
        }
        if (!reader.isStartElement())
            continue;

        const QString name = reader.name().toString();
        if (name == QLatin1String("entry")) {
            inEntry = true;
            continue;
        }
        if (!inEntry)
            continue;

        if (name == QLatin1String("id")) {
            const QString id = reader.readElementText();
            const int slash = id.lastIndexOf(QLatin1Char('/'));
            if (slash >= 0)
                info->version = versionFromTag(id.mid(slash + 1));
        } else if (name == QLatin1String("link")) {
            const QXmlStreamAttributes attributes = reader.attributes();
            if (attributes.value(QLatin1String("rel")) == QLatin1String("alternate")
                && info->releaseUrl.isEmpty())
                info->releaseUrl = attributes.value(QLatin1String("href")).toString();
        } else if (name == QLatin1String("title")) {
            title = reader.readElementText();
        } else if (name == QLatin1String("content")) {
            info->notes = htmlToPlainText(reader.readElementText());
        }
    }

    if (reader.hasError())
        return false;

    // id 与 link 都取不到时再退回标题，例如 "LumeShot 0.4.0"
    if (info->version.isEmpty() && !info->releaseUrl.isEmpty()) {
        const int slash = info->releaseUrl.lastIndexOf(QLatin1Char('/'));
        if (slash >= 0)
            info->version = versionFromTag(info->releaseUrl.mid(slash + 1));
    }
    if (info->version.isEmpty()) {
        static const QRegularExpression versionPattern(
            QStringLiteral("(?:LumeShot\\s*)?v?(\\d+(?:\\.\\d+)+)"));
        const QRegularExpressionMatch match = versionPattern.match(title);
        if (match.hasMatch())
            info->version = match.captured(1);
    }

    return !info->version.isEmpty();
}

QString debDownloadUrl(const QString& version)
{
    QString architecture = QSysInfo::currentCpuArchitecture();
    if (architecture == QLatin1String("x86_64"))
        architecture = QStringLiteral("amd64");
    else if (architecture == QLatin1String("aarch64"))
        architecture = QStringLiteral("arm64");

    QString url = QStringLiteral(
        "https://github.com/Louisyoung7/LumeShot/releases/download/v%1/lumeshot_%1_%2.deb");
    url.replace(QLatin1String("%1"), version);
    url.replace(QLatin1String("%2"), architecture);
    return url;
}

} // namespace update
