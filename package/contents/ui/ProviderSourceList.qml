pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: sourceList

    required property var controller

    spacing: Kirigami.Units.smallSpacing

    Kirigami.SearchField {
        Layout.fillWidth: true
        placeholderText: i18n("Search sources")
        Accessible.name: i18n("Search provider sources")
        onTextChanged: sourceList.controller.searchText = text
    }

    QQC2.ComboBox {
        Layout.fillWidth: true
        textRole: "text"
        valueRole: "value"
        Accessible.name: i18n("Filter by monitoring level")
        model: [
            { text: i18n("All monitoring levels"), value: "all" },
            { text: i18n("Usage & spend"), value: "usage" },
            { text: i18n("Detected local tools"), value: "local" },
            { text: i18n("Gateway"), value: "gateway" },
            { text: i18n("Balance"), value: "balance" },
            { text: i18n("Connectivity only"), value: "connectivity" }
        ]
        onActivated: sourceList.controller.filterKey = currentValue
    }

    ListView {
        id: providersList
        Accessible.name: i18n("Provider and detected local sources")
        activeFocusOnTab: true
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: sourceList.controller.visibleSources
        currentIndex: {
            for (var i = 0; i < model.length; ++i) {
                if (model[i].configKey === sourceList.controller.selectedSourceId) return i;
            }
            return -1;
        }
        section.property: "categoryLabel"
        section.delegate: Kirigami.ListSectionHeader { required property string section; text: section }

        Keys.onUpPressed: function(event) {
            if (currentIndex > 0) {
                sourceList.controller.selectedSourceId = model[currentIndex - 1].configKey;
            }
            event.accepted = true;
        }

        Keys.onDownPressed: function(event) {
            if (currentIndex >= 0 && currentIndex + 1 < model.length) {
                sourceList.controller.selectedSourceId = model[currentIndex + 1].configKey;
            }
            event.accepted = true;
        }

        delegate: QQC2.ItemDelegate {
            id: sourceDelegate
            required property var modelData
            required property int index
            width: ListView.view.width
            highlighted: modelData.configKey === sourceList.controller.selectedSourceId
            Accessible.name: i18n("Configure %1, %2", modelData.name, modelData.categoryLabel)
            contentItem: RowLayout {
                QQC2.Label {
                    Layout.fillWidth: true
                    text: sourceDelegate.modelData.name
                    elide: Text.ElideRight
                }
                Kirigami.Icon {
                    visible: !!sourceList.controller.value(sourceDelegate.modelData.enabledConfigKey)
                    source: "emblem-checked"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: width
                    Accessible.name: i18n("Enabled")
                }
            }
            onClicked: sourceList.controller.selectedSourceId = modelData.configKey
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width
            visible: providersList.count === 0
            text: i18n("No sources match the current search and filter.")
        }
    }
}
