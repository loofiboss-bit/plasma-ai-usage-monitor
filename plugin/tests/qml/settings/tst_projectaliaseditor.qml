import QtQuick
import QtTest
import "../../../../package/contents/ui" as AppUi

TestCase {
    id: testCase
    name: "ProjectAliasEditor"
    when: windowShown
    visible: true
    width: 640
    height: 700

    QtObject {
        id: backend
        property var rows: []
        property bool failSave: false
        property int writes: 0
        function projectAliases() { return rows; }
        function createProjectAlias(directory, name) {
            writes++;
            if (failSave) return "";
            rows = rows.concat([{projectId: "project-id", alias: name, sessionCount: 0}]);
            return "project-id";
        }
        function setProjectAlias(projectId, name) {
            writes++;
            if (failSave) return false;
            rows = name ? [{projectId: projectId, alias: name, sessionCount: 2}] : [];
            return true;
        }
    }

    property alias aliasBackend: backend
    Component {
        id: editorComponent
        AppUi.ProjectAliasEditor {
            width: 500
            backend: testCase.aliasBackend
        }
    }

    function init() {
        backend.rows = [];
        backend.failSave = false;
        backend.writes = 0;
    }

    function test_emptyListAndExplicitCreation() {
        var editor = createTemporaryObject(editorComponent, testCase);
        verify(editor !== null);
        compare(editor.aliases.length, 0);
        verify(findChild(editor, "projectAliasesEmptyState").visible);
        var add = findChild(editor, "createProjectAliasButton");
        mouseClick(add);
        var dialog = findChild(editor, "projectAliasDialog");
        var name = findChild(editor, "projectAliasNameField");
        var save = findChild(editor, "saveProjectAliasButton");
        tryCompare(dialog, "opened", true);
        verify(!save.enabled);
        name.text = "My app";
        verify(!save.enabled);
        dialog.selectedDirectory = "file:///tmp/my-app";
        verify(save.enabled);
        mouseClick(save);
        tryCompare(dialog, "opened", false);
        compare(backend.writes, 1);
        compare(editor.aliases.length, 1);
        compare(editor.aliases[0].alias, "My app");
        verify(!findChild(editor, "projectAliasesEmptyState").visible);
    }

    function test_cancelDoesNotCreateAndFailureKeepsInput() {
        var editor = createTemporaryObject(editorComponent, testCase);
        editor.openCreate();
        var dialog = findChild(editor, "projectAliasDialog");
        var name = findChild(editor, "projectAliasNameField");
        name.text = "Unsaved";
        dialog.selectedDirectory = "file:///tmp/unsaved";
        dialog.close();
        compare(backend.writes, 0);
        compare(editor.aliases.length, 0);
        editor.openCreate();
        name.text = "Retry name";
        dialog.selectedDirectory = "file:///tmp/retry";
        backend.failSave = true;
        editor.saveProject();
        tryCompare(dialog, "opened", true);
        compare(name.text, "Retry name");
        verify(dialog.errorText.length > 0);
        compare(editor.aliases.length, 0);
        backend.failSave = false;
        editor.saveProject();
        compare(editor.aliases[0].alias, "Retry name");
    }

    function test_renameDoesNotRequireChoosingFolderAgain() {
        backend.rows = [{projectId: "known-id", alias: "Old name", sessionCount: 2}];
        var editor = createTemporaryObject(editorComponent, testCase);
        editor.openRename(backend.rows[0]);
        var dialog = findChild(editor, "projectAliasDialog");
        var name = findChild(editor, "projectAliasNameField");
        compare(name.text, "Old name");
        verify(!findChild(editor, "projectAliasFolderButton").visible);
        name.text = "New name";
        editor.saveProject();
        compare(editor.aliases[0].projectId, "known-id");
        compare(editor.aliases[0].alias, "New name");
        compare(editor.aliases[0].sessionCount, 2);
    }

    function test_removeRequiresConfirmationAndPreservesListOnFailure() {
        backend.rows = [{projectId: "known-id", alias: "My project", sessionCount: 2}];
        var editor = createTemporaryObject(editorComponent, testCase);
        var remove = findChild(editor, "removeProjectAliasButton");
        var dialog = findChild(editor, "removeProjectAliasDialog");
        mouseClick(remove);
        tryCompare(dialog, "opened", true);
        compare(backend.writes, 0);
        dialog.reject();
        compare(editor.aliases.length, 1);
        mouseClick(remove);
        backend.failSave = true;
        dialog.accept();
        compare(editor.aliases.length, 1);
        verify(editor.errorText.length > 0);
        backend.failSave = false;
        mouseClick(remove);
        dialog.accept();
        compare(editor.aliases.length, 0);
        compare(editor.errorText, "");
    }
}
