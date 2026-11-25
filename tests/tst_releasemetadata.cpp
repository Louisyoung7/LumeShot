#include <QtTest>

#include "update/releasemetadata.h"

class TestReleaseMetadata : public QObject {
    Q_OBJECT

private slots:
    void parsesLatestEntry();
    void fallsBackToTitle();
    void rejectsInvalidFeed();
    void buildsDownloadUrl();
};

void TestReleaseMetadata::parsesLatestEntry()
{
    const QByteArray feed = R"XML(
<feed xmlns="http://www.w3.org/2005/Atom">
  <id>tag:github.com,2008:https://github.com/Louisyoung7/LumeShot/releases</id>
  <title>Release notes from LumeShot</title>
  <entry>
    <id>tag:github.com,2008:Repository/1466538223/v0.4.1</id>
    <updated>2026-11-25T09:01:33Z</updated>
    <link rel="alternate" type="text/html"
          href="https://github.com/Louisyoung7/LumeShot/releases/tag/v0.4.1"/>
    <title>v0.4.1</title>
    <content type="html"><![CDATA[
<h2>LumeShot 0.4.1</h2>
<p>修复检查更新功能 &amp; 提升稳定性。</p>
<ul>
<li>改用 Atom feed</li>
<li>不再受 API 限流影响</li>
</ul>
]]></content>
  </entry>
  <entry>
    <id>tag:github.com,2008:Repository/1466538223/v0.4.0</id>
    <link rel="alternate" type="text/html"
          href="https://github.com/Louisyoung7/LumeShot/releases/tag/v0.4.0"/>
    <title>LumeShot 0.4.0</title>
    <content type="html"><![CDATA[<p>旧版本</p>]]></content>
  </entry>
</feed>
)XML";

    update::ReleaseInfo info;
    QVERIFY(update::parseAtomFeed(feed, &info));
    QCOMPARE(info.version, QStringLiteral("0.4.1"));
    QCOMPARE(info.releaseUrl,
             QStringLiteral("https://github.com/Louisyoung7/LumeShot/releases/tag/v0.4.1"));
    QVERIFY(info.notes.contains(QStringLiteral("修复检查更新功能 & 提升稳定性。")));
    QVERIFY(info.notes.contains(QStringLiteral("改用 Atom feed")));
    QVERIFY(!info.notes.contains(QLatin1String("<h2>")));
    QVERIFY(!info.notes.contains(QLatin1String("<li>")));
}

void TestReleaseMetadata::fallsBackToTitle()
{
    const QByteArray feed = R"XML(
<feed xmlns="http://www.w3.org/2005/Atom">
  <entry>
    <title>LumeShot 0.3.0</title>
  </entry>
</feed>
)XML";

    update::ReleaseInfo info;
    QVERIFY(update::parseAtomFeed(feed, &info));
    QCOMPARE(info.version, QStringLiteral("0.3.0"));
}

void TestReleaseMetadata::rejectsInvalidFeed()
{
    update::ReleaseInfo info;
    QVERIFY(!update::parseAtomFeed(QByteArray("<feed><entry>"), &info));

    const QByteArray emptyFeed = "<feed xmlns=\"http://www.w3.org/2005/Atom\"><title>none</title></feed>";
    QVERIFY(!update::parseAtomFeed(emptyFeed, &info));
}

void TestReleaseMetadata::buildsDownloadUrl()
{
    const QString url = update::debDownloadUrl(QStringLiteral("0.4.1"));
    QVERIFY(url.startsWith(
        QStringLiteral("https://github.com/Louisyoung7/LumeShot/releases/download/v0.4.1/lumeshot_0.4.1_")));
    QVERIFY(url.endsWith(QLatin1String(".deb")));
}

QTEST_GUILESS_MAIN(TestReleaseMetadata)

#include "tst_releasemetadata.moc"
