#include "AudioController.hpp"

#include "VoStudio/Engine/Diagnostics/AudioQualityAnalyzer.hpp"
#include "VoStudio/Engine/HybridSynthesisEngine.hpp"
#include "VoStudio/Engine/WavFile.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>

namespace {

std::filesystem::path toFilesystemPath(const QString &path)
{
    const QByteArray utf8 = path.toUtf8();
    return std::filesystem::u8path(utf8.constData());
}

QString fileNameForUrl(const QUrl &url)
{
    // QFile's Android content-URI engine handles SAF document URIs. Desktop
    // and iOS file dialogs generally return file: URLs instead.
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

QString formatDuration(const double seconds)
{
    return QStringLiteral("%1 s").arg(seconds, 0, 'f', 2);
}

} // namespace

AudioController::AudioController(QObject *parent)
    : QObject(parent),
      m_watcher(new QFutureWatcher<RenderResult>(this))
{
    connect(m_watcher, &QFutureWatcher<RenderResult>::finished, this, [this] {
        const RenderResult result = m_watcher->result();
        m_busy = false;
        if (!result.audio) {
            m_rendered.reset();
            m_outputInfo = QStringLiteral("No render available");
            setStatus(result.error.isEmpty() ? QStringLiteral("Rendering failed.") : result.error);
            emit stateChanged();
            return;
        }

        m_rendered = result.audio;
        try {
            updateOutputInfo();
            createPreviewFile();
            setStatus(QStringLiteral("Render complete. Preview it or export the WAV."));
            emit stateChanged();
            emit previewReady();
        } catch (const std::exception &error) {
            setStatus(QStringLiteral("Render finished, but preview setup failed: %1")
                          .arg(QString::fromUtf8(error.what())));
            emit stateChanged();
        }
    });
}

AudioController::~AudioController()
{
    if (m_watcher && m_watcher->isRunning())
        m_watcher->waitForFinished();
}

bool AudioController::hasAudio() const noexcept
{
    return m_source.has_value();
}

bool AudioController::hasRenderedAudio() const noexcept
{
    return static_cast<bool>(m_rendered);
}

void AudioController::setStatus(const QString &message)
{
    m_status = message;
    emit stateChanged();
}

void AudioController::setPitchSemitones(const double value)
{
    const double bounded = std::clamp(value, -12.0, 12.0);
    if (std::abs(bounded - m_pitchSemitones) < 1.0e-9) return;
    m_pitchSemitones = bounded;
    emit settingsChanged();
}

void AudioController::setDurationPercent(const double value)
{
    const double bounded = std::clamp(value, 50.0, 200.0);
    if (std::abs(bounded - m_durationPercent) < 1.0e-9) return;
    m_durationPercent = bounded;
    emit settingsChanged();
}

void AudioController::setGainPercent(const double value)
{
    const double bounded = std::clamp(value, 0.0, 150.0);
    if (std::abs(bounded - m_gainPercent) < 1.0e-9) return;
    m_gainPercent = bounded;
    emit settingsChanged();
}

void AudioController::setFormantPreservationPercent(const double value)
{
    const double bounded = std::clamp(value, 0.0, 100.0);
    if (std::abs(bounded - m_formantPreservationPercent) < 1.0e-9) return;
    m_formantPreservationPercent = bounded;
    emit settingsChanged();
}

void AudioController::setStretchMode(const int value)
{
    const int bounded = std::clamp(value, 0, 1);
    if (bounded == m_stretchMode) return;
    m_stretchMode = bounded;
    emit settingsChanged();
}

void AudioController::loadAudio(const QUrl &url)
{
    if (m_busy) return;
    if (url.isEmpty()) {
        setStatus(QStringLiteral("No file was selected."));
        return;
    }

    try {
        const QString selectedName = fileNameForUrl(url);
        VoStudio::Engine::AudioBuffer loaded = [&]() {
            if (url.isLocalFile())
                return VoStudio::Engine::WavFile::read(toFilesystemPath(url.toLocalFile()));

            QFile input(selectedName);
            if (!input.open(QIODevice::ReadOnly))
                throw std::runtime_error(QStringLiteral("Cannot open selected WAV: %1")
                                             .arg(input.errorString()).toStdString());

            QTemporaryFile staged(QDir::tempPath() + QStringLiteral("/vostudio-input-XXXXXX.wav"));
            if (!staged.open())
                throw std::runtime_error("Cannot create a temporary file for the selected audio");

            QByteArray chunk(64 * 1024, Qt::Uninitialized);
            while (!input.atEnd()) {
                const qint64 count = input.read(chunk.data(), chunk.size());
                if (count < 0)
                    throw std::runtime_error(input.errorString().toStdString());
                if (count == 0) break;
                if (staged.write(chunk.constData(), count) != count)
                    throw std::runtime_error("Failed to stage the selected WAV file");
            }
            if (!staged.flush())
                throw std::runtime_error("Failed to flush the staged WAV file");
            const QString stagedPath = staged.fileName();
            staged.close();
            return VoStudio::Engine::WavFile::read(toFilesystemPath(stagedPath));
        }();

        m_source = std::move(loaded);
        m_rendered.reset();
        m_previewFile.reset();
        m_previewUrl.clear();
        m_inputFileName = QFileInfo(url.path()).fileName();
        if (m_inputFileName.isEmpty()) m_inputFileName = QFileInfo(selectedName).fileName();
        if (m_inputFileName.isEmpty()) m_inputFileName = QStringLiteral("Selected WAV");
        m_outputInfo = QStringLiteral("No render yet");
        updateInputInfo();
        setStatus(QStringLiteral("Loaded %1. Adjust the controls and render.").arg(m_inputFileName));
    } catch (const std::exception &error) {
        setStatus(QStringLiteral("Could not load WAV: %1").arg(QString::fromUtf8(error.what())));
    }
}

void AudioController::render()
{
    if (!m_source || m_busy) return;

    const auto source = *m_source;
    VoStudio::Engine::RenderParameters parameters;
    parameters.source_root_midi_note = 60;
    parameters.target_midi_note = 60 + static_cast<int>(std::lround(m_pitchSemitones));
    parameters.output_duration_seconds = source.duration_seconds() * m_durationPercent / 100.0;
    parameters.gain = static_cast<float>(m_gainPercent / 100.0);
    parameters.formant_preservation = m_formantPreservationPercent / 100.0;
    parameters.time_stretch.algorithm = m_stretchMode == 1
        ? VoStudio::Engine::TimeStretchAlgorithm::PhaseVocoder
        : VoStudio::Engine::TimeStretchAlgorithm::Wsola;

    m_busy = true;
    m_rendered.reset();
    m_previewFile.reset();
    m_previewUrl.clear();
    m_outputInfo = QStringLiteral("Rendering…");
    setStatus(QStringLiteral("Rendering audio. Larger WAV files may take a little longer…"));

    m_watcher->setFuture(QtConcurrent::run([source, parameters]() -> RenderResult {
        try {
            auto output = std::make_shared<VoStudio::Engine::AudioBuffer>(
                VoStudio::Engine::HybridSynthesisEngine::render(source, parameters));
            return {std::move(output), {}};
        } catch (const std::exception &error) {
            return {{}, QString::fromUtf8(error.what())};
        } catch (...) {
            return {{}, QStringLiteral("Unknown rendering error")};
        }
    }));
    emit stateChanged();
}

void AudioController::saveAudio(const QUrl &url)
{
    if (!m_rendered || url.isEmpty() || m_busy) return;

    try {
        const QString destinationName = fileNameForUrl(url);
        if (url.isLocalFile()) {
            VoStudio::Engine::WavFile::write(toFilesystemPath(destinationName), *m_rendered, 24);
        } else {
            QTemporaryFile staged(QDir::tempPath() + QStringLiteral("/vostudio-export-XXXXXX.wav"));
            if (!staged.open()) throw std::runtime_error("Cannot create a temporary WAV for export");
            const QString stagedPath = staged.fileName();
            staged.close();
            VoStudio::Engine::WavFile::write(toFilesystemPath(stagedPath), *m_rendered, 24);

            QFile source(stagedPath);
            QFile destination(destinationName);
            if (!source.open(QIODevice::ReadOnly))
                throw std::runtime_error("Cannot read the rendered temporary WAV");
            if (!destination.open(QIODevice::WriteOnly | QIODevice::Truncate))
                throw std::runtime_error(QStringLiteral("Cannot create the selected output: %1")
                                             .arg(destination.errorString()).toStdString());
            QByteArray chunk(64 * 1024, Qt::Uninitialized);
            while (!source.atEnd()) {
                const qint64 count = source.read(chunk.data(), chunk.size());
                if (count < 0) throw std::runtime_error("Failed while reading rendered WAV");
                if (count == 0) break;
                if (destination.write(chunk.constData(), count) != count)
                    throw std::runtime_error("Failed while writing to the selected output");
            }
            if (!destination.flush()) throw std::runtime_error("Failed to finish writing the output WAV");
        }
        setStatus(QStringLiteral("Exported WAV successfully."));
    } catch (const std::exception &error) {
        setStatus(QStringLiteral("Could not export WAV: %1").arg(QString::fromUtf8(error.what())));
    }
}

void AudioController::resetSettings()
{
    m_pitchSemitones = 0.0;
    m_durationPercent = 100.0;
    m_gainPercent = 100.0;
    m_formantPreservationPercent = 65.0;
    m_stretchMode = 0;
    emit settingsChanged();
}

void AudioController::updateInputInfo()
{
    if (!m_source) return;
    const auto metrics = VoStudio::Engine::AudioQualityAnalyzer::analyze(*m_source);
    m_inputInfo = QStringLiteral("%1 Hz  •  %2 ch  •  %3  •  peak %4 dBFS")
        .arg(m_source->sample_rate())
        .arg(m_source->channel_count())
        .arg(formatDuration(m_source->duration_seconds()))
        .arg(metrics.peak > 0.0 ? 20.0 * std::log10(metrics.peak) : -120.0, 0, 'f', 1);
}

void AudioController::updateOutputInfo()
{
    if (!m_rendered) return;
    const auto metrics = VoStudio::Engine::AudioQualityAnalyzer::analyze(*m_rendered);
    const double peakDb = metrics.peak > 0.0 ? 20.0 * std::log10(metrics.peak) : -120.0;
    m_outputInfo = QStringLiteral("%1 Hz  •  %2 ch  •  %3  •  peak %4 dBFS  •  %5% clipped samples")
        .arg(m_rendered->sample_rate())
        .arg(m_rendered->channel_count())
        .arg(formatDuration(m_rendered->duration_seconds()))
        .arg(peakDb, 0, 'f', 1)
        .arg(metrics.clipped_sample_fraction * 100.0, 0, 'f', 2);
}

void AudioController::createPreviewFile()
{
    if (!m_rendered) return;
    auto preview = std::make_unique<QTemporaryFile>(
        QDir::tempPath() + QStringLiteral("/vostudio-preview-XXXXXX.wav"));
    if (!preview->open()) throw std::runtime_error("Cannot create a preview WAV file");
    const QString path = preview->fileName();
    preview->close();
    VoStudio::Engine::WavFile::write(toFilesystemPath(path), *m_rendered, 16);
    m_previewUrl = QUrl::fromLocalFile(path).toString();
    m_previewFile = std::move(preview);
}
