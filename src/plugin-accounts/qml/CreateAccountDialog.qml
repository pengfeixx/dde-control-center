// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Dialogs
import QtQuick.Window
import QtQml.Models
import QtQuick.Layouts 1.15
import org.deepin.dtk 1.0 as D
import org.deepin.dtk.style 1.0 as DS
import org.deepin.dcc 1.0

D.DialogWindow {
    id: dialog
    width: 460
    height: 432
    minimumWidth: width
    minimumHeight: height
    maximumWidth: minimumWidth
    maximumHeight: minimumHeight
    icon: "preferences-system"
    modality: Qt.WindowModal
    title: qsTr("Create a new account")

    signal accepted()

    ColumnLayout {
        id: dialogLayout
        width: dialog.width - 6
        spacing: 0
        property int maxLabelWidth: 64

        function getLabelWidth(width) {
            if (dialogLayout.maxLabelWidth === 100)
                return
            if (width > 100 || (pwdLayout.maxLabelWidth !== undefined && pwdLayout.maxLabelWidth > 100)) {
                dialogLayout.maxLabelWidth = 100
                return
            }
        
            let max = -Infinity
            const values = [dialogLayout.maxLabelWidth, width]
            if (pwdLayout.maxLabelWidth !== undefined) {
                values.push(pwdLayout.maxLabelWidth)
            }
            values.forEach(n => {
                if (n <= 100 && n > max) {
                    max = n
                }
            });
        
            dialogLayout.maxLabelWidth = max
            pwdLayout.maxLabelWidth = dialogLayout.maxLabelWidth
        }

        Label {
            text: dialog.title
            font.bold: true
            Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
            Layout.bottomMargin: 20
        }

        RowLayout {
            implicitHeight: 40
            Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.bottomMargin: 10
            spacing: 10

            Label {
                id: typeLabel
                text: qsTr("Account type")
                Layout.preferredWidth: dialogLayout.maxLabelWidth
                Layout.alignment: Qt.AlignVCenter
                Layout.leftMargin: 10
                Component.onCompleted: {
                    dialogLayout.getLabelWidth(contentWidth)
                }
            }

            ComboBox {
                id: userType
                implicitHeight: 30
                Layout.alignment: Qt.AlignVCenter
                Layout.rightMargin: 10
                Layout.fillWidth: true
                model: dccData.userTypes(true)
            }
        }

        ListModel {
            id: namesModel
            ListElement {
                name: qsTr("UserName")
                placeholder: qsTr("Required")
            }
            ListElement {
                name: qsTr("FullName")
                placeholder: qsTr("Optional")
            }
        }

        Rectangle {
            id: namesContainter
            property var eidtItems: []
            radius: 8
            Layout.fillWidth: true
            Layout.bottomMargin: 20
            Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
            implicitHeight: 68
            color: "transparent"

            function checkNames() {
                let edit0 = namesContainter.eidtItems[0]
                let edit1 = namesContainter.eidtItems[1]
                if (!edit0 || !edit1)
                    return true
                let alertText = dccData.checkUsername(edit0.text)
                if (alertText.length > 0) {
                    // TODO: dtk fixme, showAlert tip timeout cannot show alertText again
                    edit0.showAlert = false
                    edit0.showAlert = true
                    edit0.alertText = alertText
                    return false
                }

                alertText = dccData.checkFullname(edit1.text)
                if (alertText.length > 0) {
                    edit1.showAlert = false
                    edit1.showAlert = true
                    edit1.alertText = alertText
                    return false
                }

                return true
            }

            ColumnLayout {
                spacing: 8
                anchors.fill: parent
                Repeater {
                    model: namesModel
                    delegate: D.ItemDelegate {
                        Layout.fillWidth: true
                        backgroundVisible: false
                        checkable: false
                        implicitHeight: 30
                        leftPadding: 10
                        rightPadding: 10

                        contentItem: RowLayout {
                            spacing: 0
                            Label {
                                id: name
                                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                                text: model.name
                                Layout.preferredWidth: dialogLayout.maxLabelWidth
                                Layout.rightMargin: 10
                            }
                            D.LineEdit {
                                id: edit
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                placeholderText: model.placeholder
                                background.implicitHeight: 30
                                alertDuration: 3000
                                // can not past
                                // validator: RegularExpressionValidator {
                                //     regularExpression: index ? /^[^:]{0,32}$/ : /[A-Za-z0-9-_]{0,32}$/
                                // }
                                onTextChanged: {
                                    if (showAlert)
                                        showAlert = false

                                    var regex = index ? /^[^:]{0,32}$/ : /^[A-Za-z0-9-_]{0,32}$/
                                    if (!regex.test(text)) {
                                        var filteredText = text
                                        if (index === 0) {
                                            filteredText = filteredText.replace(/[^A-Za-z0-9-_]/g, "")
                                        } else {
                                            filteredText = filteredText.replace(":", "")
                                        }

                                        // 长度 32
                                        filteredText = filteredText.slice(0, 32)
                                        text = filteredText
                                    }
                                }
                                Component.onCompleted: {
                                    namesContainter.eidtItems[index] = this
                                }
                            }
                        }

                        background: DccItemBackground {
                            separatorVisible: true
                        }
                        Component.onCompleted: {
                            dialogLayout.getLabelWidth(name.contentWidth)
                        }
                    }
                }
            }
        }

        PasswordLayout {
            id: pwdLayout
            currentPwdVisible: false
            Layout.fillWidth: true
            name:  {
                let nameEdit = namesContainter.eidtItems[0]
                if (nameEdit === undefined)
                    return ""

                return nameEdit.text
            }
        }

        RowLayout {
            spacing: 6
            Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter
            Layout.bottomMargin: 6
            Layout.leftMargin: 6
            Layout.rightMargin: 10

            Button {
                Layout.fillWidth: true
                implicitHeight: 30
                text: qsTr("Cancel")
                onClicked: {
                    close()
                }
            }
            D.RecommandButton {
                Layout.fillWidth: true
                implicitHeight: 30
                text: qsTr("Create account")
                onClicked: {
                    if (!namesContainter.checkNames())
                        return

                    if (!pwdLayout.checkPassword())
                        return

                    var info = pwdLayout.getPwdInfo()
                    info["type"] = userType.currentIndex
                    info["name"] = namesContainter.eidtItems[0].text
                    info["fullname"] = namesContainter.eidtItems[1].text

                    dccData.addUser(info);
                    dialog.accepted()
                    close()
                }
            }
        }
    }
}
