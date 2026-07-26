pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import com.github.loofi.aiusagemonitor 1.0

ColumnLayout {
    id: panel

    spacing: Kirigami.Units.smallSpacing

    Repeater {
        model: [
            {
                label: i18n("Providers"),
                schemaVersion: ProviderPricingCatalog.schemaVersion,
                catalogVersion: ProviderPricingCatalog.catalogVersion,
                lastReviewed: ProviderPricingCatalog.lastReviewed,
                runtimeScraping: ProviderPricingCatalog.runtimeScraping,
                valid: ProviderPricingCatalog.valid,
                stale: ProviderPricingCatalog.stale,
                manualReviewCount: ProviderPricingCatalog.manualReviewCount,
                sourceConflictCount: ProviderPricingCatalog.sourceConflictCount,
                reviewItems: ProviderPricingCatalog.reviewItems
            },
            {
                label: i18n("Subscriptions"),
                schemaVersion: SubscriptionPlanCatalog.schemaVersion,
                catalogVersion: SubscriptionPlanCatalog.catalogVersion,
                lastReviewed: SubscriptionPlanCatalog.lastReviewed,
                runtimeScraping: SubscriptionPlanCatalog.runtimeScraping,
                valid: SubscriptionPlanCatalog.valid,
                stale: SubscriptionPlanCatalog.stale,
                manualReviewCount: SubscriptionPlanCatalog.manualReviewCount,
                sourceConflictCount: SubscriptionPlanCatalog.sourceConflictCount,
                reviewItems: SubscriptionPlanCatalog.reviewItems
            }
        ]

        delegate: ColumnLayout {
            id: catalogRow
            required property var modelData
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: catalogRow.modelData.valid
                            && !catalogRow.modelData.stale
                            && !catalogRow.modelData.runtimeScraping
                        ? "dialog-ok" : "dialog-warning"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    color: catalogRow.modelData.valid
                           && !catalogRow.modelData.stale
                           && !catalogRow.modelData.runtimeScraping
                        ? Kirigami.Theme.positiveTextColor
                        : Kirigami.Theme.neutralTextColor
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: i18n("%1: schema %2, catalog %3, reviewed %4, runtime scraping %5, review items %6, conflicts %7",
                               catalogRow.modelData.label,
                               catalogRow.modelData.schemaVersion,
                               catalogRow.modelData.catalogVersion,
                               catalogRow.modelData.lastReviewed,
                               catalogRow.modelData.runtimeScraping ? i18n("enabled") : i18n("disabled"),
                               catalogRow.modelData.manualReviewCount,
                               catalogRow.modelData.sourceConflictCount)
                    wrapMode: Text.WordWrap
                }
            }

            Repeater {
                model: catalogRow.modelData.reviewItems

                delegate: RowLayout {
                    id: reviewRow
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: reviewRow.modelData.sourceConflict
                            ? "dialog-error" : "dialog-warning"
                        Layout.preferredWidth: Kirigami.Units.iconSizes.small
                        Layout.preferredHeight: Kirigami.Units.iconSizes.small
                        color: reviewRow.modelData.sourceConflict
                            ? Kirigami.Theme.negativeTextColor
                            : Kirigami.Theme.neutralTextColor
                    }

                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: {
                            var reason = reviewRow.modelData.sourceConflict
                                ? (reviewRow.modelData.sourceConflictReason
                                   || reviewRow.modelData.reviewReason)
                                : reviewRow.modelData.reviewReason;
                            if (!reason || reason.length === 0) {
                                reason = reviewRow.modelData.source
                                    || i18n("Needs maintainer review before it can be shown as exact.");
                            }
                            return i18n(
                                "%1: %2", reviewRow.modelData.label, reason);
                        }
                        wrapMode: Text.WordWrap
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                    }
                }
            }
        }
    }
}
