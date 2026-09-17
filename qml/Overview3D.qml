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
import QtGraphs
import QtQml.Models
import Hydra2.Native 1.0

Window {
    id: root
    width: 1000
    height: 720
    minimumWidth: 640
    minimumHeight: 420
    visible: false
    title: qsTr("3D Overview — hYdra2")

    required property var document
    property int mode: 0

    function depthAspectRatio(count) {
        return Math.max(1.1, Math.min(8.0,
                        14.0 / Math.sqrt(Math.max(1, count))))
    }

    BreakpointGraphModel {
        id: graphModel
        document: root.document
        mode: root.mode
    }

    Rectangle {
        anchors.fill: parent
        color: "#181a1d"
    }

    Scatter3D {
        id: graph
        width: root.width
        height: root.height

        axisX: Value3DAxis {
            min: 0
            max: graphModel.durationMs
            title: qsTr("Time (ms)")
            titleVisible: true
            segmentCount: 5
            subSegmentCount: 2
            labelFormat: "%.0f"
        }
        axisY: Value3DAxis {
            min: 0
            max: graphModel.maximumY
            title: root.mode === 0 ? qsTr("Amplitude") : qsTr("Frequency (Hz)")
            titleVisible: true
            segmentCount: 4
            subSegmentCount: 2
            labelFormat: "%.0f"
        }
        axisZ: Value3DAxis {
            min: 0
            max: graphModel.maximumZ + 1
            title: qsTr("Partial")
            titleVisible: true
            segmentCount: Math.min(8, Math.max(1, root.document.partialCount - 1))
            subSegmentCount: 1
            labelFormat: "%.0f"
        }

        orthoProjection: false
        cameraPreset: Graphs3D.CameraPreset.IsometricRightHigh
        cameraZoomLevel: 100
        minCameraZoomLevel: 25
        maxCameraZoomLevel: 400
        horizontalAspectRatio: root.depthAspectRatio(root.document.partialCount)
        aspectRatio: 1.8
        margin: 0.08

        rotationEnabled: true
        zoomEnabled: true
        zoomAtTargetEnabled: true
        selectionEnabled: true
        selectionMode: Graphs3D.SelectionFlag.Item
        shadowQuality: Graphs3D.ShadowQuality.None
        optimizationHint: Graphs3D.OptimizationHint.Default
        gridLineType: Graphs3D.GridLineType.Shader
        ambientLightStrength: 1.0
        lightStrength: 0.0
        shadowStrength: 0.0

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

        Component.onCompleted: setDragButton(Qt.RightButton)
        onSelectedSeriesChanged: overviewInput.finishPick()
    }

    Instantiator {
        id: partialSeries
        model: graphModel

        delegate: Spline3DSeries {
            required property int partialIndex
            required property var pointModel
            required property bool partialSelected

            name: qsTr("Partial %1").arg(partialIndex + 1)
            baseColor: partialSelected ? "#4eb5ff" : "#68717c"
            splineColor: partialSelected ? "#4eb5ff" : "#68717c"
            singleHighlightColor: "#ffd640"
            itemSize: partialSelected ? 0.02 : 0.018 // the size of the breakpoints
            mesh: Abstract3DSeries.Mesh.Cube
            meshSmooth: false
            lightingMode: Abstract3DSeries.LightingMode.Unshaded
            splineVisible: true
            splineTension: 1.0
            splineResolution: 3
            splineLooping: false
            itemLabelVisible: false

            onSelectedItemChanged: {
                if (overviewInput.pendingPick
                        && selectedItem !== invalidSelectionIndex)
                    overviewInput.finishPick()
            }

            dataProxy: ItemModelScatterDataProxy {
                itemModel: pointModel
                xPosRole: "xPos"
                yPosRole: "yPos"
                zPosRole: "zPos"
            }
        }

        onObjectAdded: function(index, object) {
            graph.addSeries(object)
        }
        onObjectRemoved: function(index, object) {
            if (graph.hasSeries(object))
                graph.removeSeries(object)
        }
    }

    MouseArea {
        id: overviewInput
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true

        property bool pendingPick: false
        property int pendingModifiers: Qt.NoModifier

        function finishPick() {
            if (!pendingPick)
                return

            const series = graph.selectedSeries
            if (!series || series.partialIndex === undefined)
                return

            pendingPick = false
            missTimer.stop()
            graphModel.selectPartial(series.partialIndex, pendingModifiers)
            graph.clearSelection()
        }

        Timer {
            id: missTimer
            interval: 250
            repeat: false
            onTriggered: {
                if (!overviewInput.pendingPick)
                    return
                overviewInput.pendingPick = false
                if (!(overviewInput.pendingModifiers
                      & (Qt.ShiftModifier | Qt.ControlModifier | Qt.MetaModifier)))
                    graphModel.clearSelection()
            }
        }

        onPressed: function(mouse) {
            pendingModifiers = mouse.modifiers
            pendingPick = true
            graph.clearSelection()
            graph.doPicking(Qt.point(mouse.x, mouse.y))
            missTimer.restart()
        }

        onWheel: function(wheel) {
            wheel.accepted = false
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 10
        color: "#d916181c"
        radius: 4
        width: helpLabel.implicitWidth + 12
        height: helpLabel.implicitHeight + 8

        Text {
            id: helpLabel
            anchors.centerIn: parent
            text: qsTr("Left: select partial   Cmd/Ctrl: toggle selection   Right: rotate   Wheel: zoom")
            color: "#c7cbd1"
            font.pixelSize: 11
        }
    }
}
