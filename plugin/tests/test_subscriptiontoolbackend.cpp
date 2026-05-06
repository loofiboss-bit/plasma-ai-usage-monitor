#include <QtTest>
#include <QSignalSpy>

#include "subscriptiontoolbackend.h"

/**
 * Minimal concrete subclass for testing SubscriptionToolBackend's non-virtual logic.
 */
class TestToolBackend : public SubscriptionToolBackend
{
    Q_OBJECT
public:
    explicit TestToolBackend(QObject *parent = nullptr)
        : SubscriptionToolBackend(parent)
        , m_hasSecondary(false)
    {}

    QString toolName() const override { return QStringLiteral("TestTool"); }
    QString iconName() const override { return QStringLiteral("test-tool-icon"); }
    QString toolColor() const override { return QStringLiteral("#FF0000"); }
    QString periodLabel() const override { return QStringLiteral("5-hour"); }
    void checkToolInstalled() override { setInstalled(true); }
    void detectActivity() override { /* no-op */ }
    QStringList availablePlans() const override { return {QStringLiteral("Free"), QStringLiteral("Pro")}; }
    int defaultLimitForPlan(const QString &plan) const override {
        return plan == QStringLiteral("Pro") ? 300 : 50;
    }

    // Allow tests to enable secondary limit
    void setHasSecondary(bool has) { m_hasSecondary = has; }
    bool hasSecondaryLimit() const override { return m_hasSecondary; }
    QString secondaryPeriodLabel() const override { return QStringLiteral("weekly"); }

    // Expose protected methods
    using SubscriptionToolBackend::setInstalled;
    using SubscriptionToolBackend::setUsageCount;
    using SubscriptionToolBackend::setSecondaryUsageCount;
    using SubscriptionToolBackend::setPeriodStart;
    using SubscriptionToolBackend::setSecondaryPeriodStart;
    using SubscriptionToolBackend::calculatePeriodEnd;
    using SubscriptionToolBackend::checkAndResetPeriod;

protected:
    UsagePeriod primaryPeriodType() const override { return FiveHour; }
    UsagePeriod secondaryPeriodType() const override { return Weekly; }

private:
    bool m_hasSecondary;
};

class SubscriptionToolBackendTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testIncrementUsage();
    void testResetUsage();
    void testPercentUsed();
    void testLimitReached();
    void testLimitReachedSignal();
    void testLimitWarningSignal();
    void testCalculatePeriodEndFiveHour();
    void testCalculatePeriodEndDaily();
    void testCalculatePeriodEndWeekly();
    void testCalculatePeriodEndMonthly();
    void testCalculatePeriodEndMonthlyCustomResetDay();
    void testCalculatePeriodEndInvalidStart();
    void testSecondaryUsageTracking();
    void testCheckAndResetPeriod();
    void testDefaultLimitForPlan();
    void testTimeUntilResetFormat();
    void testQuotaWindowsAndNotify();
};

void SubscriptionToolBackendTest::testIncrementUsage()
{
    TestToolBackend t;
    t.setUsageLimit(100);
    t.setPeriodStart(QDateTime::currentDateTimeUtc());

    QSignalSpy usageSpy(&t, &SubscriptionToolBackend::usageUpdated);
    QSignalSpy activitySpy(&t, &SubscriptionToolBackend::activityDetected);

    t.incrementUsage();

    QCOMPARE(t.usageCount(), 1);
    QVERIFY(usageSpy.count() >= 1);
    QCOMPARE(activitySpy.count(), 1);
    QCOMPARE(activitySpy.first().at(0).toString(), QStringLiteral("TestTool"));
    QVERIFY(t.lastActivity().isValid());
}

void SubscriptionToolBackendTest::testResetUsage()
{
    TestToolBackend t;
    t.setPeriodStart(QDateTime::currentDateTimeUtc());
    t.setUsageLimit(100);
    t.incrementUsage();
    t.incrementUsage();
    QCOMPARE(t.usageCount(), 2);

    QSignalSpy usageSpy(&t, &SubscriptionToolBackend::usageUpdated);
    t.resetUsage();

    QCOMPARE(t.usageCount(), 0);
    QVERIFY(t.periodStart().isValid());
    QVERIFY(usageSpy.count() >= 1);
}

void SubscriptionToolBackendTest::testPercentUsed()
{
    TestToolBackend t;

    // Zero limit — should return 0
    QVERIFY(qAbs(t.percentUsed()) < 0.01);

    t.setUsageLimit(100);
    t.setUsageCount(50);
    QVERIFY(qAbs(t.percentUsed() - 50.0) < 0.01);

    t.setUsageCount(100);
    QVERIFY(qAbs(t.percentUsed() - 100.0) < 0.01);
}

void SubscriptionToolBackendTest::testLimitReached()
{
    TestToolBackend t;
    t.setUsageLimit(5);

    QVERIFY(!t.isLimitReached());
    t.setUsageCount(4);
    QVERIFY(!t.isLimitReached());
    t.setUsageCount(5);
    QVERIFY(t.isLimitReached());
    t.setUsageCount(6);
    QVERIFY(t.isLimitReached());
}

void SubscriptionToolBackendTest::testLimitReachedSignal()
{
    TestToolBackend t;
    t.setUsageLimit(3);
    t.setPeriodStart(QDateTime::currentDateTimeUtc());

    QSignalSpy limitSpy(&t, &SubscriptionToolBackend::usageLimitReached);

    t.incrementUsage(); // 1
    t.incrementUsage(); // 2
    QCOMPARE(limitSpy.count(), 0);

    t.incrementUsage(); // 3 — limit reached
    QCOMPARE(limitSpy.count(), 1);
    QCOMPARE(limitSpy.first().at(0).toString(), QStringLiteral("TestTool"));
}

void SubscriptionToolBackendTest::testLimitWarningSignal()
{
    TestToolBackend t;
    t.setUsageLimit(10);
    t.setPeriodStart(QDateTime::currentDateTimeUtc());

    QSignalSpy warningSpy(&t, &SubscriptionToolBackend::limitWarning);

    // Increment to 7 — no warning yet (70%)
    for (int i = 0; i < 7; ++i) t.incrementUsage();
    QCOMPARE(warningSpy.count(), 0);

    // Increment to 8 — 80% triggers warning
    t.incrementUsage();
    QCOMPARE(warningSpy.count(), 1);
    QCOMPARE(warningSpy.first().at(0).toString(), QStringLiteral("TestTool"));
    QCOMPARE(warningSpy.first().at(1).toInt(), 80);
}

void SubscriptionToolBackendTest::testCalculatePeriodEndFiveHour()
{
    TestToolBackend t;
    QDateTime start(QDate(2026, 1, 15), QTime(10, 0, 0), QTimeZone::utc());
    QDateTime end = t.calculatePeriodEnd(SubscriptionToolBackend::FiveHour, start);

    QCOMPARE(end, start.addSecs(5 * 3600));
}

void SubscriptionToolBackendTest::testCalculatePeriodEndDaily()
{
    TestToolBackend t;
    QDateTime start(QDate(2026, 1, 15), QTime(10, 0, 0), QTimeZone::utc());
    QDateTime end = t.calculatePeriodEnd(SubscriptionToolBackend::Daily, start);

    QCOMPARE(end, start.addDays(1));
}

void SubscriptionToolBackendTest::testCalculatePeriodEndWeekly()
{
    TestToolBackend t;
    QDateTime start(QDate(2026, 1, 15), QTime(10, 0, 0), QTimeZone::utc());
    QDateTime end = t.calculatePeriodEnd(SubscriptionToolBackend::Weekly, start);

    QCOMPARE(end, start.addDays(7));
}

void SubscriptionToolBackendTest::testCalculatePeriodEndMonthly()
{
    TestToolBackend t;
    QDateTime start(QDate(2026, 1, 15), QTime(10, 0, 0), QTimeZone::utc());
    QDateTime end = t.calculatePeriodEnd(SubscriptionToolBackend::Monthly, start);

    // Should be 1st of next month at 00:00 UTC
    QDateTime expected(QDate(2026, 2, 1), QTime(0, 0, 0), QTimeZone::utc());
    QCOMPARE(end, expected);
}

void SubscriptionToolBackendTest::testCalculatePeriodEndMonthlyCustomResetDay()
{
    TestToolBackend t;
    t.setMonthlyResetDay(15);

    QDateTime beforeReset(QDate(2026, 1, 10), QTime(10, 0, 0), QTimeZone::utc());
    QCOMPARE(t.calculatePeriodEnd(SubscriptionToolBackend::Monthly, beforeReset),
             QDateTime(QDate(2026, 1, 15), QTime(0, 0, 0), QTimeZone::utc()));

    QDateTime afterReset(QDate(2026, 1, 20), QTime(10, 0, 0), QTimeZone::utc());
    QCOMPARE(t.calculatePeriodEnd(SubscriptionToolBackend::Monthly, afterReset),
             QDateTime(QDate(2026, 2, 15), QTime(0, 0, 0), QTimeZone::utc()));

    t.setMonthlyResetDay(31);
    QCOMPARE(t.monthlyResetDay(), 28);
}

void SubscriptionToolBackendTest::testCalculatePeriodEndInvalidStart()
{
    TestToolBackend t;
    QDateTime end = t.calculatePeriodEnd(SubscriptionToolBackend::FiveHour, QDateTime());
    QVERIFY(!end.isValid());
}

void SubscriptionToolBackendTest::testSecondaryUsageTracking()
{
    TestToolBackend t;
    t.setHasSecondary(true);
    t.setSecondaryUsageLimit(100);
    t.setPeriodStart(QDateTime::currentDateTimeUtc());
    t.setSecondaryPeriodStart(QDateTime::currentDateTimeUtc());
    t.setUsageLimit(50);

    // Increment should bump both primary and secondary
    t.incrementUsage();
    QCOMPARE(t.usageCount(), 1);
    QCOMPARE(t.secondaryUsageCount(), 1);

    t.setSecondaryUsageCount(99);
    QVERIFY(!t.isSecondaryLimitReached());
    t.setSecondaryUsageCount(100);
    QVERIFY(t.isSecondaryLimitReached());

    QVERIFY(qAbs(t.secondaryPercentUsed() - 100.0) < 0.01);
}

void SubscriptionToolBackendTest::testCheckAndResetPeriod()
{
    TestToolBackend t;
    // Set period start to 6 hours ago (past the 5-hour window)
    QDateTime oldStart = QDateTime::currentDateTimeUtc().addSecs(-6 * 3600);
    t.setPeriodStart(oldStart);
    t.setUsageLimit(100);
    t.setUsageCount(42);

    QSignalSpy usageSpy(&t, &SubscriptionToolBackend::usageUpdated);
    t.checkAndResetPeriod();

    // Usage should be reset because the period expired
    QCOMPARE(t.usageCount(), 0);
    QVERIFY(usageSpy.count() >= 1);
    // Period start should be updated to now (approximately)
    QVERIFY(t.periodStart().secsTo(QDateTime::currentDateTimeUtc()) < 2);
}

void SubscriptionToolBackendTest::testDefaultLimitForPlan()
{
    TestToolBackend t;
    QCOMPARE(t.defaultLimitForPlan(QStringLiteral("Pro")), 300);
    QCOMPARE(t.defaultLimitForPlan(QStringLiteral("Free")), 50);
}

void SubscriptionToolBackendTest::testTimeUntilResetFormat()
{
    TestToolBackend t;
    // Set period start to now — 5 hours from now
    t.setPeriodStart(QDateTime::currentDateTimeUtc());

    int secs = t.secondsUntilReset();
    // Should be approximately 5 hours (18000 secs) ± a few seconds
    QVERIFY(secs > 17990 && secs <= 18000);

    QString timeStr = t.timeUntilReset();
    // Should contain hours and minutes
    QVERIFY(timeStr.contains(QLatin1Char('h')));
}

void SubscriptionToolBackendTest::testQuotaWindowsAndNotify()
{
    TestToolBackend t;
    t.setPeriodStart(QDateTime::currentDateTimeUtc());

    QSignalSpy quotaSpy(&t, &SubscriptionToolBackend::quotaWindowsChanged);
    t.setUsageLimit(10);
    t.setUsageCount(4);

    QVERIFY(quotaSpy.count() >= 2);
    const QVariantList rows = t.quotaWindows();
    QVERIFY(!rows.isEmpty());
    const QVariantMap first = rows.first().toMap();
    QCOMPARE(first.value(QStringLiteral("source")).toString(), QStringLiteral("user_config"));
    QCOMPARE(first.value(QStringLiteral("badge")).toString(), QStringLiteral("Custom"));
    QCOMPARE(first.value(QStringLiteral("used")).toInt(), 4);
    QCOMPARE(first.value(QStringLiteral("limit")).toInt(), 10);
}

QTEST_MAIN(SubscriptionToolBackendTest)
#include "test_subscriptiontoolbackend.moc"
