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
import QtQml.Models
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

    function clampGraphTarget(value) {
        return Math.max(-1.0, Math.min(1.0, value))
    }

    function depthAspectRatio(count) {
        // X must remain the longest horizontal axis so Front uses the full
        // editor width. Give larger analyses progressively more Z depth, but
        // never let depth become physically longer than X.
        return Math.max(1.25, Math.min(12.0,
                        16.0 / Math.sqrt(Math.max(1, count))))
    }

    BreakpointGraphModel {
        id: breakpointGraphModel
        document: hydraDocument
        mode: window.editorMode
    }

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

        Item {
            id: editor
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 300

            // Qt Graphs keeps a separate primary 3D sub-viewport. Its default
            // is only a fraction of the available viewport, so explicitly keep
            // it synchronized with this editor area (in device pixels).
            function syncGraphViewport() {
                if (!breakpointGraph.scene)
                    return
                const dpr = Math.max(1.0, breakpointGraph.scene.devicePixelRatio)
                breakpointGraph.scene.primarySubViewport = Qt.rect(
                    0, 0,
                    Math.max(1, Math.round(width * dpr)),
                    Math.max(1, Math.round(height * dpr)))
            }

            onWidthChanged: syncGraphViewport()
            onHeightChanged: syncGraphViewport()

            Rectangle {
                anchors.fill: parent
                color: "#181a1d"
            }

            Scatter3D {
                id: breakpointGraph

                // Scatter3D is documented by Qt as a Qt Quick graph item with
                // width/height. Qt Creator 6.11 currently reports M16 for these
                // inherited properties, so suppress that tooling false-positive.
                // qmllint disable
                width: Math.max(1, editor.width)
                height: Math.max(1, editor.height)
                // qmllint enable

                // The partial index is display-only depth. Data editing never
                // modifies Z; left-drag always changes time/value only.
                axisX: Value3DAxis {
                    min: 0
                    max: breakpointGraphModel.durationMs
                    title: qsTr("Time (ms)")
                    titleVisible: true
                    segmentCount: 5
                    subSegmentCount: 2
                    labelFormat: "%.0f"
                }
                axisY: Value3DAxis {
                    min: 0
                    max: breakpointGraphModel.maximumY
                    title: window.editorMode === 0 ? qsTr("Amplitude") : qsTr("Frequency (Hz)")
                    titleVisible: true
                    segmentCount: 4
                    subSegmentCount: 2
                    labelFormat: "%.0f"
                }
                axisZ: Value3DAxis {
                    // Leave half/one lane of breathing room at both depth edges.
                    min: 0
                    max: breakpointGraphModel.maximumZ + 1
                    title: qsTr("Partial")
                    titleVisible: true
                    segmentCount: Math.min(8, Math.max(1, hydraDocument.partialCount - 1))
                    subSegmentCount: 1
                    labelFormat: "%.0f"
                }

                orthoProjection: true
                cameraPreset: Graphs3D.CameraPreset.FrontLow
                cameraZoomLevel: 100
                minCameraZoomLevel: 25
                maxCameraZoomLevel: 500
                // Each partial owns a fixed Z lane. Increase physical depth as
                // more lanes are added, so neighboring partials remain visibly
                // separated after the user rotates the graph.
                horizontalAspectRatio: window.depthAspectRatio(hydraDocument.partialCount)
                // Match X/Y physical scaling to the actual editor rectangle.
                // This makes Front fill the available area and react to window
                // resizing like the former 2D painter implementation.
                aspectRatio: Math.max(0.25, editor.width / Math.max(1.0, editor.height))
                margin: 0.04
                rotationEnabled: true
                zoomEnabled: true
                zoomAtTargetEnabled: false
                // Flat technical drawing: no directional/specular shading.
                // Ambient light at full strength keeps the configured colors
                // uniform while the per-series Unshaded mode removes highlights.
                ambientLightStrength: 1.0
                lightStrength: 0.0
                shadowStrength: 0.0
                selectionEnabled: true
                // MultiSeries is not supported by Scatter3D. hYdra2 keeps its
                // own multi-selection and draws it using selectedPointSeries.
                selectionMode: Graphs3D.SelectionFlag.Item
                shadowQuality: Graphs3D.ShadowQuality.None
                gridLineType: Graphs3D.GridLineType.Shader
                optimizationHint: Graphs3D.OptimizationHint.Default

                theme: GraphsTheme {
                    theme: GraphsTheme.Theme.UserDefined
                    colorScheme: GraphsTheme.ColorScheme.Dark
                    backgroundColor: "#181a1d"
                    plotAreaBackgroundColor: "#202328"
                    backgroundVisible: true
                    plotAreaBackgroundVisible: true
                    gridVisible: true
                    grid.mainColor: "#343840"
                    grid.subColor: "#292d33"
                    labelTextColor: "#d2d6dc"
                    labelBackgroundVisible: false
                    labelBorderVisible: false
                    singleHighlightColor: "#ffd640"
                    multiHighlightColor: "#4eb5ff"
                }

                Component.onCompleted: {
                    setDragButton(Qt.RightButton)
                    editor.syncGraphViewport()
                }

                // doPicking() completes asynchronously. The edit overlay stores
                // the press state and handles the result here.
                onSelectedElementChanged: breakpointEditArea.completePick()
                onSelectedSeriesChanged: breakpointEditArea.completePick()

                // Bright overlay for all selected breakpoints. It is a separate
                // scatter series because Qt Graphs natively highlights only one
                // picked item at a time.
                Scatter3DSeries {
                    id: selectedPointSeries
                    property bool hydraSelectionOverlay: true
                    baseColor: "#ffd640"
                    singleHighlightColor: "#fff0a0"
                    itemSize: 0.018
                    mesh: Abstract3DSeries.Mesh.Cube
                    meshSmooth: false
                    lightingMode: Abstract3DSeries.LightingMode.Unshaded
                    itemLabelVisible: false
                    onSelectedItemChanged: {
                        if (breakpointEditArea.pendingPick
                                && selectedItem !== invalidSelectionIndex)
                            breakpointEditArea.completePick()
                    }
                    dataProxy: ItemModelScatterDataProxy {
                        itemModel: breakpointGraphModel.selectedPointModel
                        xPosRole: "xPos"
                        yPosRole: "yPos"
                        zPosRole: "zPos"
                    }
                }
            }
                Instantiator {
                    id: partialSeriesInstantiator
                    model: breakpointGraphModel

                    delegate: Spline3DSeries {
                        required property int partialIndex
                        required property var pointModel
                        required property bool partialSelected

                        property bool hydraSelectionOverlay: false

                        name: qsTr("Partial %1").arg(partialIndex + 1)
                        baseColor: partialSelected ? "#4eb5ff" : "#68717c"
                        splineColor: partialSelected ? "#4eb5ff" : "#68717c"
                        singleHighlightColor: "#ffd640"
                        multiHighlightColor: "#4eb5ff"
                        itemSize: partialSelected ? 0.015 : 0.011
                        mesh: Abstract3DSeries.Mesh.Cube
                        meshSmooth: false
                        lightingMode: Abstract3DSeries.LightingMode.Unshaded
                        splineVisible: true
                        splineTension: 1.0
                        splineResolution: 3
                        splineLooping: false
                        // hYdra2 draws its own compact value label, avoiding
                        // one floating 3D label allocation per native selection.
                        itemLabelVisible: false

                        onSelectedItemChanged: {
                            if (breakpointEditArea.pendingPick
                                    && selectedItem !== invalidSelectionIndex)
                                breakpointEditArea.completePick()
                        }

                        dataProxy: ItemModelScatterDataProxy {
                            itemModel: pointModel
                            xPosRole: "xPos"
                            yPosRole: "yPos"
                            zPosRole: "zPos"
                        }
                    }

                    onObjectAdded: function(index, object) {
                        breakpointGraph.addSeries(object)
                    }
                    onObjectRemoved: function(index, object) {
                        if (breakpointGraph.hasSeries(object))
                            breakpointGraph.removeSeries(object)
                    }
                }


            // Qt Graphs owns rendering/camera. This transparent overlay owns
            // only data editing. Right mouse goes through to the graph for
            // camera rotation; wheel events are also passed through for zoom.
            MouseArea {
                id: breakpointEditArea
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                hoverEnabled: true
                preventStealing: true

                property point lastPos: Qt.point(0, 0)
                property bool dataDrag: false
                property bool panDrag: false
                property bool pressHeld: false
                property bool pendingPick: false
                property int pendingModifiers: Qt.NoModifier
                property real pendingZoom: 0.0
                property real pendingOldZoom: 0.0

                function isFrontView() {
                    return Math.abs(breakpointGraph.cameraXRotation) < 0.1
                        && Math.abs(breakpointGraph.cameraYRotation) < 0.1
                }

                // Picking in a large 3D graph can complete on a later render
                // pass. Keep a generous empty-space fallback instead of the old
                // 40 ms timeout, which could beat a valid GPU pick.
                Timer {
                    id: pickFallback
                    interval: 300
                    repeat: false
                    onTriggered: breakpointEditArea.completePick()
                }

                // graphPositionQuery is asynchronous and completes on a render
                // pass. The Connections object below waits for Scene3D to reset
                // graphPositionQuery to invalidSelectionPoint, which Qt does
                // after the result has been written to queriedGraphPosition.
                function completePick() {
                    if (!pendingPick)
                        return

                    const series = breakpointGraph.selectedSeries
                    if (breakpointGraph.selectedElement !== Graphs3D.ElementType.Series || !series) {
                        // A miss is only final when the fallback timer fires.
                        if (pickFallback.running)
                            return
                        pendingPick = false
                        if (!(pendingModifiers & (Qt.ShiftModifier | Qt.ControlModifier | Qt.MetaModifier)))
                            breakpointGraphModel.clearSelection()
                        return
                    }

                    pendingPick = false
                    pickFallback.stop()

                    const selectedIndex = series.selectedItem
                    let started = false

                    if (series.hydraSelectionOverlay === true) {
                        if (selectedIndex !== series.invalidSelectionIndex) {
                            started = breakpointGraphModel.beginSelectedPointDrag(
                                selectedIndex, pendingModifiers, window.logicSnap)
                        }
                    } else if (selectedIndex !== series.invalidSelectionIndex) {
                        started = breakpointGraphModel.beginPointDrag(
                            series.partialIndex, selectedIndex, pendingModifiers, window.logicSnap)
                    } else {
                        // If the backend identifies the spline series without a
                        // control point, treat it as a whole-partial selection.
                        started = breakpointGraphModel.beginPartialDrag(
                            series.partialIndex, pendingModifiers, window.logicSnap)
                    }

                    dataDrag = started && pressHeld
                    if (started && !pressHeld)
                        breakpointGraphModel.endDrag()
                }

                function applyPendingZoom() {
                    if (pendingZoom <= 0.0)
                        return

                    const oldZoom = Math.max(1.0, pendingOldZoom)
                    const newZoom = pendingZoom
                    const factor = newZoom / oldZoom
                    const q = breakpointGraph.queriedGraphPosition
                    const t = breakpointGraph.cameraTargetPosition

                    if (q.x >= -1.0 && q.x <= 1.0
                            && q.y >= -1.0 && q.y <= 1.0
                            && q.z >= -1.0 && q.z <= 1.0) {
                        // Keep q at the same screen position after the zoom:
                        // (q - newTarget) * newZoom == (q - oldTarget) * oldZoom
                        breakpointGraph.cameraTargetPosition = Qt.vector3d(
                            window.clampGraphTarget(q.x - (q.x - t.x) / factor),
                            window.clampGraphTarget(q.y - (q.y - t.y) / factor),
                            window.clampGraphTarget(q.z - (q.z - t.z) / factor))
                    }

                    breakpointGraph.cameraZoomLevel = newZoom
                    pendingZoom = 0.0
                }

                onPressed: function(mouse) {
                    lastPos = Qt.point(mouse.x, mouse.y)
                    dataDrag = false
                    pressHeld = true

                    // Option-left or middle mouse pans the camera target. This
                    // moves the view only; no breakpoint coordinates change.
                    panDrag = mouse.button === Qt.MiddleButton
                           || (mouse.button === Qt.LeftButton
                               && (mouse.modifiers & Qt.AltModifier))
                    if (panDrag) {
                        pendingPick = false
                        pickFallback.stop()
                        return
                    }

                    pendingModifiers = mouse.modifiers
                    pendingPick = true
                    breakpointGraph.clearSelection()

                    // This is the same custom-input pattern used by Qt's
                    // official Axis Handling example. MouseArea owns the left
                    // click and explicitly asks Scatter3D to pick at that view
                    // coordinate.
                    pickFallback.restart()
                    breakpointGraph.doPicking(Qt.point(mouse.x, mouse.y))
                }

                onPositionChanged: function(mouse) {
                    if (!pressed)
                        return

                    const dx = mouse.x - lastPos.x
                    const dy = mouse.y - lastPos.y

                    if (panDrag) {
                        const scale = 100.0 / Math.max(1.0, breakpointGraph.cameraZoomLevel)
                        const t = breakpointGraph.cameraTargetPosition
                        breakpointGraph.cameraTargetPosition = Qt.vector3d(
                            window.clampGraphTarget(t.x - dx * 2.0 / width * scale),
                            window.clampGraphTarget(t.y + dy * 2.0 / height * scale),
                            t.z)
                        lastPos = Qt.point(mouse.x, mouse.y)
                        return
                    }

                    if (!dataDrag)
                        return

                    // Editing remains 2D even with a rotated camera: mouse X
                    // changes time, mouse Y changes value, and Z is immutable.
                    breakpointGraphModel.dragByPixels(
                        dx, dy, width, height,
                        breakpointGraph.cameraZoomLevel, window.logicSnap)
                    lastPos = Qt.point(mouse.x, mouse.y)
                }

                onReleased: function(mouse) {
                    pressHeld = false
                    panDrag = false
                    breakpointGraphModel.endDrag()
                    dataDrag = false
                }

                onCanceled: {
                    pressHeld = false
                    panDrag = false
                    pendingPick = false
                    pickFallback.stop()
                    breakpointGraphModel.endDrag()
                    dataDrag = false
                }

                onWheel: function(wheel) {
                    const rawDelta = wheel.angleDelta.y !== 0
                                   ? wheel.angleDelta.y
                                   : wheel.pixelDelta.y * 2.0
                    if (rawDelta === 0)
                        return

                    const oldZoom = Math.max(1.0, breakpointGraph.cameraZoomLevel)
                    const factor = Math.pow(1.001, rawDelta)
                    const newZoom = Math.max(
                        breakpointGraph.minCameraZoomLevel,
                        Math.min(breakpointGraph.maxCameraZoomLevel,
                                 oldZoom * factor))

                    // For the orthographic Front view, move the camera target
                    // toward the cursor while zooming. In rotated 3D views the
                    // graph still zooms around its current target.
                    if (isFrontView()) {
                        const zoomFactor = newZoom / oldZoom
                        const qx = (wheel.x / Math.max(1.0, width)) * 2.0 - 1.0
                        const qy = 1.0 - (wheel.y / Math.max(1.0, height)) * 2.0
                        const t = breakpointGraph.cameraTargetPosition
                        breakpointGraph.cameraTargetPosition = Qt.vector3d(
                            window.clampGraphTarget(qx - (qx - t.x) / zoomFactor),
                            window.clampGraphTarget(qy - (qy - t.y) / zoomFactor),
                            t.z)
                    }

                    breakpointGraph.cameraZoomLevel = newZoom
                    wheel.accepted = true
                }
            }

            Label {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 12
                visible: breakpointGraphModel.activeLabel.length > 0
                text: breakpointGraphModel.activeLabel
                color: "#e6e9ee"
                padding: 6
                background: Rectangle {
                    color: "#eb16181c"
                    radius: 4
                }
            }

            Row {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                spacing: 8

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Left: edit   Option/middle: pan   Right: rotate   Wheel: zoom")
                    color: "#9299a3"
                    font.pixelSize: 11
                }

                Button {
                    text: qsTr("Front")
                    onClicked: {
                        breakpointGraph.cameraPreset = Graphs3D.CameraPreset.FrontLow
                        breakpointGraph.cameraTargetPosition = Qt.vector3d(0, 0, 0)
                        breakpointGraph.cameraZoomLevel = 100
                    }
                }
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
                        // Keep one category per bar, but draw our own labels below
                        // the plot area so their centers align exactly with bars.
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
                    // Match the actual GPU plot rectangle rather than the whole
                    // GraphsView, so bar hit-testing stays exact at the edges.
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
