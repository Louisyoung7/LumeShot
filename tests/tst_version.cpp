#include <QtTest>

#include "update/version.h"

class TestVersion : public QObject {
    Q_OBJECT

private slots:
    void compares_data();
    void compares();
};

void TestVersion::compares_data()
{
    QTest::addColumn<QString>("remote");
    QTest::addColumn<QString>("current");
    QTest::addColumn<bool>("newer");

    QTest::newRow("patch higher") << QStringLiteral("1.2.3") << QStringLiteral("1.2.2") << true;
    QTest::newRow("patch lower") << QStringLiteral("1.2.2") << QStringLiteral("1.2.3") << false;
    QTest::newRow("same") << QStringLiteral("1.2.3") << QStringLiteral("1.2.3") << false;
    QTest::newRow("missing patch") << QStringLiteral("1.2") << QStringLiteral("1.2.0") << false;
    QTest::newRow("numeric not lexical")
        << QStringLiteral("1.10") << QStringLiteral("1.9") << true;
    QTest::newRow("extra segment") << QStringLiteral("1.2.3.1") << QStringLiteral("1.2.3") << true;
    QTest::newRow("major higher") << QStringLiteral("2") << QStringLiteral("1.9.9") << true;
    QTest::newRow("major lower") << QStringLiteral("1") << QStringLiteral("2.0") << false;
}

void TestVersion::compares()
{
    QFETCH(QString, remote);
    QFETCH(QString, current);
    QFETCH(bool, newer);

    QCOMPARE(update::isNewerVersion(remote, current), newer);
}

QTEST_GUILESS_MAIN(TestVersion)

#include "tst_version.moc"
