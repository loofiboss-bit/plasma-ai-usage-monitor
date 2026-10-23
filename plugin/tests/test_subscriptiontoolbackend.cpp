#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

#include "subscriptiontoolbackend.h"
#include "localactivitymonitorbase.h"

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
    using SubscriptionToolBackend::calculatePeriodEnd;
    using SubscriptionToolBackend::checkAndResetPeriod;
    using SubscriptionToolBackend::recordSyncHttpFailure;
    using SubscriptionToolBackend::setHasSessionInfo;
    using SubscriptionToolBackend::setInstalled;
    using SubscriptionToolBackend::setPeriodStart;
    using SubscriptionToolBackend::setSecondaryPeriodStart;
    using SubscriptionToolBackend::setSecondaryUsageCount;
    using SubscriptionToolBackend::setSessionPercentUsed;
    using SubscriptionToolBackend::setSyncedQuotaWindows;
    using SubscriptionToolBackend::setUsageCount;

  protected:
    UsagePeriod primaryPeriodType() const override { return FiveHour; }
    UsagePeriod secondaryPeriodType() const override { return Weekly; }

private:
    bool m_hasSecondary;
};

/**
 * Minimal concrete subclass for testing LocalActivityMonitorBase.
 */
class TestLocalActivityMonitor : public LocalActivityMonitorBase
{
    Q_OBJECT
public:
    explicit TestLocalActivityMonitor(QObject *parent = nullptr)
        : LocalActivityMonitorBase(parent)
    {}

    QString toolName() const override { return QStringLiteral("TestLocalMonitor"); }
    QString iconName() const override { return QStringLiteral("test-icon"); }
    QString toolColor() const override { return QStringLiteral("#00FF00"); }
    QString periodLabel() const override { return QStringLiteral("monthly"); }
    QStringList availablePlans() const override { return {QStringLiteral("Free"), QStringLiteral("Pro")}; }
    int defaultLimitForPlan(const QString &plan) const override {
        return plan == QStringLiteral("Pro") ? 500 : 100;
    }

    using LocalActivityMonitorBase::setInstallExecutableNames;
    using LocalActivityMonitorBase::setInstallPaths;
    using LocalActivityMonitorBase::setWatchedPaths;
    using LocalActivityMonitorBase::setIgnoredPathSuffixes;
    using LocalActivityMonitorBase::setDebounceIntervalMs;
    using LocalActivityMonitorBase::watchedPaths;
    using LocalActivityMonitorBase::updateWatchHealth;

protected:
    UsagePeriod primaryPeriodType() const override { return Monthly; }
};

class SubscriptionToolBackendTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
  void testAutomaticSyncRetryPolicy();
  void testQuotaIdentityAndMalformedValues();
  void testEqualSessionObservationNotifies();
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
  void testLocalActivityMonitorHealthAndDiagnostics();
  void testLocalActivityMonitorSetters();
  void testLocalActivityMonitorCheckInstalled();
  void testLocalActivityMonitorDetectActivity();
};

void SubscriptionToolBackendTest::testAutomaticSyncRetryPolicy() {
  TestToolBackend tool;
  const auto now = QDateTime::currentDateTimeUtc();
  QVERIFY(tool.canAutoSyncAt(now));
  tool.recordSyncHttpFailure(429, "120", now);
  QVERIFY(!tool.canAutoSyncAt(now.addSecs(119)));
  QVERIFY(tool.canAutoSyncAt(now.addSecs(120)));
  tool.recordSyncHttpFailure(401, {}, now);
  QVERIFY(!tool.canAutoSyncAt(now.addDays(1)));
  tool.resetSyncRetry();
  tool.recordSyncHttpFailure(0, {}, now);
  QVERIFY(!tool.canAutoSyncAt(now.addSecs(59)));
  QVERIFY(tool.canAutoSyncAt(now.addSecs(60)));
  Q_EMIT tool.syncCompleted(true, QString());
  QVERIFY(tool.canAutoSyncAt(now));
}

void SubscriptionToolBackendTest::testQuotaIdentityAndMalformedValues() {
  TestToolBackend tool;
  QVariantMap first{{"kind", "quota"},
                    {"window", "five_hour"},
                    {"source", "browser_sync"},
                    {"percentRemaining", 0.0}};
  QVariantMap second = first;
  second.insert("window", "weekly");
  second.insert("percentRemaining", 50.0);
  tool.setSyncedQuotaWindows({first, second});
  QCOMPARE(tool.quotaWindows().size(),
           2); // No local activity or configured target.
  const auto observed = tool.lastQuotaObservation();
  for (const QVariant &invalid : {QVariant(true), QVariant("10"), QVariant(),
                                  QVariant(-1), QVariant(101)}) {
    auto malformed = first;
    malformed.insert("percentRemaining", invalid);
    tool.setSyncedQuotaWindows({malformed});
  }
  auto inconsistent = first;
  inconsistent.insert("percentUsed", 1.0);
  tool.setSyncedQuotaWindows({inconsistent});
  QCOMPARE(tool.lastQuotaObservation(), observed);
  QCOMPARE(
      tool.quotaWindows().first().toMap().value("percentRemaining").toDouble(),
      0.0);
}

void SubscriptionToolBackendTest::testEqualSessionObservationNotifies() {
  TestToolBackend tool;
  tool.setHasSessionInfo(true);
  tool.setSessionPercentUsed(0);
  QSignalSpy spy(&tool, &SubscriptionToolBackend::usageUpdated);
  tool.setSessionPercentUsed(0);
  QCOMPARE(spy.size(), 1);
}

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

void SubscriptionToolBackendTest::testLocalActivityMonitorHealthAndDiagnostics()
{
    TestLocalActivityMonitor monitor;
    QCOMPARE(monitor.watchDiagnosticCode(), QString());

    QSignalSpy spy(&monitor, &LocalActivityMonitorBase::watchHealthChanged);

    monitor.updateWatchHealth(true);
    QCOMPARE(monitor.watchDiagnosticCode(), QStringLiteral("watch_incomplete"));
    QCOMPARE(spy.count(), 1);

    // Call updateWatchHealth(true) again with no change -> should not re-emit signal
    monitor.updateWatchHealth(true);
    QCOMPARE(spy.count(), 1);

    monitor.updateWatchHealth(false);
    QCOMPARE(monitor.watchDiagnosticCode(), QString());
    QCOMPARE(spy.count(), 2);
}

void SubscriptionToolBackendTest::testLocalActivityMonitorSetters()
{
    TestLocalActivityMonitor monitor;

    const QStringList paths = {QStringLiteral("/tmp/path1"), QStringLiteral("/tmp/path2")};
    monitor.setWatchedPaths(paths);
    QCOMPARE(monitor.watchedPaths(), paths);

    // Re-setting same watched paths should be a no-op
    monitor.setWatchedPaths(paths);
    QCOMPARE(monitor.watchedPaths(), paths);

    const QStringList suffixes = {QStringLiteral(".log"), QStringLiteral(".tmp")};
    monitor.setIgnoredPathSuffixes(suffixes);
    monitor.setIgnoredPathSuffixes(suffixes);

    monitor.setDebounceIntervalMs(100);
    monitor.setInstallExecutableNames({QStringLiteral("nonexistent_binary_xyz_123")});
    monitor.setInstallPaths({QStringLiteral("/nonexistent/path/xyz/123")});
}

void SubscriptionToolBackendTest::testLocalActivityMonitorCheckInstalled()
{
    TestLocalActivityMonitor monitor;

    monitor.setInstallExecutableNames({QStringLiteral("nonexistent_binary_xyz_123")});
    monitor.setInstallPaths({QStringLiteral("/nonexistent/path/xyz/123")});
    monitor.checkToolInstalled();
    QVERIFY(!monitor.isInstalled());

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dummyFile = tempDir.filePath(QStringLiteral("test_tool"));
    QFile file(dummyFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("dummy");
    file.close();

    monitor.setInstallPaths({dummyFile});
    monitor.checkToolInstalled();
    QVERIFY(monitor.isInstalled());
}

void SubscriptionToolBackendTest::testLocalActivityMonitorDetectActivity()
{
    TestLocalActivityMonitor monitor;
    monitor.setEnabled(true);
    monitor.setWatchedPaths({QStringLiteral("/tmp")});

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dummyFile = tempDir.filePath(QStringLiteral("test_exec"));
    QFile file(dummyFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("dummy");
    file.close();

    monitor.setInstallPaths({dummyFile});
    monitor.checkToolInstalled();
    QVERIFY(monitor.isInstalled());

    monitor.setDebounceIntervalMs(250);
    monitor.detectActivity();
}

QTEST_MAIN(SubscriptionToolBackendTest)
#include "test_subscriptiontoolbackend.moc"
