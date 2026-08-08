pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.plasmoid
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0
import "components" as Components

Kirigami.ScrollablePage {
    id: analystPage
    required property var monitor

    title: i18n("Insights")
    Accessible.name: analystState.loading
        ? i18n("Insights view loading")
        : i18n("Insights view ready")

    readonly property bool mediaScenario: AppInfo.demoMode
        && AppInfo.smokeView.indexOf("media-analyst") === 0
    readonly property bool mediaSufficient:
        AppInfo.smokeView === "media-analyst-sufficient"
    readonly property var db: analystPage.monitor
        && (Plasmoid.configuration.historyEnabled || mediaScenario)
        ? analystPage.monitor.usageDb
        : null
    readonly property var runwayRows: pacingAndRunwayRows()

    function mediaSnapshot() {
        var sufficient = mediaSufficient;
        var spend = [];
        for (var day = 0; day < (sufficient ? 20 : 1); ++day) {
            spend.push({
                date: "2026-07-" + String(day + 1).padStart(2, "0"),
                actualAvailable: true,
                actual: sufficient ? 0.34 + (day % 5) * 0.09 : 0.42,
                estimatedAvailable: false,
                estimated: null
            });
        }
        return {
            ok: true,
            from: "2026-06-27T00:00:00Z",
            to: "2026-07-27T00:00:00Z",
            coverage: {
                observedDayCount: sufficient ? 20 : 1,
                requestedDayCount: 30,
                percent: sufficient ? 67 : 3
            },
            currencyStatus: "single",
            currency: "USD",
            actualSampleCount: sufficient ? 20 : 1,
            estimatedSampleCount: 0,
            spendSeries: spend,
            activityAvailable: false,
            topDrivers: sufficient ? [{
                provider: "OpenRouter",
                model: "account",
                quality: "actual",
                value: 8.46,
                currency: "USD"
            }] : [],
            kpis: {
                averageDailySpend: sufficient
                    ? { available: true, value: 0.56 }
                    : {
                        available: false,
                        reasonKey: "insufficient_day_samples",
                        sampleCount: 1,
                        minimumSamples: 3
                    },
                weekOverWeekChange: sufficient
                    ? { available: true, value: 12.4 }
                    : {
                        available: false,
                        reasonKey: "incomplete_comparison_windows",
                        sampleCount: 1,
                        minimumSamples: 14
                    },
                volatility: sufficient
                    ? { available: true, value: 18.2 }
                    : {
                        available: false,
                        reasonKey: "insufficient_volatility_samples",
                        sampleCount: 1,
                        minimumSamples: 7
                    },
                outputInputRatio: {
                    available: false,
                    reasonKey: "insufficient_ratio_samples",
                    sampleCount: sufficient ? 0 : 1,
                    minimumSamples: 3
                }
            }
        };
    }

    function pacingAndRunwayRows() {
        if (!Plasmoid.configuration.forecastUiEnabled) return [];
        if (mediaScenario) {
            return [{
                kind: "budget_overrun",
                state: mediaSufficient ? "warning" : "unavailable",
                sourceId: "openrouter",
                sourceKind: "provider",
                window: "calendar_month",
                scope: "organization",
                currentValue: mediaSufficient ? 8.46 : null,
                projectedValue: mediaSufficient ? 18.72 : null,
                limitValue: mediaSufficient ? 15.0 : null,
                unit: "USD",
                currency: "USD",
                contractVersion: "budget-pacing-v2",
                policyId: "media-openrouter-monthly",
                periodStart: "2026-07-01T00:00:00Z",
                predictedAt: mediaSufficient
                    ? "2026-07-29T10:00:00Z" : null,
                periodEnd: "2026-08-01T00:00:00Z",
                spentMinor: mediaSufficient ? 846 : null,
                remainingMinor: mediaSufficient ? 654 : null,
                consumedPercent: mediaSufficient ? 56.4 : null,
                projectedPeriodEndMinor: mediaSufficient ? 1872 : null,
                predictedOverrun: mediaSufficient,
                safeTodayMinor: mediaSufficient ? 32 : null,
                remainingDailyAllowanceMinor: mediaSufficient ? 55 : null,
                previousPeriodSpentMinor: mediaSufficient ? 762 : null,
                previousPeriodChangePercent: mediaSufficient ? 11.0 : null,
                sampleCount: mediaSufficient ? 20 : 1,
                coveragePercent: mediaSufficient ? 80 : 3,
                evidenceGrade: mediaSufficient ? "strong" : "unavailable",
                methodId: "budget-pacing-v2",
                reasonKey: mediaSufficient ? "" : "insufficient_samples",
                reasonText: mediaSufficient ? ""
                    : i18n("Not enough compatible observations"),
                valueClass: "actual"
            }];
        }
        var rows = analystPage.monitor && analystPage.monitor.guardrails
            ? (analystPage.monitor.guardrails.forecasts || []).slice() : [];
        var rank = { critical: 0, warning: 1, safe: 2, unavailable: 3 };
        rows.sort(function(left, right) {
            return Number(rank[left.state] === undefined ? 4 : rank[left.state])
                - Number(rank[right.state] === undefined
                         ? 4 : rank[right.state]);
        });
        return rows.slice(0, 8);
    }
    Components.AnalystState {
        id: analystState
        db: analystPage.db
        onReportReady: function(report) {
            clipboard.setText(report);
        }
    }

    onDbChanged: if (!mediaScenario && db !== null) analystState.refreshData()
    Component.onCompleted: {
        if (mediaScenario) {
            analystState.snapshot = mediaSnapshot();
            analystState.loading = false;
        } else {
            analystState.refreshData();
        }
    }
    onVisibleChanged: if (visible && analystState.reportRequestId === "")
        analystState.refreshData()

    actions: [
        Kirigami.Action {
            icon.name: "view-refresh"
            text: i18n("Refresh")
            enabled: analystState.reportRequestId === ""
            onTriggered: {
                analystState.refreshData();
                analystPage.monitor.refreshGuardrails();
            }
        },
        Kirigami.Action {
            icon.name: "edit-copy"
            text: i18n("Copy 7-day report")
            enabled: analystPage.db !== null
                && !analystState.loading
                && analystState.reportRequestId === ""
            onTriggered: analystState.requestReport(7)
        },
        Kirigami.Action {
            icon.name: "edit-copy"
            text: i18n("Copy 30-day report")
            enabled: analystPage.db !== null
                && !analystState.loading
                && analystState.reportRequestId === ""
            onTriggered: analystState.requestReport(30)
        }
    ]

    ClipboardHelper {
        id: clipboard
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Controls.BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            visible: analystState.loading
            running: visible
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: !analystState.loading && !analystState.hasSnapshot
            type: Kirigami.MessageType.Information
            text: analystState.reasonText(analystState.snapshot.errorKey || "history_unavailable",
                             0, 0)
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            visible: Plasmoid.configuration.forecastUiEnabled
            text: i18n("Pacing and runway")
            level: 3
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: Plasmoid.configuration.forecastUiEnabled
                && analystPage.runwayRows.length === 0
                && !(analystPage.monitor
                     && analystPage.monitor.guardrails
                     && analystPage.monitor.guardrails.busy)
            type: Kirigami.MessageType.Information
            text: i18n("No compatible runway forecast is available for the current local history and guardrail settings.")
            Accessible.name: text
        }

        Controls.BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            visible: Plasmoid.configuration.forecastUiEnabled
                && analystPage.monitor
                && analystPage.monitor.guardrails
                && analystPage.monitor.guardrails.busy
                && analystPage.runwayRows.length === 0
            running: visible
        }

        Repeater {
            model: analystPage.runwayRows

            RunwayCard {
                required property var modelData
                Layout.fillWidth: true
                forecast: modelData
            }
        }

        Kirigami.Card {
            Layout.fillWidth: true
            visible: analystState.hasSnapshot

            header: Kirigami.Heading {
                text: i18n("Data coverage")
                level: 3
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                Controls.Label {
                    text: i18n("Period: %1 – %2",
                               analystState.formatDate(analystState.snapshot.from),
                               analystState.formatDate(new Date(
                                   new Date(analystState.snapshot.to).getTime()
                                   - 1)))
                    color: Kirigami.Theme.disabledTextColor
                }

                Controls.Label {
                    objectName: "coverageLabel"
                    text: i18n("%1 of %2 days contain compatible observations (%3%)",
                               analystState.coverage.observedDayCount || 0,
                               analystState.coverage.requestedDayCount || 30,
                               Number(analystState.coverage.percent || 0).toFixed(0))
                    wrapMode: Text.WordWrap
                }

                Controls.Label {
                    text: i18n("%1 actual and %2 estimated compatible cost samples",
                               analystState.snapshot.actualSampleCount || 0,
                               analystState.snapshot.estimatedSampleCount || 0)
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: analystState.hasSnapshot
                && !analystState.kpi("averageDailySpend").available
            type: analystState.snapshot.mixedCurrencies
                ? Kirigami.MessageType.Warning
                : Kirigami.MessageType.Information
            text: analystState.reasonText(
                analystState.kpi("averageDailySpend").reasonKey,
                analystState.kpi("averageDailySpend").sampleCount,
                analystState.kpi("averageDailySpend").minimumSamples)
        }

        Kirigami.Card {
            Layout.fillWidth: true
            visible: analystState.hasSnapshot
                && analystState.kpi("averageDailySpend").available

            header: Kirigami.Heading {
                text: i18n("Spend trend")
                level: 3
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.mediumSpacing

                Kirigami.CardsLayout {
                    id: kpiCards
                    Layout.fillWidth: true
                    maximumColumns: 3
                    minimumColumnWidth: Kirigami.Units.gridUnit * 11
                    maximumColumnWidth: Kirigami.Units.gridUnit * 18

                    Kirigami.AbstractCard {
                        contentItem: ColumnLayout {
                            Controls.Label {
                                Layout.fillWidth: true
                                text: analystState.averageSpendLabel(
                                    analystState.snapshot)
                                color: Kirigami.Theme.disabledTextColor
                                wrapMode: Text.WordWrap
                            }
                            Controls.Label {
                                objectName: "averageSpendValue"
                                text: analystState.formatMoney(
                                    analystState.kpi("averageDailySpend").value,
                                    analystState.snapshot.currency)
                                font.pointSize: 20
                                font.weight: Font.Bold
                            }
                        }
                    }

                    Kirigami.AbstractCard {
                        contentItem: ColumnLayout {
                            Controls.Label {
                                text: i18n("Week over week")
                                color: Kirigami.Theme.disabledTextColor
                            }
                            Controls.Label {
                                text: analystState.kpi("weekOverWeekChange").available
                                    ? analystState.formatPercent(analystState.kpi("weekOverWeekChange").value)
                                    : i18n("Unavailable")
                                font.pointSize: 20
                                font.weight: Font.Bold
                            }
                            Controls.Label {
                                Layout.fillWidth: true
                                visible: !analystState.kpi("weekOverWeekChange").available
                                text: analystState.reasonText(
                                    analystState.kpi("weekOverWeekChange").reasonKey,
                                    analystState.kpi("weekOverWeekChange").sampleCount,
                                    analystState.kpi("weekOverWeekChange").minimumSamples)
                                wrapMode: Text.WordWrap
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }
                    }

                    Kirigami.AbstractCard {
                        contentItem: ColumnLayout {
                            Controls.Label {
                                text: i18n("Volatility")
                                color: Kirigami.Theme.disabledTextColor
                            }
                            Controls.Label {
                                text: analystState.kpi("volatility").available
                                    ? analystState.formatPercent(analystState.kpi("volatility").value)
                                    : i18n("Unavailable")
                                font.pointSize: 20
                                font.weight: Font.Bold
                            }
                            Controls.Label {
                                Layout.fillWidth: true
                                visible: !analystState.kpi("volatility").available
                                text: analystState.reasonText(
                                    analystState.kpi("volatility").reasonKey,
                                    analystState.kpi("volatility").sampleCount,
                                    analystState.kpi("volatility").minimumSamples)
                                wrapMode: Text.WordWrap
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }
                    }
                }

                MultiSeriesChart {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 12
                    metric: "cost"
                    seriesData: analystState.spendChartSeries()
                }
            }
        }

        Kirigami.Card {
            Layout.fillWidth: true
            visible: analystState.hasSnapshot
                && analystState.snapshot.activityAvailable === true

            header: Kirigami.Heading {
                text: i18n("Activity trend")
                level: 3
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.mediumSpacing

                MultiSeriesChart {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 11
                    visible: analystState.seriesHasValues("tokens")
                    metric: "tokens"
                    seriesData: analystState.activityChartSeries("tokens")
                }

                MultiSeriesChart {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 11
                    visible: analystState.seriesHasValues("requests")
                    metric: "requests"
                    seriesData: analystState.activityChartSeries("requests")
                }

                ColumnLayout {
                    visible: analystState.seriesHasValues("toolUsage")
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading {
                        text: i18n("Local tool activity")
                        level: 4
                    }

                    MultiSeriesChart {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 11
                        metric: "requests"
                        seriesData: analystState.activityChartSeries("toolUsage")
                    }
                }

                OutputInputRatioCard {
                    Layout.fillWidth: true
                    visible: analystState.kpi("outputInputRatio").available
                    outputInputRatio: Number(
                        analystState.kpi("outputInputRatio").value)
                }

                Controls.Label {
                    visible: !analystState.kpi("outputInputRatio").available
                    text: analystState.reasonText(
                        analystState.kpi("outputInputRatio").reasonKey,
                        analystState.kpi("outputInputRatio").sampleCount,
                        analystState.kpi("outputInputRatio").minimumSamples)
                    wrapMode: Text.WordWrap
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }

        Kirigami.Card {
            Layout.fillWidth: true
            visible: analystState.hasSnapshot
                && (analystState.snapshot.topDrivers || []).length > 0

            header: Kirigami.Heading {
                text: i18n("Top compatible spend drivers")
                level: 3
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: analystState.snapshot.topDrivers || []

                    RowLayout {
                        id: driverRow
                        required property int index
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Controls.Label {
                            text: (driverRow.index + 1) + "."
                            color: Kirigami.Theme.disabledTextColor
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Controls.Label {
                                Layout.fillWidth: true
                                text: driverRow.modelData.provider + "  ["
                                      + driverRow.modelData.model + "]"
                                elide: Text.ElideRight
                            }
                            Controls.Label {
                                text: driverRow.modelData.quality === "estimated"
                                    ? i18n("Estimated")
                                    : i18n("Actual")
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }

                        Controls.Label {
                            text: analystState.formatMoney(
                                driverRow.modelData.value,
                                driverRow.modelData.currency)
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }
        }

        Kirigami.Card {
            Layout.fillWidth: true
            visible: analystState.hasSnapshot
                && analystState.snapshot.anomaliesAvailable === true

            header: Kirigami.Heading {
                text: i18n("Anomaly candidates")
                level: 3
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                Controls.Label {
                    visible: (analystState.snapshot.anomalies || []).length === 0
                    text: i18n("No day crossed the documented threshold.")
                    color: Kirigami.Theme.disabledTextColor
                }

                Repeater {
                    model: analystState.snapshot.anomalies || []

                    Controls.Label {
                        required property var modelData
                        Layout.fillWidth: true
                        text: i18n("%1: %2, compared with a %3 period baseline",
                                   modelData.date,
                                   analystState.formatMoney(modelData.value,
                                               modelData.currency),
                                   analystState.formatMoney(modelData.baseline,
                                               modelData.currency))
                        wrapMode: Text.WordWrap
                    }
                }

                Controls.Label {
                    text: i18n("Candidates require at least seven recorded days, two standard deviations above the period mean, and a material absolute increase.")
                    wrapMode: Text.WordWrap
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }

        Kirigami.Card {
            Layout.fillWidth: true
            visible: analystState.hasSnapshot

            header: Kirigami.Heading {
                text: i18n("Period summary")
                level: 3
            }

            contentItem: Controls.Label {
                objectName: "writtenSummary"
                text: analystState.writtenSummary()
                wrapMode: Text.WordWrap
            }
        }
    }
}
