import QtQuick
import QtQuick.Layouts

Loader {
    required property bool runtimeReady
    required property Component bootstrapRepresentation
    property Component runtimeRepresentation: null

    sourceComponent: runtimeReady && runtimeRepresentation
        ? runtimeRepresentation
        : bootstrapRepresentation

    Layout.fillWidth: item ? item.Layout.fillWidth : false
    Layout.fillHeight: item ? item.Layout.fillHeight : false
    Layout.minimumWidth: item ? item.Layout.minimumWidth : -1
    Layout.minimumHeight: item ? item.Layout.minimumHeight : -1
    Layout.preferredWidth: item ? item.Layout.preferredWidth : -1
    Layout.preferredHeight: item ? item.Layout.preferredHeight : -1
    Layout.maximumWidth: item ? item.Layout.maximumWidth : Number.POSITIVE_INFINITY
    Layout.maximumHeight: item ? item.Layout.maximumHeight : Number.POSITIVE_INFINITY
}
