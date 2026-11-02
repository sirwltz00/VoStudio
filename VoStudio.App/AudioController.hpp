#pragma once

#include "VoStudio/Engine/AudioBuffer.hpp"

#include <QFutureWatcher>
#include <QObject>
#include <QUrl>

#include <memory>
#include <optional>

class QTemporaryFile;

class AudioController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasAudio READ hasAudio NOTIFY stateChanged)
    Q_PROPERTY(bool hasRenderedAudio READ hasRenderedAudio NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString inputFileName READ inputFileName NOTIFY stateChanged)
    Q_PROPERTY(QString inputInfo READ inputInfo NOTIFY stateChanged)
    Q_PROPERTY(QString outputInfo READ outputInfo NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString previewUrl READ previewUrl NOTIFY stateChanged)
    Q_PROPERTY(double pitchSemitones READ pitchSemitones WRITE setPitchSemitones NOTIFY settingsChanged)
    Q_PROPERTY(double durationPercent READ durationPercent WRITE setDurationPercent NOTIFY settingsChanged)
    Q_PROPERTY(double gainPercent READ gainPercent WRITE setGainPercent NOTIFY settingsChanged)
    Q_PROPERTY(double formantPreservationPercent READ formantPreservationPercent WRITE setFormantPreservationPercent NOTIFY settingsChanged)
    Q_PROPERTY(int stretchMode READ stretchMode WRITE setStretchMode NOTIFY settingsChanged)

public:
    explicit AudioController(QObject *parent = nullptr);
    ~AudioController() override;

    bool hasAudio() const noexcept;
    bool hasRenderedAudio() const noexcept;
    bool busy() const noexcept { return m_busy; }
    QString inputFileName() const { return m_inputFileName; }
    QString inputInfo() const { return m_inputInfo; }
    QString outputInfo() const { return m_outputInfo; }
    QString status() const { return m_status; }
    QString previewUrl() const { return m_previewUrl; }

    double pitchSemitones() const noexcept { return m_pitchSemitones; }
    double durationPercent() const noexcept { return m_durationPercent; }
    double gainPercent() const noexcept { return m_gainPercent; }
    double formantPreservationPercent() const noexcept { return m_formantPreservationPercent; }
    int stretchMode() const noexcept { return m_stretchMode; }

    void setPitchSemitones(double value);
    void setDurationPercent(double value);
    void setGainPercent(double value);
    void setFormantPreservationPercent(double value);
    void setStretchMode(int value);

    Q_INVOKABLE void loadAudio(const QUrl &url);
    Q_INVOKABLE void render();
    Q_INVOKABLE void saveAudio(const QUrl &url);
    Q_INVOKABLE void resetSettings();

signals:
    void stateChanged();
    void settingsChanged();
    void previewReady();

private:
    struct RenderResult {
        std::shared_ptr<VoStudio::Engine::AudioBuffer> audio;
        QString error;
    };

    void setStatus(const QString &message);
    void updateInputInfo();
    void updateOutputInfo();
    void createPreviewFile();

    std::optional<VoStudio::Engine::AudioBuffer> m_source;
    std::shared_ptr<VoStudio::Engine::AudioBuffer> m_rendered;
    std::unique_ptr<QTemporaryFile> m_previewFile;
    QFutureWatcher<RenderResult> *m_watcher{};

    bool m_busy{false};
    QString m_inputFileName{QStringLiteral("No WAV selected")};
    QString m_inputInfo{QStringLiteral("Choose a WAV recording to begin.")};
    QString m_outputInfo{QStringLiteral("No render yet")};
    QString m_status{QStringLiteral("Ready. Import a WAV recording.")};
    QString m_previewUrl;

    double m_pitchSemitones{0.0};
    double m_durationPercent{100.0};
    double m_gainPercent{100.0};
    double m_formantPreservationPercent{65.0};
    int m_stretchMode{0}; // 0 = WSOLA, 1 = phase vocoder
};
