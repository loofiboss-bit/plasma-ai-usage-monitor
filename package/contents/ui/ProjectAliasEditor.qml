pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import QtQuick.Dialogs as Dialogs
import org.kde.kirigami as Kirigami

Item {
    id: editor

    required property var backend
    property var aliases: []
    property string errorText: ""
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    implicitHeight: content.implicitHeight

    function reload() {
        if (typeof backend.projectAliases !== "function") {
            errorText = i18n("Install the matching native plugin to manage project aliases.");
            return;
        }
        aliases = backend.projectAliases() || [];
    }

    function openCreate() {
        projectDialog.projectId = "";
        projectDialog.selectedDirectory = "";
        projectName.text = "";
        projectDialog.errorText = "";
        projectDialog.open();
        projectName.forceActiveFocus();
    }

    function openRename(row) {
        projectDialog.projectId = row.projectId;
        projectDialog.selectedDirectory = "";
        projectName.text = row.alias;
        projectDialog.errorText = "";
        projectDialog.open();
        projectName.forceActiveFocus();
        projectName.selectAll();
    }

    function saveProject() {
        var name = projectName.text.trim();
        if (!name || (!projectDialog.projectId && !projectDialog.selectedDirectory.toString())) return;
        var saved = projectDialog.projectId
            ? backend.setProjectAlias(projectDialog.projectId, name)
            : typeof backend.createProjectAlias === "function"
                && !!backend.createProjectAlias(projectDialog.selectedDirectory, name);
        if (!saved) {
            projectDialog.errorText = i18n("The project alias could not be saved. Check the folder and try again.");
            return;
        }
        projectDialog.close();
        reload();
    }

    Component.onCompleted: reload()

    ColumnLayout {
        id: content
        width: parent.width
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true
            Kirigami.Heading {
                Layout.fillWidth: true
                text: i18n("Project aliases")
                level: 3
            }
            QQC2.Button {
                objectName: "createProjectAliasButton"
                text: i18n("Create project")
                icon.name: "list-add"
                enabled: typeof editor.backend.createProjectAlias === "function"
                onClicked: editor.openCreate()
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            text: i18n("Choose a project folder and give it a name. Only aliases you create appear here; matching sessions use that name. Folder paths are not saved.")
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: editor.errorText.length > 0
            text: editor.errorText
            type: Kirigami.MessageType.Error
        }

        QQC2.Label {
            objectName: "projectAliasesEmptyState"
            Layout.fillWidth: true
            visible: editor.aliases.length === 0
            text: i18n("No project aliases yet. Create one to name a project in your session history.")
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: editor.aliases
            delegate: RowLayout {
                id: aliasRow
                required property var modelData
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 0
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: aliasRow.modelData.alias
                        elide: Text.ElideRight
                        QQC2.ToolTip.visible: aliasHover.hovered && truncated
                        QQC2.ToolTip.text: text
                        HoverHandler { id: aliasHover }
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: aliasRow.modelData.sessionCount > 0
                            ? i18np("%1 session", "%1 sessions", aliasRow.modelData.sessionCount)
                            : i18n("No sessions yet")
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
                QQC2.ToolButton {
                    icon.name: "document-edit"
                    Accessible.name: i18n("Rename %1", aliasRow.modelData.alias)
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: Accessible.name
                    onClicked: editor.openRename(aliasRow.modelData)
                }
                QQC2.ToolButton {
                    objectName: "removeProjectAliasButton"
                    icon.name: "edit-delete"
                    Accessible.name: i18n("Remove alias %1", aliasRow.modelData.alias)
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: Accessible.name
                    onClicked: {
                        removeDialog.projectId = aliasRow.modelData.projectId;
                        removeDialog.aliasName = aliasRow.modelData.alias;
                        removeDialog.open();
                    }
                }
            }
        }

    }

    QQC2.Dialog {
        id: projectDialog
        objectName: "projectAliasDialog"
        parent: QQC2.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(Kirigami.Units.gridUnit * 32, parent ? parent.width - Kirigami.Units.largeSpacing * 2 : Kirigami.Units.gridUnit * 32)
        modal: true
        title: projectId ? i18n("Rename project") : i18n("Create project alias")
        property string projectId: ""
        property url selectedDirectory: ""
        property string errorText: ""

        contentItem: ColumnLayout {
            QQC2.Label { text: i18n("Project name:") }
            QQC2.TextField {
                id: projectName
                objectName: "projectAliasNameField"
                Layout.fillWidth: true
                placeholderText: i18n("Enter a name")
                maximumLength: 80
                Accessible.name: i18n("Project name")
                onAccepted: if (saveButton.enabled) editor.saveProject()
            }
            QQC2.Label {
                visible: !projectDialog.projectId
                text: i18n("Project folder:")
            }
            QQC2.Button {
                objectName: "projectAliasFolderButton"
                Layout.fillWidth: true
                visible: !projectDialog.projectId
                text: projectDialog.selectedDirectory.toString()
                    ? i18n("Change folder") : i18n("Choose folder…")
                icon.name: "folder-open"
                onClicked: folderDialog.open()
            }
            QQC2.Label {
                Layout.fillWidth: true
                visible: !projectDialog.projectId && !!projectDialog.selectedDirectory.toString()
                text: decodeURIComponent(projectDialog.selectedDirectory.toString().replace(/^file:\/\//, ""))
                wrapMode: Text.WrapAnywhere
            }
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: projectDialog.errorText.length > 0
                text: projectDialog.errorText
                type: Kirigami.MessageType.Error
            }
        }
        footer: RowLayout {
            Item { Layout.fillWidth: true }
            QQC2.Button {
                text: i18n("Cancel")
                onClicked: projectDialog.close()
            }
            QQC2.Button {
                id: saveButton
                objectName: "saveProjectAliasButton"
                text: projectDialog.projectId ? i18n("Save name") : i18n("Create project")
                enabled: projectName.text.trim().length > 0
                    && (!!projectDialog.projectId || !!projectDialog.selectedDirectory.toString())
                onClicked: editor.saveProject()
            }
        }
        onClosed: {
            selectedDirectory = "";
            projectName.text = "";
        }
    }

    Dialogs.FolderDialog {
        id: folderDialog
        title: i18n("Choose the folder used by your coding tool")
        onAccepted: projectDialog.selectedDirectory = selectedFolder
    }

    QQC2.Dialog {
        id: removeDialog
        objectName: "removeProjectAliasDialog"
        parent: QQC2.Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(Kirigami.Units.gridUnit * 28, parent ? parent.width - Kirigami.Units.largeSpacing * 2 : Kirigami.Units.gridUnit * 28)
        modal: true
        title: i18n("Remove project alias")
        property string projectId: ""
        property string aliasName: ""
        standardButtons: QQC2.Dialog.Ok | QQC2.Dialog.Cancel
        contentItem: QQC2.Label {
            text: i18n("Remove the name “%1”? Session history will be kept.", removeDialog.aliasName)
            wrapMode: Text.WordWrap
        }
        onAccepted: {
            if (!editor.backend.setProjectAlias(projectId, ""))
                editor.errorText = i18n("The project alias could not be removed. Try again.");
            else {
                editor.errorText = "";
                editor.reload();
            }
        }
    }
}
