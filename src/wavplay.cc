#include "wavplay.h"

#include <QFile>
#include <QDebug>
#include <QFileInfo>

#include <pulse/stream.h>
#include <pulse/scache.h>

#include "pavucontrol.h"

void WavPlay::stateCallback(pa_stream *stream, void *userdata)
{
    WavPlay *that = reinterpret_cast<WavPlay*>(userdata);
    assert(stream);

    switch (pa_stream_get_state(stream)) {
    case PA_STREAM_TERMINATED:
        that->m_uploadComplete = true;
        pa_stream_unref(that->m_uploadStream);
        that->m_uploadStream = nullptr;
        break;

    case PA_STREAM_CREATING:
    case PA_STREAM_READY:
        break;
    case PA_STREAM_FAILED:
        qWarning() << "Stream error" << pa_strerror(pa_context_errno(pa_stream_get_context(stream)));
        break;
    default:
        qWarning() << "Unhandled state" << pa_stream_get_state(stream);
        break;
    }
}

namespace {
#pragma pack(push,1)
struct WavHeader {
    // RIFF header
    uint32_t ChunkID;
    uint32_t ChunkSize;
    uint32_t Format;

    // fmt subchunk
    uint32_t Subchunk1ID;
    uint32_t Subchunk1Size;

    enum AudioFormats {
        PCM = 0x1,
        ADPCM = 0x2,
        IEEEFloat = 0x3,
        ALaw = 0x6,
        MULaw = 0x7,
        DVIADPCM = 0x11,
        AAC = 0xff,
        ExtendedWav = 0xfffe,
        WWISE = 0xffffu,
    };

    uint16_t AudioFormat;
    uint16_t NumChannels;
    uint32_t SampleRate;
    uint32_t ByteRate;
    uint16_t BlockAlign;
    uint16_t BitsPerSample;

    // data subchunk
    uint32_t Subchunk2ID;
    uint32_t Subchunk2Size;
};
#pragma pack(pop)
}//namespace

WavPlay::WavPlay(const QString &filename, QObject *parent) :
    QObject(parent)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open" << filename;
        return;
    }
    m_name = QFileInfo(file).baseName().toUtf8();

    m_data = file.readAll(); // yolo
    if (size_t(m_data.size()) < sizeof(WavHeader)) {
        qWarning() << "Invalid wav header";
        return;
    }

    WavHeader *header = reinterpret_cast<WavHeader*>(m_data.data());
    if (header->AudioFormat != WavHeader::PCM) {
        qWarning() << "Can only play PCM, got audio format" << header->AudioFormat;
        return;
    }
    std::unique_ptr<pa_sample_spec> sampleSpec = std::make_unique<pa_sample_spec>();

    switch(header->BitsPerSample) {
    case 8:
        sampleSpec->format = PA_SAMPLE_U8;
        break;
    case 16:
        sampleSpec->format = PA_SAMPLE_S16LE;
        break;
    case 32:
        sampleSpec->format = PA_SAMPLE_S32LE;
        break;
    default:
        qWarning() << "Unsupported sample format" << header->BitsPerSample;
        qWarning() << "Implement it if you want it";
        return;
    }

    sampleSpec->rate = header->SampleRate;
    sampleSpec->channels = header->NumChannels;

    if(!pa_sample_spec_valid(sampleSpec.get())) {
        qWarning() << "Invalid wav header";
        return;
    }

    m_sampleSpec = std::move(sampleSpec);

    uploadSample();

}

WavPlay::~WavPlay()
{
    if (!get_context()) {
        qWarning() << "Context gone already";
        return;
    }

    if (m_uploadComplete) {
        pa_context_remove_sample(get_context(), m_name.constData(), nullptr, nullptr);
    }
}

void WavPlay::playSound(const QString &device)
{
    if (!m_uploadComplete) {
        if (!m_uploadStream) {
            uploadSample();
            return;
        }

        qWarning() << "Sample not uploaded yet, and no upload started";
        return;
    }


    pa_operation *playingOperation = pa_context_play_sample(get_context(), m_name.constData(), device.toUtf8().constData(), PA_VOLUME_INVALID, &WavPlay::uploadStartedCallback, this);
    if (!playingOperation) {
        qWarning() << "Failed to play";
        return;
    }
    pa_operation_unref(playingOperation);
}

void WavPlay::uploadSample()
{
    if (m_uploadComplete) {
        return;
    }
    if (!m_sampleSpec) {
        return;
    }

    if (!get_context()) {
        qWarning() << "Context not available";
        return;
    }

    if (m_uploadStream) {
        qWarning() << "Already an upload stream available";
        pa_stream_unref(m_uploadStream);
        m_uploadStream = nullptr;
    }

    m_uploadStream = pa_stream_new(get_context(), m_name.constData(), m_sampleSpec.get(), nullptr);
    if (!m_uploadStream) {
        qWarning() << "pa_stream_new() failed: %s\n" << pa_strerror(pa_context_errno(get_context()));
        return;
    }
    pa_stream_set_state_callback(m_uploadStream, &WavPlay::stateCallback, this);
    pa_stream_set_write_callback(m_uploadStream, &WavPlay::requestCallback, this);


    m_position = sizeof(WavHeader);
    if (pa_stream_connect_upload(m_uploadStream, m_data.length() - m_position) != 0) {
        qWarning() << "pa_stream_connect_playback() failed: %s\n" << pa_strerror(pa_context_errno(get_context()));
    }
}

void WavPlay::uploadStartedCallback(pa_context *c, int success, void *userdata)
{
    WavPlay *that = reinterpret_cast<WavPlay*>(userdata);
    if (!success) {
        qWarning() << "Error playing:" << pa_strerror(pa_context_errno(c));
        that->m_uploadComplete = false;
    }
}

void WavPlay::requestCallback(pa_stream *s, size_t maxLength, void *userdata)
{
    WavPlay *that = reinterpret_cast<WavPlay*>(userdata);
    const int length = std::min<int>(maxLength, that->m_data.length() - that->m_position);
    if (length < 1) {
        qWarning() << "Can't give more";
        return;
    }

    const uint8_t *data = reinterpret_cast<const uint8_t *>(that->m_data.constData() + that->m_position);

    if (pa_stream_write(s, data, length, nullptr, 0, PA_SEEK_RELATIVE) < 0) {
        fprintf(stderr, "pa_stream_write() failed: %s\n", pa_strerror(pa_context_errno(get_context())));
        return;
    }

    that->m_position += length;
    if (that->m_position >= size_t(that->m_data.length())) {
        pa_stream_finish_upload(s);
    }
}
