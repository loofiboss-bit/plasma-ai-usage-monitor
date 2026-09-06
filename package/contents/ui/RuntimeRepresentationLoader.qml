import QtQuick
import QtQuick.Layouts

Loader {
    required property bool runtimeReady
    required property Component bootstrapRepresentation
    property Component runtimeRepresentation: null
    readonly property Item representationItem: item as Item

    sourceComponent: runtimeReady && runtimeRepresentation
        ? runtimeRepresentation
        : bootstrapRepresentation

    Layout.fillWidth: representationItem ? representationItem.Layout.fillWidth : false
    Layout.fillHeight: representationItem ? representationItem.Layout.fillHeight : false
    Layout.minimumWidth: representationItem ? representationItem.Layout.minimumWidth : -1
    Layout.minimumHeight: representationItem ? representationItem.Layout.minimumHeight : -1
    Layout.preferredWidth: representationItem ? representationItem.Layout.preferredWidth : -1
    Layout.preferredHeight: representationItem ? representationItem.Layout.preferredHeight : -1
    Layout.maximumWidth: representationItem ? representationItem.Layout.maximumWidth : Number.POSITIVE_INFINITY
    Layout.maximumHeight: representationItem ? representationItem.Layout.maximumHeight : Number.POSITIVE_INFINITY
}
