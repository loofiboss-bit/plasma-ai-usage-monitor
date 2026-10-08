#include <QtTest>

#include "refreshschedulermodel.h"

class RefreshSchedulerModelTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
  void recoveryEventsCoalesce();
  void networkPauseIsExplicitAndUnknownAllowsAttempts();
  void deterministicJitter();
  void idleAndPopupIntervals();
  void retryBackoff();
  void freshnessAndNextSchedule();
};

void RefreshSchedulerModelTest::networkPauseIsExplicitAndUnknownAllowsAttempts() {
  RefreshSchedulerModel model;
  QVERIFY(!model.networkStateKnown());
  QVERIFY(model.externalRefreshAllowed());

  model.observeReachability(false);
  QVERIFY(model.networkStateKnown());
  QVERIFY(!model.externalRefreshAllowed());

  model.observeReachability(true);
  QVERIFY(model.networkStateKnown());
  QVERIFY(model.externalRefreshAllowed());
}

void RefreshSchedulerModelTest::recoveryEventsCoalesce() {
  RefreshSchedulerModel model;
  QSignalSpy recovered(&model, &RefreshSchedulerModel::recoveryRequested);
  const auto start = QDateTime(QDate(2026, 9, 7), QTime(12, 0), QTimeZone::UTC);
  model.observeWakeClock(start);
  model.observeWakeClock(start.addSecs(30));
  QCOMPARE(recovered.count(), 0);
  model.observeReachability(false);
  model.observeWakeClock(start.addSecs(3600));
  model.observeReachability(true);
  model.observeReachability(true);
  QTRY_COMPARE(recovered.count(), 1);
  model.observeWakeClock(start.addSecs(3630));
  model.observeReachability(true);
  QCOMPARE(recovered.count(), 1);
}

void RefreshSchedulerModelTest::deterministicJitter()
{
    RefreshSchedulerModel model;
    QCOMPARE(model.deterministicJitterMs(QStringLiteral("openai")),
             model.deterministicJitterMs(QStringLiteral("openai")));
    QVERIFY(model.deterministicJitterMs(QStringLiteral("openai"))
            != model.deterministicJitterMs(QStringLiteral("anthropic")));
}

void RefreshSchedulerModelTest::idleAndPopupIntervals()
{
    RefreshSchedulerModel model;
    QCOMPARE(model.effectiveIntervalMs(30, 60, true), 30000);
    QCOMPARE(model.effectiveIntervalMs(0, 60, false), 900000);
}

void RefreshSchedulerModelTest::retryBackoff()
{
    RefreshSchedulerModel model;
    QCOMPARE(model.backoffMultiplier(0, true), 1.0);
    QCOMPARE(model.backoffMultiplier(3, true), 8.0);
    QCOMPARE(model.backoffMultiplier(8, true), 8.0);
    QCOMPARE(model.backoffMultiplier(8, false), 4.0);
}

void RefreshSchedulerModelTest::freshnessAndNextSchedule()
{
    RefreshSchedulerModel model;
    const QDateTime last(QDate(2026, 7, 13), QTime(10, 0), QTimeZone::UTC);
    QVERIFY(model.isFresh(last, 60, 60, true, last.addSecs(30)));
    QVERIFY(!model.isFresh(last, 60, 60, true, last.addSecs(61)));
    const QDateTime next = model.nextScheduledRefresh(last, QStringLiteral("openai"),
                                                     60, 60, true, 0, false);
    QVERIFY(next > last.addSecs(59));
    QVERIFY(next < last.addSecs(100));
}

QTEST_MAIN(RefreshSchedulerModelTest)
#include "test_refreshscheduler.moc"
