#pragma once

#include <QByteArray>
#include <QObject>

#include <memory>

struct pa_context;
struct pa_stream;
struct pa_operation;
struct pa_sample_spec;

class QString;

class WavPlay : public QObject
{
    Q_OBJECT

public:
    WavPlay(const QString &filename, QObject *parent = nullptr);
    ~WavPlay();

public slots:
    void playSound(const QString &device);

private:
    void uploadSample();

    static void uploadStartedCallback(pa_context *c, int success, void *userdata);
    static void stateCallback(pa_stream *s, void *userdata);
    static void requestCallback(pa_stream *s, size_t length, void *userdata);
    static void underflowCallback(pa_stream *s, void *userdata);

    size_t m_position = 0;
    QByteArray m_data;

    bool m_uploadComplete = false;

    pa_stream *m_uploadStream = nullptr;

    QByteArray m_name = "none";
    pa_operation *m_playingOperation = nullptr;
    std::unique_ptr<pa_sample_spec> m_sampleSpec;
};

