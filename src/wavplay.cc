#include "wavplay.h"

#include <QFile>
#include <QDebug>
#include <QFileInfo>

#include <pulse/stream.h>
#include <pulse/scache.h>

#include "pavucontrol.h"

void WavPlay::stateCallback(pa_stream *s, void *userdata)
{
    WavPlay *that = reinterpret_cast<WavPlay*>(userdata);
    assert(s);

    switch (pa_stream_get_state(s)) {
    case PA_STREAM_CREATING:
    case PA_STREAM_TERMINATED:
        break;

    case PA_STREAM_READY:
        const pa_buffer_attr *a;
        char cmt[PA_CHANNEL_MAP_SNPRINT_MAX], sst[PA_SAMPLE_SPEC_SNPRINT_MAX];

        fprintf(stderr, "Stream successfully created.\n");

        if (!(a = pa_stream_get_buffer_attr(s)))
            fprintf(stderr, "pa_stream_get_buffer_attr() failed: %s\n", pa_strerror(pa_context_errno(pa_stream_get_context(s))));
        else {

            fprintf(stderr, "Buffer metrics: maxlength=%u, tlength=%u, prebuf=%u, minreq=%u\n", a->maxlength, a->tlength, a->prebuf, a->minreq);
        }

        fprintf(stderr, "Using sample spec '%s', channel map '%s'.\n",
                pa_sample_spec_snprint(sst, sizeof(sst), pa_stream_get_sample_spec(s)),
                pa_channel_map_snprint(cmt, sizeof(cmt), pa_stream_get_channel_map(s)));

        fprintf(stderr, "Connected to device %s (%u, %ssuspended).\n",
                pa_stream_get_device_name(s),
                pa_stream_get_device_index(s),
                pa_stream_is_suspended(s) ? "" : "not ");

        that->m_uploadComplete = true;
        break;
    case PA_STREAM_FAILED:
        qWarning() << "Stream error" << pa_strerror(pa_context_errno(pa_stream_get_context(s)));
        break;
    default:
        qWarning() << "Unhandled state" << pa_stream_get_state(s);
        break;
    }
}

namespace {
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
}

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
    if (m_data.size() < sizeof(WavHeader)) {
        qWarning() << "Invalid wav header";
    }

    WavHeader *header = reinterpret_cast<WavHeader*>(m_data.data());

    if (header->AudioFormat != WavHeader::PCM) {
        qWarning() << "Can only play PCM, got audio format" << header->AudioFormat;
        qWarning() << header->Format;
        qWarning() << header->NumChannels;
        qWarning() << header->ChunkID;
        return;
    }

//    int audioFormat = 0;

    if (header->BitsPerSample == 8) {
    } else if (header->BitsPerSample == 16) {
    } else if (header->BitsPerSample == 32) {
    } else {
        qWarning() << "Unsupported sample format" << header->BitsPerSample;
        return;
    }
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

    if (m_playingOperation) {
        pa_operation_unref(m_playingOperation);
        m_playingOperation = nullptr;
    }
}

void WavPlay::playSound(const QString &device)
{
    if (!m_uploadComplete) {
        if (!m_uploadStream) {
            uploadSample();
            return;
        }
        qWarning() << "Sample not uploaded yet";
        return;
    }

    if (m_playingOperation) {
        pa_operation_cancel(m_playingOperation);
        pa_operation_unref(m_playingOperation);
        m_playingOperation = nullptr;
    }

    m_playingOperation = pa_context_play_sample(get_context(), m_name.constData(), device.toUtf8().constData(), PA_VOLUME_INVALID, &WavPlay::playCallback, this);
    if (!m_playingOperation) {
        qWarning() << "Failed to play";
    }
    qDebug() << "Playing";

//    if (m_uploadStream) {
//        pa_stream_unref(m_uploadStream);
//        m_uploadStream = nullptr;
//    }

}

void WavPlay::uploadSample()
{
    if (m_uploadComplete) {
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
    if (m_data.size() < sizeof(WavHeader)) {
        qWarning() << "File invalid";
        return;
    }

    pa_sample_spec spec;
    WavHeader *header = reinterpret_cast<WavHeader*>(m_data.data());
    if (header->BitsPerSample == 8) {
        spec.format = PA_SAMPLE_U8;
    } else if (header->BitsPerSample == 16) {
        spec.format = PA_SAMPLE_S16LE;
    } else if (header->BitsPerSample == 32) {
        spec.format = PA_SAMPLE_S32LE;
    } else {
        qWarning() << "Unsupported sample format" << header->BitsPerSample;
        qWarning() << "Implement it if you want it";
        return;
    }
    spec.rate = header->SampleRate;
    spec.channels = header->NumChannels;

    qDebug() << "CReating stream" << m_name;
    m_uploadStream = pa_stream_new(get_context(), m_name.constData(), &spec, nullptr);
    pa_stream_set_state_callback(m_uploadStream, &WavPlay::stateCallback, this);
    pa_stream_set_write_callback(m_uploadStream, &WavPlay::requestCallback, this);

    qDebug() << "Rate:" << spec.rate;
    qDebug() << "Channels:" << spec.channels;
    qDebug() << "Format:" << spec.format;
    qDebug() << "Bytes:" << (m_data.length() - sizeof(WavHeader));

    m_position = sizeof(WavHeader);

    qDebug() << "Stream connect";
    if (pa_stream_connect_upload(m_uploadStream, m_data.length() - sizeof(WavHeader)) != 0) {
        qWarning() << "pa_stream_connect_playback() failed: %s\n" << pa_strerror(pa_context_errno(get_context()));
//        qWarning() << "Invalid blah";
    }
    qDebug() << "Starting upload";
}

void WavPlay::playCallback(pa_context *c, int success, void *userdata)
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
    qDebug() << "Uploading" << length << "bytes";

    const uint8_t *data = reinterpret_cast<const uint8_t *>(that->m_data.constData() + that->m_position);
    if (pa_stream_write(s, data, length, nullptr, 0, PA_SEEK_RELATIVE) < 0) {
        fprintf(stderr, "pa_stream_write() failed: %s\n", pa_strerror(pa_context_errno(get_context())));
        return;
    }

    that->m_position += length;
    if (that->m_position >= that->m_data.length()) {
        qDebug() << "Upload complete";
        pa_stream_finish_upload(s);
    }
}
