#include <QtTest>

#include <QDateTime>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>

#include "usagedatabase.h"

namespace
{
constexpr int FixtureRows = 100000;

QString databasePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/plasma-ai-usage-monitor/usage_history.db");
}

QVariantMap sourceDescriptor()
{
    return {
        {QStringLiteral("historyId"), QStringLiteral("provider:Scale Provider")},
        {QStringLiteral("sourceKind"), QStringLiteral("provider")},
        {QStringLiteral("dbName"), QStringLiteral("Scale Provider")},
        {QStringLiteral("displayName"), QStringLiteral("Scale Provider")},
    };
}

bool seedObservations(const QDateTime &start)
{
    const QString connectionName =
        QStringLiteral("phase7_seed_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath());
        if (database.open() && database.transaction()) {
            QSqlQuery query(database);
            query.prepare(QStringLiteral(
                "INSERT INTO observations("
                "provider,observed_at_utc,metric_kind,unit,value,currency,semantic,"
                "source,data_quality,scope,window,model_scope,correlation_id) "
                "VALUES('Scale Provider',?,'cost','USD',?,'USD','interval_total',"
                "'billing_api','actual','account','calendar_day','fixture-model',?)"));
            ok = true;
            for (int index = 0; index < FixtureRows; ++index) {
                query.bindValue(0, start.addSecs(index * 60)
                                       .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
                query.bindValue(1, static_cast<double>(index % 100) / 100.0);
                query.bindValue(2, QStringLiteral("phase7-%1").arg(index));
                if (!query.exec()) {
                    qWarning() << query.lastError().text();
                    ok = false;
                    break;
                }
            }
            ok = ok && database.commit();
            if (!ok) {
                database.rollback();
            }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return ok;
}
}

class Phase7PerformanceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void databaseQueriesMeetReleaseBudgets();
    void supersededRequestsDrainWorkersAndConnections();

private:
    QTemporaryDir m_dataRoot;
    UsageDatabase m_database;
    QDateTime m_start;
};

void Phase7PerformanceTest::initTestCase()
{
    QVERIFY(m_dataRoot.isValid());
    qputenv("XDG_DATA_HOME", m_dataRoot.path().toUtf8());
    m_start = QDateTime::fromString(QStringLiteral("2026-01-01T00:00:00Z"), Qt::ISODate);
    m_database.init();
    QVERIFY2(seedObservations(m_start), "Could not create the deterministic Phase 7 fixture");
}

void Phase7PerformanceTest::databaseQueriesMeetReleaseBudgets()
{
    struct Budget {
        int rows;
        qint64 historyMilliseconds;
        qint64 analystMilliseconds;
    };
#ifdef NDEBUG
    const QList<Budget> budgets{
        {1000, 150, 250},
        {10000, 500, 700},
        {100000, 2000, 1000},
    };
#else
    const QList<Budget> budgets{
        {1000, 300, 500},
        {10000, 1500, 2000},
        {100000, 12000, 8000},
    };
#endif

    for (const Budget &budget : budgets) {
        const QDateTime to = m_start.addSecs((budget.rows - 1) * 60);
        QElapsedTimer timer;
        timer.start();
        const QVariantMap history = m_database.getHistorySeries(
            {sourceDescriptor()}, m_start.addSecs(-1), to.addSecs(1),
            QStringLiteral("cost"), 60);
        const qint64 historyElapsed = timer.elapsed();
        QVERIFY(history.value(QStringLiteral("ok")).toBool());
        const QVariantMap historySeries =
            history.value(QStringLiteral("series")).toList().constFirst().toMap();
        QCOMPARE(historySeries.value(QStringLiteral("sampleCount")).toInt(), budget.rows);
        QVERIFY(historySeries.value(QStringLiteral("availablePointCount")).toInt() <= 240);
        QVERIFY2(historyElapsed <= budget.historyMilliseconds,
                 qPrintable(QStringLiteral(
                     "Phase 7 history %1-row budget exceeded: %2 ms > %3 ms")
                                .arg(budget.rows)
                                .arg(historyElapsed)
                                .arg(budget.historyMilliseconds)));

        timer.restart();
        const QVariantMap analyst =
            m_database.getAnalystSnapshot(m_start.addSecs(-1), to.addSecs(1), QStringLiteral("USD"));
        const qint64 analystElapsed = timer.elapsed();
        QVERIFY(analyst.value(QStringLiteral("ok")).toBool());
        QCOMPARE(analyst.value(QStringLiteral("actualSampleCount")).toInt(), budget.rows);
        QVERIFY2(analystElapsed <= budget.analystMilliseconds,
                 qPrintable(QStringLiteral(
                     "Phase 7 Analyst %1-row budget exceeded: %2 ms > %3 ms")
                                .arg(budget.rows)
                                .arg(analystElapsed)
                                .arg(budget.analystMilliseconds)));

        qInfo().noquote()
            << QStringLiteral(
                   "PHASE7_METRIC rows=%1 history_ms=%2 analyst_ms=%3 "
                   "history_budget_ms=%4 analyst_budget_ms=%5")
                   .arg(budget.rows)
                   .arg(historyElapsed)
                   .arg(analystElapsed)
                   .arg(budget.historyMilliseconds)
                   .arg(budget.analystMilliseconds);
    }
}

void Phase7PerformanceTest::supersededRequestsDrainWorkersAndConnections()
{
    const int initialConnections = m_database.databaseConnectionCount();
    QSignalSpy historySpy(&m_database, &UsageDatabase::historySeriesReady);
    QSignalSpy analystSpy(&m_database, &UsageDatabase::analystReady);
    const QDateTime to = m_start.addSecs((FixtureRows - 1) * 60);
    const QVariantList sources{sourceDescriptor()};

    m_database.requestHistorySeries(QStringLiteral("history-old"), sources,
                                    m_start, to, QStringLiteral("cost"), 60);
    m_database.requestHistorySeries(QStringLiteral("history-new"), sources,
                                    m_start, to, QStringLiteral("cost"), 60);
    m_database.requestAnalyst(QStringLiteral("analyst-old"), m_start, to,
                              QStringLiteral("USD"));
    m_database.requestAnalyst(QStringLiteral("analyst-new"), m_start, to,
                              QStringLiteral("USD"));

    QVERIFY(m_database.pendingWorkerCount() >= 1);
    QTRY_VERIFY_WITH_TIMEOUT(historySpy.size() == 1, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(analystSpy.size() == 1, 30000);
    QCOMPARE(historySpy.constFirst().at(0).toString(), QStringLiteral("history-new"));
    QCOMPARE(analystSpy.constFirst().at(0).toString(), QStringLiteral("analyst-new"));
    QTRY_COMPARE_WITH_TIMEOUT(m_database.pendingWorkerCount(), 0, 30000);
    QTRY_COMPARE_WITH_TIMEOUT(m_database.databaseConnectionCount(), initialConnections, 30000);

    qInfo().noquote()
        << QStringLiteral("PHASE7_METRIC pending_workers=0 database_connections=%1")
               .arg(m_database.databaseConnectionCount());
}

QTEST_MAIN(Phase7PerformanceTest)
#include "test_phase7_performance.moc"
