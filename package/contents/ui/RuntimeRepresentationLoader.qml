import QtQuick

Loader {
    required property bool runtimeReady
    required property Component bootstrapRepresentation
    property Component runtimeRepresentation: null

    sourceComponent: runtimeReady && runtimeRepresentation
        ? runtimeRepresentation
        : bootstrapRepresentation
}
