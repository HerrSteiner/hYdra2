/*
Copyright (C) 2026 Malte Steiner

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtGraphs
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
    property bool logicSnap: true

    function partialCategories(count) {
        let categories = []
        for (let i = 0; i < count; ++i)
            categories.push("")
        return categories
    }

    function partialLabelStep(count) {
        return Math.max(1, Math.ceil(count / 16))
    }

    function showPartialLabel(index, count) {
        const step = partialLabelStep(count)
        return (index % step) === 0 || index === count - 1
    }

    function timeTick(index) {
        const ms = editor.viewTimeMinimum
                 + (editor.viewTimeMaximum - editor.viewTimeMinimum) * index / 5.0
        const seconds = ms / 1000.0
        const span = editor.viewTimeMaximum - editor.viewTimeMinimum
        return seconds.toFixed(span < 10000 ? 2 : (span < 60000 ? 1 : 0)) + " s"
    }

    function valueTick(index) {
        const value = editor.viewValueMinimum
                    + (editor.viewValueMaximum - editor.viewValueMinimum) * index / 4.0
        const rounded = Math.round(value)
        return editorMode === 0 ? String(rounded) : String(rounded) + " Hz"
    }

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
        id: normalizeAmplitudesAction
        text: qsTr("Normalize Amplitudes")
        enabled: hydraDocument.loaded && window.editorMode === 0
        onTriggered: hydraDocument.normalizeAmplitudes()
    }

    Action {
        id: logicSnapAction
        text: qsTr("Logic Snap")
        checkable: true
        checked: window.logicSnap
        onToggled: window.logicSnap = checked
    }

    property var overviewWindowInstance: null

    Component {
        id: overview3DComponent
        Overview3D {
            document: hydraDocument
            mode: window.editorMode
        }
    }

    Action {
        id: overview3DAction
        text: qsTr("3D Overview")
        enabled: hydraDocument.loaded
        onTriggered: {
            if (!window.overviewWindowInstance)
                window.overviewWindowInstance = overview3DComponent.createObject(window)
            window.overviewWindowInstance.visible = true
            window.overviewWindowInstance.raise()
            window.overviewWindowInstance.requestActivate()
        }
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
            title: qsTr("Edit")
            MenuItem { action: normalizeAmplitudesAction }
            MenuSeparator { }
            MenuItem {
                action: logicSnapAction
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: qsTr("Prevents breakpoints from crossing neighboring points when dragged.")
            }
        }

        Menu {
            title: qsTr("View")
            MenuItem { action: overview3DAction }
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
        onAccepted: hydraDocument.saveAs(selectedFile, selectedNameFilter.index)
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

                CheckBox {
                    id: focusSelected
                    text: qsTr("Focus on selected partials")
                    checked: false
                    enabled: hydraDocument.loaded
                }

                Button {
                    text: qsTr("Fit")
                    enabled: hydraDocument.loaded
                    onClicked: editor.fitView()
                }

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

        Item {
            id: editorPane
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 300

            BreakpointEditor {
                id: editor
                anchors.fill: parent
                document: hydraDocument
                mode: window.editorMode
                logicSnap: window.logicSnap
                focusOnSelected: focusSelected.checked
            }

            Repeater {
                model: 6
                delegate: Label {
                    required property int index
                    width: 76
                    height: 20
                    x: 58 + (editor.width - 58 - 16) * index / 5.0 - width / 2
                    y: editor.height - 30
                    text: window.timeTick(index)
                    color: "#d2d6dc"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignTop
                }
            }

            Repeater {
                model: 5
                delegate: Label {
                    required property int index
                    x: 3
                    width: 48
                    height: 18
                    y: 18 + (editor.height - 18 - 38) * (4 - index) / 4.0 - height / 2
                    text: window.valueTick(index)
                    color: "#d2d6dc"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Label {
                anchors.centerIn: parent
                visible: !hydraDocument.loaded
                text: qsTr("Open a Csound HETRO / adsyn analysis file")
                color: "#aaafb8"
            }

            Rectangle {
                id: activePointLabel
                visible: editor.activeLabel.length > 0
                color: "#eb16181c"
                radius: 4
                width: activePointText.implicitWidth + 12
                height: activePointText.implicitHeight + 8
                x: Math.max(58, Math.min(editorPane.width - width - 8,
                                         editor.activeLabelX + 10))
                y: Math.max(4, Math.min(editorPane.height - height - 4,
                                        editor.activeLabelY - height - 7))

                Text {
                    id: activePointText
                    anchors.centerIn: parent
                    text: editor.activeLabel
                    color: "#e6e9ee"
                    font.pixelSize: 11
                }
            }

            Label {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                text: qsTr("Left: edit/select   Alt/middle: pan   Wheel: zoom")
                color: "#9299a3"
                font.pixelSize: 11
            }
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
                text: qsTr("Partials — drag vertically to adjust level; selected bars move together")
                color: "#c7cbd1"
            }

            Item {
                id: mixerPane
                Layout.fillWidth: true
                Layout.fillHeight: true

                GraphsView {
                    id: mixerGraph
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    anchors.bottomMargin: 20
                    marginLeft: 4
                    marginRight: 4
                    marginTop: 4
                    marginBottom: 4
                    panStyle: GraphsView.PanStyle.None
                    zoomStyle: GraphsView.ZoomStyle.None

                    theme: GraphsTheme {
                        theme: GraphsTheme.Theme.UserDefined
                        colorScheme: GraphsTheme.ColorScheme.Dark
                        backgroundColor: "#181a1d"
                        plotAreaBackgroundColor: "#202328"
                        backgroundVisible: true
                        plotAreaBackgroundVisible: true
                        gridVisible: false
                        labelTextColor: "#c7cbd1"
                    }

                    axisX: BarCategoryAxis {
                        categories: window.partialCategories(hydraDocument.partialCount)
                        visible: false
                        labelsVisible: false
                        lineVisible: false
                        gridVisible: false
                        subGridVisible: false
                    }

                    axisY: ValueAxis {
                        min: 0
                        max: 1
                        visible: false
                    }

                    BarSeries {
                        id: mixerBarSeries
                        barWidth: 0.82
                        labelsVisible: false

                        barDelegate: Rectangle {
                            property color barColor
                            property color barBorderColor
                            property real barBorderWidth
                            property real barValue
                            property string barLabel
                            property bool barSelected
                            property int barIndex

                            color: barSelected ? "#dc4eb5ff" : "#aab2bbc7"
                            border.color: barSelected ? "#87d7ff" : "transparent"
                            border.width: barSelected ? 1.25 : 0
                            radius: Math.min(2, width * 0.2)
                        }

                        BarSet {
                            id: mixerBarSet
                            values: mixerController.values
                            color: "#b2bbc7"
                            selectedColor: "#4eb5ff"
                        }
                    }
                }

                Repeater {
                    model: hydraDocument.partialCount
                    delegate: Label {
                        required property int index
                        visible: window.showPartialLabel(index, hydraDocument.partialCount)
                        width: 34
                        height: 16
                        x: mixerGraph.x + mixerGraph.plotArea.x
                           + ((index + 0.5) / Math.max(1, hydraDocument.partialCount))
                             * mixerGraph.plotArea.width
                           - width / 2
                        y: mixerGraph.y + mixerGraph.plotArea.y
                           + mixerGraph.plotArea.height + 2
                        text: String(index + 1)
                        color: "#bec3cb"
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignTop
                    }
                }

                PartialMixer {
                    id: mixerController
                    x: mixerGraph.x + mixerGraph.plotArea.x
                    y: mixerGraph.y + mixerGraph.plotArea.y
                    width: mixerGraph.plotArea.width
                    height: mixerGraph.plotArea.height
                    document: hydraDocument
                    z: 2
                }

                function syncMixerSelection() {
                    mixerBarSet.deselectAllBars()
                    const bars = mixerController.selectedBars
                    if (bars.length > 0)
                        mixerBarSet.selectBars(bars)
                }

                Connections {
                    target: mixerController
                    function onSelectionValuesChanged() {
                        mixerPane.syncMixerSelection()
                    }
                }

                Component.onCompleted: syncMixerSelection()
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
