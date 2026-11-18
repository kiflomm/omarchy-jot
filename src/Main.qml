import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1060
    height: 720
    minimumWidth: 720
    minimumHeight: 520
    visible: true
    title: qsTr("Jot — Notes & to-dos")
    color: "#111214"
    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius
    font.family: "sans-serif"
    font.pixelSize: 14

    property int page: 0
    property bool textEditing: activeFocusItem instanceof TextInput || activeFocusItem instanceof TextEdit
    property bool allowClose: false
    property string pendingId: ""
    property string pendingKind: ""

    function newNote() {
        if (backend.addNote() !== "") {
            page = 0
            search.clear()
            noteTitle.forceActiveFocus()
        }
    }
    function newTask() { page = 1; taskInput.forceActiveFocus() }
    function requestDelete(kind, id) { pendingKind = kind; pendingId = id; deleteDialog.open() }
    onClosing: function(close) {
        if (allowClose) return
        var saved = backend.flush()
        if (!saved || taskInput.text.trim().length > 0) {
            close.accepted = false
            quitDialog.open()
        }
    }

    Shortcut { sequence: "Ctrl+N"; context: Qt.ApplicationShortcut; onActivated: win.newNote() }
    Shortcut { sequence: "Ctrl+L"; context: Qt.ApplicationShortcut; onActivated: win.newTask() }
    Shortcut { sequence: "Ctrl+1"; context: Qt.ApplicationShortcut; onActivated: win.page = 0 }
    Shortcut { sequence: "Ctrl+2"; context: Qt.ApplicationShortcut; onActivated: win.page = 1 }
    Shortcut { sequence: "Ctrl+F"; context: Qt.ApplicationShortcut; onActivated: { search.forceActiveFocus(); search.selectAll() } }
    Shortcut { sequence: "Ctrl+S"; context: Qt.ApplicationShortcut; onActivated: backend.flush() }
    Shortcut { sequence: "?"; enabled: !win.textEditing; context: Qt.ApplicationShortcut; onActivated: helpDialog.open() }
    Shortcut { sequence: "F1"; context: Qt.ApplicationShortcut; onActivated: helpDialog.open() }
    Shortcut { sequence: "Q"; enabled: !win.textEditing; context: Qt.ApplicationShortcut; onActivated: win.close() }
    Shortcut { sequence: "Ctrl+Q"; context: Qt.ApplicationShortcut; onActivated: win.close() }
    Shortcut { sequence: "Escape"; enabled: !helpDialog.visible && !deleteDialog.visible && !quitDialog.visible && !editDialog.visible; context: Qt.ApplicationShortcut; onActivated: workspace.forceActiveFocus() }
    Shortcut { sequence: "Space"; enabled: !win.textEditing; context: Qt.ApplicationShortcut; onActivated: win.page === 0 ? win.newNote() : win.newTask() }
    Shortcut { sequences: [StandardKey.Undo]; enabled: !win.textEditing; context: Qt.ApplicationShortcut; onActivated: backend.undo() }
    Shortcut { sequences: [StandardKey.Redo]; enabled: !win.textEditing; context: Qt.ApplicationShortcut; onActivated: backend.redo() }

    component JotButton: Button {
        id: control
        property bool primary: false
        property bool selected: false
        leftPadding: 16
        rightPadding: 16
        implicitHeight: 40
        topInset: 0
        bottomInset: 0
        leftInset: 0
        rightInset: 0
        font.pixelSize: 13
        contentItem: Text {
            text: control.text
            font: control.font
            color: control.primary ? theme.accentForeground : control.selected ? theme.accent : "#e6e6e8"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            opacity: control.enabled ? 1 : 0.35
        }
        background: Rectangle {
            radius: theme.radius
            color: control.primary ? theme.accent : control.selected ? Qt.alpha(theme.accent, 0.10) : control.hovered || control.visualFocus ? "#303237" : "#23252a"
            opacity: control.enabled ? 1 : 0.45
            border.width: control.visualFocus ? 1 : 0
            border.color: theme.accent
        }
    }

    component JotField: Basic.TextField {
        id: field
        implicitHeight: Math.max(48, font.pixelSize + 26)
        padding: 14
        color: "#e9e9ed"
        placeholderTextColor: "#999da6"
        selectionColor: theme.accent
        selectedTextColor: theme.accentForeground
        background: Rectangle {
            color: "#17181b"
            radius: theme.radius
            border.color: field.activeFocus ? theme.accent : "#3a3d44"
            border.width: field.activeFocus ? 2 : 1
        }
    }

    Item {
        id: workspace
        anchors.fill: parent
        focus: true
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 82
                color: "#17181b"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 26
                    anchors.rightMargin: 26
                    spacing: 10
                    Rectangle {
                        width: 32; height: 32; radius: Math.min(4, theme.radius); color: theme.accent
                        Text { anchors.centerIn: parent; text: "j"; font.pixelSize: 27; font.bold: true; color: theme.accentForeground }
                    }
                    Label { text: qsTr("Jot"); font.pixelSize: 23; font.bold: true; color: "#f4f4f5"; Layout.rightMargin: 22 }
                    JotButton { objectName: "notesTab"; text: qsTr("Notes"); selected: win.page === 0; onClicked: win.page = 0 }
                    JotButton { objectName: "tasksTab"; text: qsTr("To-do"); selected: win.page === 1; onClicked: win.page = 1 }
                    Item { Layout.fillWidth: true }
                    JotButton { text: qsTr("Undo"); enabled: backend.canUndo || backend.dirty; onClicked: backend.undo(); ToolTip.visible: hovered; ToolTip.text: qsTr("Undo (Ctrl+Z)") }
                    JotButton { text: qsTr("Redo"); enabled: backend.canRedo; onClicked: backend.redo(); ToolTip.visible: hovered; ToolTip.text: qsTr("Redo (Ctrl+Shift+Z)") }
                    JotButton { text: "?"; Accessible.name: qsTr("Keyboard shortcuts"); onClicked: helpDialog.open() }
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#2a2b30" }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: errorRow.implicitHeight + 24
                visible: backend.error.length > 0
                color: "#3b2425"
                RowLayout {
                    id: errorRow
                    anchors.fill: parent
                    anchors.margins: 12
                    Label { text: backend.error; textFormat: Text.PlainText; color: "#ffc5c5"; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    JotButton { text: qsTr("Retry save"); onClicked: backend.dirty ? backend.flush() : backend.refresh() }
                    JotButton { text: "×"; Accessible.name: qsTr("Dismiss error"); onClicked: backend.dismissError() }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 20
                spacing: 16
                JotField {
                    id: search
                    objectName: "search"
                    Layout.fillWidth: true
                    placeholderText: win.page === 0 ? qsTr("Search notes…") : qsTr("Search tasks…")
                    selectByMouse: true
                    onTextChanged: backend.query = text
                    Accessible.name: placeholderText
                }
                JotButton {
                    text: win.page === 0 ? qsTr("+ New note") : qsTr("+ New task")
                    primary: true
                    onClicked: win.page === 0 ? win.newNote() : win.newTask()
                }
            }
            RowLayout {
                visible: win.page === 0
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0
                Rectangle {
                    Layout.preferredWidth: Math.max(220, Math.min(285, win.width * 0.28))
                    Layout.fillHeight: true
                    color: "#17181b"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12
                        Label { text: qsTr("YOUR NOTES"); font.pixelSize: 11; font.letterSpacing: 1.4; color: "#999da6" }
                        ListView {
                            id: noteList
                            objectName: "noteList"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 6
                            model: backend.notes
                            ScrollBar.vertical: ScrollBar {}
                            delegate: ItemDelegate {
                                id: noteDelegate
                                required property var modelData
                                width: ListView.view.width
                                height: 94
                                padding: 14
                                onClicked: backend.selectNote(modelData.id)
                                Accessible.name: modelData.title || qsTr("Untitled note")
                                background: Rectangle {
                                    radius: theme.radius
                                    color: backend.selectedId === noteDelegate.modelData.id ? Qt.alpha(theme.accent, 0.10) : noteDelegate.hovered ? "#25272c" : "transparent"
                                    border.width: backend.selectedId === noteDelegate.modelData.id || noteDelegate.visualFocus ? 1 : 0
                                    border.color: Qt.alpha(theme.accent, 0.4)
                                }
                                contentItem: Column {
                                    spacing: 7
                                    Label { width: parent.width; text: noteDelegate.modelData.title || qsTr("Untitled note"); textFormat: Text.PlainText; color: "#e9e9ed"; font.bold: true; elide: Text.ElideRight }
                                    Label { width: parent.width; text: noteDelegate.modelData.preview || qsTr("No text yet"); textFormat: Text.PlainText; color: "#a1a4ad"; font.pixelSize: 12; maximumLineCount: 2; wrapMode: Text.Wrap; elide: Text.ElideRight }
                                }
                            }
                        }
                        Label {
                            visible: noteList.count === 0
                            text: search.text.length ? qsTr("No matching notes") : qsTr("Your notes will appear here.")
                            color: "#a1a4ad"; wrapMode: Text.Wrap; Layout.fillWidth: true
                        }
                    }
                }
                Rectangle { width: 1; Layout.fillHeight: true; color: "#2a2b30" }
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.min(340, parent.width - 64)
                        spacing: 16
                        visible: backend.selectedId.length === 0
                        Label { text: qsTr("A little room for your thoughts."); font.pixelSize: 24; font.bold: true; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        Label { text: qsTr("Capture an idea, make a plan, or write something down."); color: "#a1a4ad"; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        JotButton { text: qsTr("Create your first note"); primary: true; onClicked: win.newNote() }
                    }
                    ColumnLayout {
                        visible: backend.selectedId.length > 0
                        anchors.fill: parent
                        anchors.margins: 30
                        spacing: 12
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: backend.dirty ? qsTr("Saving…") : qsTr("Saved on this device"); color: backend.dirty ? theme.accent : "#a1a4ad"; font.pixelSize: 12; Layout.fillWidth: true }
                            JotButton { objectName: "deleteNote"; text: qsTr("Delete note"); onClicked: win.requestDelete("note", backend.selectedId) }
                        }
                        JotField {
                            id: noteTitle
                            objectName: "noteTitle"
                            Layout.fillWidth: true
                            font.pixelSize: 27
                            font.bold: true
                            background: null
                            leftPadding: 0
                            rightPadding: 0
                            placeholderText: qsTr("Untitled note")
                            text: backend.noteTitle
                            selectByMouse: true
                            onTextEdited: backend.editTitle(text)
                            Accessible.name: qsTr("Note title")
                            Keys.onReturnPressed: noteBody.forceActiveFocus()
                        }
                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            TextArea {
                                id: noteBody
                                objectName: "noteBody"
                                text: backend.noteBody
                                placeholderText: qsTr("Start writing…")
                                wrapMode: TextEdit.Wrap
                                selectByMouse: true
                                font.pixelSize: 16
                                color: "#dedee3"
                                background: null
                                padding: 4
                                onTextChanged: if (text !== backend.noteBody) backend.editBody(text)
                                Accessible.name: qsTr("Note text")
                            }
                        }
                    }
                }
            }
            Item {
                visible: win.page === 1
                Layout.fillWidth: true
                Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Math.max(28, (parent.width - 820) / 2)
                    anchors.rightMargin: anchors.leftMargin
                    anchors.topMargin: 12
                    anchors.bottomMargin: 24
                    spacing: 20
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("One thing at a time."); font.pixelSize: 27; font.bold: true; Layout.fillWidth: true }
                        Label {
                            text: { translator.language; return qsTr("%n open tasks", "", backend.openCount) }
                            color: "#a1a4ad"
                        }
                    }
                    RowLayout {
                        spacing: 12
                        JotField {
                            id: taskInput
                            objectName: "taskInput"
                            Layout.fillWidth: true
                            placeholderText: qsTr("What needs to get done?")
                            selectByMouse: true
                            Accessible.name: placeholderText
                            onAccepted: addTaskButton.clicked()
                        }
                        JotButton {
                            id: addTaskButton
                            objectName: "addTask"
                            text: qsTr("Add task")
                            primary: true
                            enabled: taskInput.text.trim().length > 0
                            onClicked: {
                                if (backend.addTask(taskInput.text) !== "") {
                                    taskInput.clear()
                                    search.clear()
                                    backend.taskFilter = "open"
                                    taskInput.forceActiveFocus()
                                }
                            }
                        }
                    }
                    RowLayout {
                        spacing: 8
                        JotButton { text: qsTr("Open"); selected: backend.taskFilter === "open"; onClicked: backend.taskFilter = "open" }
                        JotButton { text: qsTr("Done"); selected: backend.taskFilter === "done"; onClicked: backend.taskFilter = "done" }
                        JotButton { objectName: "allTasks"; text: qsTr("All"); selected: backend.taskFilter === "all"; onClicked: backend.taskFilter = "all" }
                    }
                    ListView {
                        id: taskList
                        objectName: "taskList"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 8
                        model: backend.tasks
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            id: taskDelegate
                            required property var modelData
                            width: ListView.view.width
                            height: Math.max(68, taskRow.implicitHeight + 20)
                            radius: theme.radius
                            color: "#1d1f23"
                            RowLayout {
                                id: taskRow
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8
                                CheckBox {
                                    objectName: "taskCheck"
                                    checked: taskDelegate.modelData.done
                                    Accessible.name: taskDelegate.modelData.text
                                    onClicked: backend.setTaskDone(taskDelegate.modelData.id, checked)
                                }
                                Label {
                                    text: taskDelegate.modelData.text
                                    textFormat: Text.PlainText
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    color: taskDelegate.modelData.done ? "#999da6" : "#e9e9ed"
                                    font.strikeout: taskDelegate.modelData.done
                                    font.pixelSize: 15
                                }
                                JotButton {
                                    text: qsTr("Edit")
                                    onClicked: { editDialog.taskId = taskDelegate.modelData.id; editText.text = taskDelegate.modelData.text; editDialog.open() }
                                }
                                JotButton { text: "×"; Accessible.name: qsTr("Delete task"); onClicked: win.requestDelete("task", taskDelegate.modelData.id) }
                            }
                        }
                        Column {
                            anchors.centerIn: parent
                            width: parent.width - 32
                            visible: taskList.count === 0
                            spacing: 12
                            Label {
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                text: search.text.length ? qsTr("No matching tasks") : backend.taskFilter === "done" ? qsTr("No completed tasks yet.") : backend.taskFilter === "open" && backend.doneCount > 0 ? qsTr("All caught up.") : qsTr("Make room for what matters.")
                                font.pixelSize: 23; wrapMode: Text.Wrap
                            }
                            Label { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: qsTr("Add a task above. Check it off when you’re done."); color: "#a1a4ad"; wrapMode: Text.Wrap }
                        }
                    }
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#2a2b30" }
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 22; Layout.rightMargin: 22
                Layout.preferredHeight: 38
                Label { text: qsTr("Local. Simple. Yours."); font.pixelSize: 11; color: "#999da6"; Layout.fillWidth: true }
                Label { text: qsTr("Ctrl+N  note    Ctrl+L  task    ?  shortcuts"); font.pixelSize: 11; color: "#999da6" }
            }
        }
    }

    component JotDialog: Dialog {
        id: dialog
        anchors.centerIn: Overlay.overlay
        width: Math.min(520, win.width - 64)
        modal: true
        padding: 24
        Material.theme: Material.Dark
        Material.accent: theme.accent
        Material.roundedScale: theme.radius
        Overlay.modal: Rectangle { color: "#b0000000" }
        background: Rectangle { color: "#222429"; radius: theme.radius; border.color: "#41444c" }
        header: Label {
            text: dialog.title
            color: "#f4f4f5"
            font.pixelSize: 22
            wrapMode: Text.Wrap
            padding: 24
        }
        contentItem.LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        contentItem.LayoutMirroring.childrenInherit: true
    }
    JotDialog {
        id: deleteDialog
        objectName: "deleteDialog"
        title: win.pendingKind === "note" ? qsTr("Delete this note?") : qsTr("Delete this task?")
        contentItem: Label { text: qsTr("You can restore it with Undo."); wrapMode: Text.Wrap }
        footer: DialogButtonBox {
            padding: 16
            background: Rectangle { color: "transparent" }
            JotButton { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            JotButton { objectName: "confirmDelete"; text: qsTr("Delete"); primary: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: win.pendingKind === "note" ? backend.deleteNote(win.pendingId) : backend.deleteTask(win.pendingId)
    }
    JotDialog {
        id: editDialog
        property string taskId: ""
        title: qsTr("Edit task")
        onOpened: { editText.forceActiveFocus(); editText.selectAll() }
        contentItem: JotField { id: editText; placeholderText: qsTr("Task"); selectByMouse: true; onAccepted: { if (backend.updateTask(editDialog.taskId, text)) editDialog.close() } }
        footer: DialogButtonBox {
            padding: 16
            background: Rectangle { color: "transparent" }
            JotButton { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            JotButton { text: qsTr("Save"); primary: true; enabled: editText.text.trim().length > 0; onClicked: { if (backend.updateTask(editDialog.taskId, editText.text)) editDialog.close() } }
        }
    }
    JotDialog {
        id: quitDialog
        title: qsTr("You have unsaved text")
        contentItem: Label { text: qsTr("Keep editing to save or copy your text, or discard it and quit."); wrapMode: Text.Wrap }
        footer: DialogButtonBox {
            padding: 16
            background: Rectangle { color: "transparent" }
            JotButton { text: qsTr("Keep editing"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            JotButton { text: qsTr("Discard and quit"); onClicked: { win.allowClose = true; win.close() } }
        }
    }
    JotDialog {
        id: helpDialog
        objectName: "helpDialog"
        title: qsTr("Keyboard shortcuts")
        contentItem: ColumnLayout {
            spacing: 14
            Repeater {
                model: [
                    ["Ctrl+N", qsTr("New note")], ["Ctrl+L", qsTr("New task")],
                    ["Ctrl+1 / Ctrl+2", qsTr("Switch notes / to-do")], ["Ctrl+F", qsTr("Search")],
                    ["Ctrl+S", qsTr("Save now")], ["Ctrl+Z", qsTr("Undo")],
                    ["Ctrl+Shift+Z", qsTr("Redo")], ["Space", qsTr("New note or task")],
                    ["Enter", qsTr("Add the task you’re typing")], ["Esc", qsTr("Leave the editor")],
                    ["? / F1", qsTr("Keyboard shortcuts")], ["Q / Ctrl+Q", qsTr("Quit")]
                ]
                RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Label { text: modelData[0]; color: theme.accent; Layout.preferredWidth: 160; font.pixelSize: 12 }
                    Label { text: modelData[1]; Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: 13 }
                }
            }
            Label { text: qsTr("Single-key shortcuts work outside text fields."); color: "#a1a4ad"; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 12 }
        }
        footer: DialogButtonBox {
            padding: 16
            background: Rectangle { color: "transparent" }
            JotButton { text: qsTr("Close"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
    }
}
