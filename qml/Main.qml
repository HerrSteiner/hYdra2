/*
Copyright (C) 2026 Malte Steiner

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Hydra2.Native 1.0

ApplicationWindow {
    id: window
    width: 1240
    height: 820
    minimumWidth: 860
    minimumHeight: 600
    visible: true
    title: (hydraDocument.dirty ? "*" : "") + hydraDocument.fileName + " — hYdra2"

    property int editorMode: 0

    // Actions live outside the Menu. Otherwise an Action declared inside a Menu
    // already becomes a menu entry and adding a MenuItem for it duplicates it.
    Action {
        id: openAction
        text: qsTr("Open…")
        shortcut: StandardKey.Open
        onTriggered: openDialog.open()
    }

    Action {
        id: saveAction
        text: qsTr("Save")
        shortcut: StandardKey.Save
        enabled: hydraDocument.loaded
        onTriggered: hydraDocument.save()
    }

    Action {
        id: saveAsAction
        text: qsTr("Save As…")
        shortcut: StandardKey.SaveAs
        enabled: hydraDocument.loaded
        onTriggered: saveDialog.open()
    }

    Action {
        id: quitAction
        text: qsTr("Quit")
        shortcut: StandardKey.Quit
        onTriggered: Qt.quit()
    }

    Action {
        id: aboutAction
        text: qsTr("About hYdra2")
        onTriggered: aboutDialog.open()
    }

    menuBar: MenuBar {
        Menu {
            title: qsTr("File")
            MenuItem { action: openAction }
            MenuSeparator { }
            MenuItem { action: saveAction }
            MenuItem { action: saveAsAction }
            MenuSeparator { }
            MenuItem { action: quitAction }
        }

        Menu {
            title: qsTr("Help")
            MenuItem { action: aboutAction }
        }
    }

    FileDialog {
        id: openDialog
        title: qsTr("Open HETRO / adsyn analysis")
        fileMode: FileDialog.OpenFile
        nameFilters: [
            qsTr("Csound analysis files (*.het *.ads)"),
            qsTr("All files (*)")
        ]
        onAccepted: hydraDocument.openUrl(selectedFile)
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Save analysis")
        fileMode: FileDialog.SaveFile

        nameFilters: [
            qsTr("Current HETRO text (*.het)"),
            qsTr("Legacy adsyn binary (*.ads)")
        ]

        defaultSuffix: selectedNameFilter.index === 1 ? "ads" : "het"

        onAccepted: hydraDocument.saveAs(selectedFile,selectedNameFilter.index)
    }

    Dialog {
        id: aboutDialog
        title: qsTr("About hYdra2")
        modal: true
        standardButtons: Dialog.Ok
        anchors.centerIn: parent
        width: 420

        contentItem: ColumnLayout {
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: qsTr("hYdra2")
                font.pixelSize: 24
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Version %1").arg(Qt.application.version)
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Csound HETRO / ADSYN analysis editor")
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                Layout.fillWidth: true
                text: "© 1998–1999, 2026 Malte Steiner"
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Licensed under GNU GPLv3")
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ToolBar {
            Layout.fillWidth: true
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Label {
                    text: hydraDocument.loaded
                          ? qsTr("%1 partials").arg(hydraDocument.partialCount)
                          : qsTr("No file loaded")
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("Amplitude")
                    checkable: true
                    checked: window.editorMode === 0
                    onClicked: window.editorMode = 0
                }
                Button {
                    text: qsTr("Frequency")
                    checkable: true
                    checked: window.editorMode === 1
                    onClicked: window.editorMode = 1
                }
            }
        }

        BreakpointEditor {
            id: editor
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 300
            document: hydraDocument
            mode: window.editorMode
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#3a3d43"
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 190
            Layout.minimumHeight: 120
            spacing: 2

            Label {
                Layout.leftMargin: 12
                Layout.topMargin: 6
                text: qsTr("Partials — drag a bar vertically to attenuate; zero remains recoverable until save + reload")
                color: "#c7cbd1"
            }

            PartialMixer {
                Layout.fillWidth: true
                Layout.fillHeight: true
                document: hydraDocument
            }
        }

        Frame {
            Layout.fillWidth: true
            padding: 6
            RowLayout {
                anchors.fill: parent
                Label {
                    text: hydraDocument.errorString.length > 0
                          ? hydraDocument.errorString
                          : (hydraDocument.loaded
                             ? qsTr("Duration %1 s").arg((hydraDocument.durationMs / 1000.0).toFixed(3))
                             : qsTr("Ready"))
                    color: hydraDocument.errorString.length > 0 ? "#ff9b9b" : palette.text
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    visible: hydraDocument.dirty
                    text: qsTr("Modified")
                    font.bold: true
                }
            }
        }
    }
}
