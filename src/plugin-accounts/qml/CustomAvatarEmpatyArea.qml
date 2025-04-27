// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Shapes
import org.deepin.dtk 1.0 as D

Control {
    id: control
    property real radius: 8
    width: 430
    height: 360

    signal requireFileDialog()
    signal iconDropped(string file)

    Shape {
        id: shape
        anchors.centerIn: parent
        width: 400
        height: 300
        layer.enabled: true
        layer.samples: 4
        layer.smooth: true

        ShapePath {
            strokeStyle: ShapePath.DashLine
            strokeWidth: 1
            fillColor: palette.window
            strokeColor: "gray"
            startX: control.radius
            startY: 0

            PathLine { x: shape.width - control.radius; y: 0 }
            PathQuad { x: shape.width; y: control.radius; controlX: shape.width; controlY: 0 }
            PathLine { x: shape.width; y: shape.height - control.radius }
            PathQuad { x: shape.width - control.radius; y: shape.height; controlX: shape.width; controlY: shape.height }
            PathLine { x: control.radius; y: shape.height }
            PathQuad { x: 0; y: shape.height - control.radius; controlX: 0; controlY: shape.height }
            PathLine { x: 0; y: control.radius }
            PathQuad { x: control.radius; y: 0; controlX: 0; controlY: 0 }
        }

        MouseArea {
            id: dropMouseArea // Add id to reference this MouseArea
            anchors.fill: parent
            // Keep existing onClicked handler
            onClicked: {
                requireFileDialog()
            }
            // Set default cursor shape
            cursorShape: Qt.ArrowCursor
        }
        DropArea {
            anchors.fill: parent
            enabled: true

            // Set cursor based on dragged file type via the MouseArea
            onEntered: function(drag) {
                if (drag.hasUrls && drag.urls.length > 0) {
                    var fileUrl = drag.urls[0];
                    var filePath = fileUrl.toLocalFile ? fileUrl.toLocalFile() : fileUrl.toString();

                    // Check if the file extension matches the allowed types (case-insensitive)
                    if (filePath.toLowerCase().endsWith(".png") ||
                        filePath.toLowerCase().endsWith(".bmp") ||
                        filePath.toLowerCase().endsWith(".jpg") ||
                        filePath.toLowerCase().endsWith(".jpeg"))
                    {
                        // Valid file type, suggest copy action
                        dropMouseArea.cursorShape = Qt.DragCopyCursor
                    } else {
                        // Invalid file type, show forbidden cursor
                        console.warn("----------------------")
                        // dropMouseArea.cursorShape = Qt.ForbiddenCursor
                        dropMouseArea.cursorShape = Qt.ArrowCursor
                    }
                } else {
                    // Not a file drag, reset cursor
                    dropMouseArea.cursorShape = Qt.ArrowCursor
                }
            }

            // Reset cursor on exit via the MouseArea
            onExited: {
                dropMouseArea.cursorShape = Qt.ArrowCursor // Reset to default arrow
            }

            onDropped: function (drop) {
                // The check in onDropped remains as a final validation before processing
                if (drop.hasUrls) {
                    var fileUrl = drop.urls[0];
                    // In QML, local file URLs might start with "file://", need to get the path part
                    var filePath = fileUrl.toLocalFile ? fileUrl.toLocalFile() : fileUrl.toString();

                    // Check if the file extension matches the allowed types (case-insensitive)
                    if (filePath.toLowerCase().endsWith(".png") ||
                        filePath.toLowerCase().endsWith(".bmp") ||
                        filePath.toLowerCase().endsWith(".jpg") ||
                        filePath.toLowerCase().endsWith(".jpeg"))
                    {
                        iconDropped(fileUrl) // Pass the original URL object
                    } else {
                        // Optional: Provide feedback to the user about invalid file type
                        console.log("Invalid file type dropped:", filePath);
                        // You might want to show a notification or message here using a component
                    }
                }
            }
        }
    }
    D.DciIcon {
        id: addIcon
        anchors.centerIn: parent
        name: "dcc_user_add_icon"
        sourceSize:  Qt.size(64, 64)
    }

    D.Label {
        width: 360
        wrapMode: Text.WordWrap
        anchors.top: addIcon.bottom
        anchors.topMargin: 20
        horizontalAlignment: Text.AlignHCenter
        anchors.horizontalCenter: parent.horizontalCenter
        text: qsTr("You haven't uploaded an avatar yet. Click or drag and drop to upload an image.")
    }
}
