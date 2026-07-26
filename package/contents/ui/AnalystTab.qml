pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.plasmoid
import org.kde.ki18n
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0
import "components" as Components

Kirigami.ScrollablePage {
    id: analystPage
    required property var monitor

    title: KI18n.i18n("The Analyst")
    Accessible.name: analystState.loading
        ? KI18n.i18n("Analyst view loading")
        : KI18n.i18n("Analyst view ready")

    readonly property var db: Plasmoid.configuration.historyEnabled
        ? analystPage.monitor.usageDb
        : null
    Components.AnalystState {
        id: analystState
        db: analystPage.db
        onReportReady: function(report) {
            clipboard.setText(report);
        }
    }

    Component.onCompleted: analystState.refreshData()
    onVisibleChanged: if (visible && analystState.reportRequestId === "")
        analystState.refreshData()

    actions: [
        Kirigami.Action {
            icon.name: "view-refresh"
            text: KI18n.i18n("Refresh")
            enabled: analystState.reportRequestId === ""
            onTriggered: analystState.refreshData()
        },
        Kirigami.Action {
            icon.name: "edit-copy"
            text: KI18n.i18n("Copy 7-day report")
            enabled: analystPage.db !== null
                && !analystState.loading
                && analystState.reportRequestId === ""
            onTriggered: analystState.requestReport(7)
        },
        Kirigami.Action {
            icon.name: "edit-copy"
            text: KI18n.i18n("Copy 30-day report")
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

        Kirigami.Card {
            Layout.fillWidth: true
            visible: analystState.hasSnapshot

            header: Kirigami.Heading {
                text: KI18n.i18n("Data coverage")
                level: 3
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                Controls.Label {
                    text: KI18n.i18n("Period: %1 – %2",
                               analystState.formatDate(analystState.snapshot.from),
                               analystState.formatDate(new Date(
                                   new Date(analystState.snapshot.to).getTime()
                                   - 1)))
                    color: Kirigami.Theme.disabledTextColor
                }

                Controls.Label {
                    objectName: "coverageLabel"
                    text: KI18n.i18n("%1 of %2 days contain compatible observations (%3%)",
                               analystState.coverage.observedDayCount || 0,
                               analystState.coverage.requestedDayCount || 30,
                               Number(analystState.coverage.percent || 0).toFixed(0))
                    wrapMode: Text.WordWrap
                }

                Controls.Label {
                    text: KI18n.i18n("%1 actual and %2 estimated compatible cost samples",
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
                text: KI18n.i18n("Spend trend")
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
                                text: KI18n.i18n("Week over week")
                                color: Kirigami.Theme.disabledTextColor
                            }
                            Controls.Label {
                                text: analystState.kpi("weekOverWeekChange").available
                                    ? analystState.formatPercent(analystState.kpi("weekOverWeekChange").value)
                                    : KI18n.i18n("Unavailable")
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
                                text: KI18n.i18n("Volatility")
                                color: Kirigami.Theme.disabledTextColor
                            }
                            Controls.Label {
                                text: analystState.kpi("volatility").available
                                    ? analystState.formatPercent(analystState.kpi("volatility").value)
                                    : KI18n.i18n("Unavailable")
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
                text: KI18n.i18n("Activity trend")
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
                        text: KI18n.i18n("Local tool activity")
                        level: 4
                    }

                    MultiSeriesChart {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 11
                        metric: "requests"
                        seriesData: analystState.activityChartSeries("toolUsage")
                    }
                }

                EfficiencyMetricCard {
                    Layout.fillWidth: true
                    visible: analystState.kpi("outputInputRatio").available
                    efficiencyRatio: Number(
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
                text: KI18n.i18n("Top compatible spend drivers")
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
                                    ? KI18n.i18n("Estimated")
                                    : KI18n.i18n("Actual")
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
                text: KI18n.i18n("Anomaly candidates")
                level: 3
            }

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                Controls.Label {
                    visible: (analystState.snapshot.anomalies || []).length === 0
                    text: KI18n.i18n("No day crossed the documented threshold.")
                    color: Kirigami.Theme.disabledTextColor
                }

                Repeater {
                    model: analystState.snapshot.anomalies || []

                    Controls.Label {
                        required property var modelData
                        Layout.fillWidth: true
                        text: KI18n.i18n("%1: %2, compared with a %3 period baseline",
                                   modelData.date,
                                   analystState.formatMoney(modelData.value,
                                               modelData.currency),
                                   analystState.formatMoney(modelData.baseline,
                                               modelData.currency))
                        wrapMode: Text.WordWrap
                    }
                }

                Controls.Label {
                    text: KI18n.i18n("Candidates require at least seven recorded days, two standard deviations above the period mean, and a material absolute increase.")
                    wrapMode: Text.WordWrap
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }

        Kirigami.Card {
            Layout.fillWidth: true
            visible: analystState.hasSnapshot

            header: Kirigami.Heading {
                text: KI18n.i18n("Period summary")
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
