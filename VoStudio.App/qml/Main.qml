import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMultimedia

ApplicationWindow {
    id: window
    width: 560
    height: 860
    minimumWidth: 320
    minimumHeight: 520
    visible: true
    title: "VoStudio"
    Material.theme: Material.Dark
    Material.accent: "#8bd5c2"
    color: "#11151b"

    MediaPlayer {
        id: previewPlayer
        audioOutput: AudioOutput { volume: 1.0 }
    }

    FileDialog {
        id: openDialog
        title: "Select a WAV recording"
        fileMode: FileDialog.OpenFile
        nameFilters: ["WAV audio (*.wav)", "All files (*)"]
        onAccepted: { previewPlayer.stop(); audioController.loadAudio(selectedFile) }
    }

    FileDialog {
        id: saveDialog
        title: "Export rendered WAV"
        fileMode: FileDialog.SaveFile
        nameFilters: ["WAV audio (*.wav)"]
        defaultSuffix: "wav"
        onAccepted: audioController.saveAudio(selectedFile)
    }

    Connections {
        target: audioController
        function onPreviewReady() {
            previewPlayer.stop()
            previewPlayer.source = audioController.previewUrl
            previewPlayer.play()
        }
    }

    ScrollView {
        anchors.fill: parent
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: Math.min(window.width - 32, 720)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 16

            Item { Layout.preferredHeight: 2 }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        text: "VoStudio"
                        font.pixelSize: 30
                        font.bold: true
                        color: "#f4f7fb"
                    }
                    Label {
                        text: "WAV vocal processing ﾂｷ Engine preview"
                        color: "#a4afbd"
                        font.pixelSize: 13
                    }
                }
                Label {
                    text: "ALPHA"
                    color: "#8bd5c2"
                    font.bold: true
                    font.pixelSize: 12
                    padding: 9
                    background: Rectangle { color: "#203b37"; radius: 8 }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                color: "#1a222c"
                border.color: "#2b3744"
                implicitHeight: fileColumn.implicitHeight + 30
                ColumnLayout {
                    id: fileColumn
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 10
                    Label { text: "SOURCE AUDIO"; color: "#8bd5c2"; font.bold: true; font.pixelSize: 11 }
                    Label {
                        Layout.fillWidth: true
                        text: audioController.inputFileName
                        color: "#f4f7fb"
                        font.pixelSize: 17
                        font.bold: true
                        elide: Text.ElideMiddle
                    }
                    Label {
                        Layout.fillWidth: true
                        text: audioController.inputInfo
                        color: "#a4afbd"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                    Button {
                        Layout.fillWidth: true
                        enabled: !audioController.busy
                        text: audioController.hasAudio ? "Choose another WAV" : "Choose WAV file"
                        icon.name: "folder-open"
                        onClicked: openDialog.open()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                color: "#1a222c"
                border.color: "#2b3744"
                implicitHeight: controlsColumn.implicitHeight + 30
                ColumnLayout {
                    id: controlsColumn
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 15

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "PROCESSING CONTROLS"; color: "#8bd5c2"; font.bold: true; font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                        ToolButton { text: "Reset"; enabled: !audioController.busy; onClicked: audioController.resetSettings() }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "Pitch shift"; color: "#f4f7fb"; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Label { text: (audioController.pitchSemitones > 0 ? "+" : "") + audioController.pitchSemitones.toFixed(0) + " st"; color: "#8bd5c2"; font.bold: true }
                        }
                        Slider {
                            id: pitchSlider
                            Layout.fillWidth: true
                            from: -12; to: 12; stepSize: 1
                            value: audioController.pitchSemitones
                            enabled: audioController.hasAudio && !audioController.busy
                            onMoved: audioController.pitchSemitones = value
                        }
                        Label { text: "Transpose the recording without intentionally changing its duration."; color: "#8190a0"; font.pixelSize: 11; wrapMode: Text.Wrap }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#2b3744" }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "Duration"; color: "#f4f7fb"; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Label { text: Math.round(audioController.durationPercent) + "%"; color: "#8bd5c2"; font.bold: true }
                        }
                        Slider {
                            Layout.fillWidth: true
                            from: 50; to: 200; stepSize: 1
                            value: audioController.durationPercent
                            enabled: audioController.hasAudio && !audioController.busy
                            onMoved: audioController.durationPercent = value
                        }
                        Label { text: "50% is shorter; 200% is twice the original duration."; color: "#8190a0"; font.pixelSize: 11; wrapMode: Text.Wrap }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#2b3744" }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "Output gain"; color: "#f4f7fb"; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Label { text: Math.round(audioController.gainPercent) + "%"; color: "#8bd5c2"; font.bold: true }
                        }
                        Slider {
                            Layout.fillWidth: true
                            from: 0; to: 150; stepSize: 1
                            value: audioController.gainPercent
                            enabled: audioController.hasAudio && !audioController.busy
                            onMoved: audioController.gainPercent = value
                        }
                        Label { text: "Export is clipped to the WAV PCM range. Leave headroom to avoid distortion."; color: "#8190a0"; font.pixelSize: 11; wrapMode: Text.Wrap }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#2b3744" }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "Formant preservation"; color: "#f4f7fb"; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Label { text: Math.round(audioController.formantPreservationPercent) + "%"; color: "#8bd5c2"; font.bold: true }
                        }
                        Slider {
                            Layout.fillWidth: true
                            from: 0; to: 100; stepSize: 1
                            value: audioController.formantPreservationPercent
                            enabled: audioController.hasAudio && !audioController.busy
                            onMoved: audioController.formantPreservationPercent = value
                        }
                        Label { text: "Controls the engine's broad spectral-envelope compensation after pitch shifting."; color: "#8190a0"; font.pixelSize: 11; wrapMode: Text.Wrap }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#2b3744" }

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Time-stretch algorithm"; color: "#f4f7fb"; font.bold: true }
                        Item { Layout.fillWidth: true }
                        ComboBox {
                            Layout.preferredWidth: 160
                            model: ["WSOLA", "Phase vocoder"]
                            currentIndex: audioController.stretchMode
                            enabled: audioController.hasAudio && !audioController.busy
                            onActivated: audioController.stretchMode = currentIndex
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 16
                color: "#1a222c"
                border.color: "#2b3744"
                implicitHeight: resultColumn.implicitHeight + 30
                ColumnLayout {
                    id: resultColumn
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 10
                    Label { text: "RENDER"; color: "#8bd5c2"; font.bold: true; font.pixelSize: 11 }
                    Button {
                        Layout.fillWidth: true
                        text: audioController.busy ? "Rendering窶ｦ" : "Render audio"
                        highlighted: true
                        enabled: audioController.hasAudio && !audioController.busy
                        onClicked: { previewPlayer.stop(); audioController.render() }
                    }
                    ProgressBar {
                        Layout.fillWidth: true
                        visible: audioController.busy
                        indeterminate: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: audioController.outputInfo
                        color: "#a4afbd"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Button {
                            Layout.fillWidth: true
                            text: "Play preview"
                            enabled: audioController.hasRenderedAudio && !audioController.busy
                            onClicked: { previewPlayer.source = audioController.previewUrl; previewPlayer.play() }
                        }
                        Button {
                            Layout.fillWidth: true
                            text: "Stop"
                            enabled: audioController.hasRenderedAudio
                            onClicked: previewPlayer.stop()
                        }
                    }
                    Button {
                        Layout.fillWidth: true
                        text: "Export WAV"
                        enabled: audioController.hasRenderedAudio && !audioController.busy
                        onClicked: saveDialog.open()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 12
                color: "#15201f"
                border.color: "#29403c"
                implicitHeight: statusLabel.implicitHeight + 24
                Label {
                    id: statusLabel
                    anchors.fill: parent
                    anchors.margins: 12
                    text: audioController.status
                    color: "#b8d8cf"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }

            Label {
                Layout.fillWidth: true
                text: "VoStudio Engine Alpha ﾂｷ WAV in / WAV out ﾂｷ UTAU voicebank import and phonemizers are not included in this preview yet."
                color: "#68788a"
                font.pixelSize: 10
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }
            Item { Layout.preferredHeight: 12 }
        }
    }
}
