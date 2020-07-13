/***
  This file is part of pavucontrol.

  Copyright 2006-2008 Lennart Poettering
  Copyright 2009 Colin Guthrie

  pavucontrol is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  pavucontrol is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with pavucontrol. If not, see <https://www.gnu.org/licenses/>.
***/

#include <set>

#include "mainwindow.h"
#include "cardwidget.h"
#include "sinkwidget.h"
#include "sourcewidget.h"
#include "playbackwidget.h"
#include "sourceoutputwidget.h"
#include "rolewidget.h"
#include "wavplay.h"
#include "utils.h"

#include <QIcon>
#include <QStyle>
#include <QSettings>
#include <QScrollArea>
#include <QDebug>
#include <QFormLayout>
#include <QLabel>
#include <QTabWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QToolButton>
#include <QMessageBox>

QWidget *createTab(QWidget *contentList, QLabel *defaultLabel, QWidget *typeSelect)
{
    contentList->setLayout(new QVBoxLayout);

    QWidget *tab = new QWidget;
    QVBoxLayout *tabLayout = new QVBoxLayout(tab);//QFormLayout(tab);

    QScrollArea *scrollArea = new QScrollArea;
    tabLayout->addWidget(scrollArea);
    scrollArea->setLayout(new QVBoxLayout);
    scrollArea->setWidgetResizable(true);

    QHBoxLayout *typeLayout = new QHBoxLayout;
    typeLayout->addWidget(new QLabel(QObject::tr("Show:")));
    typeLayout->addWidget(typeSelect);
    tabLayout->addLayout(typeLayout);

    contentList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    contentList->layout()->addWidget(defaultLabel);
    scrollArea->layout()->addWidget(contentList);
    scrollArea->setWidget(contentList);

    return tab;
}

MainWindow::MainWindow(QWidget *parent):
    QWidget(parent),
    m_showPlaybackType(SINK_INPUT_CLIENT),
    m_showSinkType(SINK_ALL),
    m_showSourceOutputType(SOURCE_OUTPUT_CLIENT),
    m_showSourceType(SOURCE_NO_MONITOR),
    m_eventRoleWidget(nullptr),
    m_canRenameDevices(false),
    m_connected(false),
    m_config_filename(nullptr)
{
    m_popPlayer = new WavPlay(":/data/bop.wav", this);

    setLayout(new QVBoxLayout);

    ////////////////////
    // Create widgets
    m_notebook = new QTabWidget;

    m_playbackTypeComboBox = new QComboBox;
    m_playbackTypeComboBox->addItems( {
        tr("All Streams"),
        tr("Applications"),
        tr("Virtual Streams"),
    });
    m_sourceOutputTypeComboBox = new QComboBox;
    m_sourceOutputTypeComboBox->addItems( {
        tr("All Streams"),
        tr("Applications"),
        tr("Virtual Streams"),
    });
    m_sinkTypeComboBox = new QComboBox;
    m_sinkTypeComboBox->addItems( {
        tr("All Output Devices"),
        tr("Hardware Output Devices"),
        tr("Virtual Output Devices"),
    });
    m_sourceTypeComboBox = new QComboBox;
    m_sourceTypeComboBox->addItems( {
        tr("All Input Devices"),
        tr("All Except Monitors"),
        tr("Hardware Input Devices"),
        tr("Virtual Input Devices"),
        tr("Monitors"),
    });

    m_showVolumeMetersCheckButton = new QCheckBox(tr("Show volume meters"));


    m_connectingLabel = new QLabel;
    m_noStreamsLabel = new QLabel("<i>No application is currently playing audio.</i>");
    m_noRecsLabel = new QLabel(tr("<i>No application is currently recording audio.</i>"));
    m_noSinksLabel = new QLabel(tr("<i>No output devices available</i>"));
    m_noSourcesLabel = new QLabel(tr("<i>No input devices available</i>"));
    m_noCardsLabel = new QLabel(tr("<i>No cards available for configuration</i>"));

    m_sinksVBox = new QWidget;
    m_sourcesVBox = new QWidget;
    m_streamsVBox = new QWidget;
    m_recsVBox = new QWidget;
    m_cardsVBox = new QWidget;

    ///////////////
    // Do layout
    // Tabs
    m_notebook->addTab(createTab(m_streamsVBox, m_noStreamsLabel, m_playbackTypeComboBox), tr("&Playback"));
    m_notebook->addTab(createTab(m_recsVBox, m_noRecsLabel, m_sourceOutputTypeComboBox), tr("&Recording"));
    m_notebook->addTab(createTab(m_sinksVBox, m_noSinksLabel, m_sinkTypeComboBox), tr("&Output Devices"));
    m_notebook->addTab(createTab(m_sourcesVBox, m_noSourcesLabel, m_sourceTypeComboBox), tr("&Input Devices"));
    m_notebook->addTab(createTab(m_cardsVBox, m_noCardsLabel, m_showVolumeMetersCheckButton), tr("&Configuration"));

    layout()->addWidget(m_notebook);
    layout()->addWidget(m_connectingLabel);

    m_playbackTypeComboBox->setCurrentIndex((int) m_showPlaybackType);
    m_sourceOutputTypeComboBox->setCurrentIndex((int) m_showSourceOutputType);
    m_sinkTypeComboBox->setCurrentIndex((int) m_showSinkType);
    m_sourceTypeComboBox->setCurrentIndex((int) m_showSourceType);


    connect(m_playbackTypeComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &MainWindow::onPlaybackTypeComboBoxChanged);
    connect(m_sourceOutputTypeComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &MainWindow::onSourceOutputTypeComboBoxChanged);
    connect(m_sinkTypeComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &MainWindow::onSinkTypeComboBoxChanged);
    connect(m_sourceTypeComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &MainWindow::onSourceTypeComboBoxChanged);
    connect(m_showVolumeMetersCheckButton, &QCheckBox::toggled, this, &MainWindow::onShowVolumeMetersCheckButtonToggled);

    QAction *quit = new QAction{this};
    connect(quit, &QAction::triggered, this, &QWidget::close);
    quit->setShortcut(QKeySequence::Quit);
    addAction(quit);

    const QSettings config;

    m_showVolumeMetersCheckButton->setChecked(config.value(QStringLiteral("window/showVolumeMeters"), true).toBool());

    const QSize last_size  = config.value(QStringLiteral("window/size")).toSize();

    if (last_size.isValid()) {
        resize(last_size);
    }

    const QVariant playbackTypeSelection = config.value(QStringLiteral("window/sinkInputType"));

    if (playbackTypeSelection.isValid()) {
        m_playbackTypeComboBox->setCurrentIndex(playbackTypeSelection.toInt());
    }

    const QVariant sourceOutputTypeSelection = config.value(QStringLiteral("window/sourceOutputType"));

    if (sourceOutputTypeSelection.isValid()) {
        m_sourceOutputTypeComboBox->setCurrentIndex(sourceOutputTypeSelection.toInt());
    }

    const QVariant sinkTypeSelection = config.value(QStringLiteral("window/sinkType"));

    if (sinkTypeSelection.isValid()) {
        m_sinkTypeComboBox->setCurrentIndex(sinkTypeSelection.toInt());
    }

    const QVariant sourceTypeSelection = config.value(QStringLiteral("window/sourceType"));

    if (sourceTypeSelection.isValid()) {
        m_sourceTypeComboBox->setCurrentIndex(sourceTypeSelection.toInt());
    }

    /* Hide first and show when we're connected */
    m_notebook->hide();
    m_connectingLabel->show();
}

MainWindow::~MainWindow()
{
    QSettings config;
    config.setValue(QStringLiteral("window/size"), size());
    config.setValue(QStringLiteral("window/sinkInputType"), m_playbackTypeComboBox->currentIndex());
    config.setValue(QStringLiteral("window/sourceOutputType"), m_sourceOutputTypeComboBox->currentIndex());
    config.setValue(QStringLiteral("window/sinkType"), m_sinkTypeComboBox->currentIndex());
    config.setValue(QStringLiteral("window/sourceType"), m_sourceTypeComboBox->currentIndex());
    config.setValue(QStringLiteral("window/showVolumeMeters"), m_showVolumeMetersCheckButton->isChecked());

    m_clientNames.clear();
}

class DeviceWidget;
static void updatePorts(DeviceWidget *w, std::map<QByteArray, PortInfo> *ports)
{

    for (std::pair<QByteArray, QByteArray> &port : w->ports) {
        std::map<QByteArray, PortInfo>::iterator it = ports->find(port.first);
        if (it == ports->end()) {
            continue;
        }

        PortInfo portInfo = it->second;

        QString availability;
        if (portInfo.available == PA_PORT_AVAILABLE_YES) {
            availability = MainWindow::tr("plugged in");
        } else if (portInfo.available == PA_PORT_AVAILABLE_NO) {
            if (portInfo.name == "analog-output-speaker" ||
                    portInfo.name == "analog-input-microphone-internal") {
                availability = MainWindow::tr("unavailable");
            } else {
                availability = MainWindow::tr("unplugged");
            }
        }

        QString desc = QStringLiteral("%1 (%2)").arg(portInfo.description, availability);
        port.second = desc.toUtf8();
    }

    std::map<QByteArray, PortInfo>::iterator it = ports->find(w->activePort);

    if (it != ports->end()) {
        w->setLatencyOffset(it->second.latency_offset);
    }
}

void MainWindow::setIconByName(QLabel *label, const QByteArray &name, const QByteArray &fallback)
{
    QIcon icon = QIcon::fromTheme(QString::fromUtf8(!name.isEmpty() ? name : fallback));

    if (icon.isNull()) {
        qWarning() << "Unable to find icon" << name << "using fallback" << fallback;

        icon = QIcon::fromTheme(fallback);

        if (icon.isNull()) {
            qWarning() << "Failed to load fallback icon" << fallback;
            return;
        }
    }

    const int size = label->style()->pixelMetric(QStyle::PM_ToolBarIconSize);
    label->setPixmap(icon.pixmap(size, size));
}

void MainWindow::updateCard(const pa_card_info &info)
{
    bool is_new = false;

    CardWidget *cardWidget = nullptr;
    if (m_cardWidgets.count(info.index)) {
        cardWidget = m_cardWidgets[info.index];
    } else {
        m_cardWidgets[info.index] = cardWidget = new CardWidget(this);
        m_cardsVBox->layout()->addWidget(cardWidget);
        cardWidget->index = info.index;
        is_new = true;
    }

    cardWidget->updating = true;

    const QString name = QString::fromUtf8(info.name);
    const QString description = utils::readProperty(info, PA_PROP_DEVICE_DESCRIPTION);
    if (!description.isEmpty()) {
        cardWidget->name = description;
    } else {
        cardWidget->name = name;
    }
    cardWidget->nameLabel->setText(cardWidget->name);

    cardWidget->iconImage->setPixmap(utils::deviceIcon(info).pixmap(iconSize()));

    cardWidget->hasSinks = cardWidget->hasSources = false;

    QVector<pa_card_profile_info2 *> profiles;
    for (uint32_t i=0; i<info.n_profiles; ++i) {
        cardWidget->hasSinks = cardWidget->hasSinks || (info.profiles2[i]->n_sinks > 0);
        cardWidget->hasSources = cardWidget->hasSources || (info.profiles2[i]->n_sources > 0);
        profiles.append(info.profiles2[i]);
    }

    cardWidget->ports.clear();

    for (uint32_t i = 0; i < info.n_ports; ++i) {
        PortInfo p;

        p.name = info.ports[i]->name;
        p.description = info.ports[i]->description;
        p.priority = info.ports[i]->priority;
        p.available = info.ports[i]->available;
        p.direction = info.ports[i]->direction;
        p.latency_offset = info.ports[i]->latency_offset;

        for (uint32_t j = 0; j < info.ports[i]->n_profiles; j++) {
            p.profiles.push_back(info.ports[i]->profiles2[j]->name);
        }

        cardWidget->ports[p.name] = p;
    }

    cardWidget->profiles.clear();

    std::sort(profiles.begin(), profiles.end(), [](const pa_card_profile_info2 *lhs, const pa_card_profile_info2 *rhs) {
        if (lhs->priority == rhs->priority) {
            return strcmp(lhs->name, rhs->name) > 0;
        }

        return lhs->priority > rhs->priority;
    });

    for (pa_card_profile_info2 *p_profile : profiles) {
        bool hasNo = false, hasOther = false;
        std::map<QByteArray, PortInfo>::iterator portIt;
        QByteArray desc = p_profile->description;

        for (portIt = cardWidget->ports.begin(); portIt != cardWidget->ports.end(); portIt++) {
            PortInfo port = portIt->second;

            if (std::find(port.profiles.begin(), port.profiles.end(), p_profile->name) == port.profiles.end()) {
                continue;
            }

            if (port.available == PA_PORT_AVAILABLE_NO) {
                hasNo = true;
            } else {
                hasOther = true;
                break;
            }
        }

        if (hasNo && !hasOther) {
            desc += tr(" (unplugged)").toUtf8().constData();
        }

        if (!p_profile->available) {
            desc += tr(" (unavailable)").toUtf8().constData();
        }

        cardWidget->profiles.push_back(std::pair<QByteArray, QByteArray>(p_profile->name, desc));

        if (p_profile->n_sinks == 0 && p_profile->n_sources == 0) {
            cardWidget->noInOutProfile = p_profile->name;
        }
    }

    cardWidget->activeProfile = info.active_profile ? info.active_profile->name : "";

    /* Because the port info for sinks and sources is discontinued we need
     * to update the port info for them here. */
    if (cardWidget->hasSinks) {
        std::map<uint32_t, SinkWidget *>::iterator it;

        for (it = m_sinkWidgets.begin() ; it != m_sinkWidgets.end(); it++) {
            SinkWidget *sw = it->second;

            if (sw->card_index == cardWidget->index) {
                sw->updating = true;
                updatePorts(sw, &cardWidget->ports);
                sw->updating = false;
            }
        }
    }

    if (cardWidget->hasSources) {
        std::map<uint32_t, SourceWidget *>::iterator it;

        for (it = m_sourceWidgets.begin() ; it != m_sourceWidgets.end(); it++) {
            SourceWidget *sw = it->second;

            if (sw->card_index == cardWidget->index) {
                sw->updating = true;
                updatePorts(sw, &cardWidget->ports);
                sw->updating = false;
            }
        }
    }

    cardWidget->prepareMenu();

    if (is_new) {
        updateDeviceVisibility();
    }

    cardWidget->updating = false;
}

bool MainWindow::updateSink(const pa_sink_info &info)
{
    bool isNew = false;
    SinkWidget *sinkWidget = nullptr;
    if (m_sinkWidgets.count(info.index)) {
        sinkWidget = m_sinkWidgets[info.index];
    } else {
        m_sinkWidgets[info.index] = sinkWidget = new SinkWidget(this);
        connect(sinkWidget, &SinkWidget::requestBop, m_popPlayer, &WavPlay::playSound);
        sinkWidget->setChannelMap(info.channel_map, !!(info.flags & PA_SINK_DECIBEL_VOLUME));
        m_sinksVBox->layout()->addWidget(sinkWidget);
        sinkWidget->index = info.index;
        sinkWidget->monitor_index = info.monitor_source;
        isNew = true;

        sinkWidget->setBaseVolume(info.base_volume);
        sinkWidget->setVolumeMeterVisible(m_showVolumeMetersCheckButton->isChecked());
    }

    sinkWidget->updating = true;

    sinkWidget->card_index = info.card;
    sinkWidget->name = info.name;
    sinkWidget->description = info.description;
    sinkWidget->type = info.flags & PA_SINK_HARDWARE ? SINK_HARDWARE : SINK_VIRTUAL;

    sinkWidget->boldNameLabel->setText(QLatin1String(""));
    sinkWidget->nameLabel->setText(QString::asprintf("%s", info.description).toHtmlEscaped());
    sinkWidget->nameLabel->setToolTip(QString::fromUtf8(info.description));

    sinkWidget->iconImage->setPixmap(utils::deviceIcon(info).pixmap(iconSize()));

    sinkWidget->setVolume(info.volume);
    sinkWidget->muteToggleButton->setChecked(info.mute);

    sinkWidget->setDefault(sinkWidget->name == m_defaultSinkName);

    QVector<pa_sink_port_info> ports;
    for (uint32_t i = 0; i < info.n_ports; ++i) {
        ports.append(*info.ports[i]);
    }
    std::sort(ports.begin(), ports.end(), [](const pa_sink_port_info &lhs, const pa_sink_port_info &rhs) {
        if (lhs.priority == rhs.priority) {
            return strcmp(lhs.name, rhs.name) > 0;
        }
        return lhs.priority > rhs.priority;
    });

    sinkWidget->ports.clear();
    for (const pa_sink_port_info &port_priority : ports) {
        sinkWidget->ports.push_back(std::pair<QByteArray, QByteArray>(port_priority.name, port_priority.description));
    }

    sinkWidget->activePort = info.active_port ? info.active_port->name : "";

    std::map<uint32_t, CardWidget *>::iterator cw = m_cardWidgets.find(info.card);
    if (cw != m_cardWidgets.end()) {
        updatePorts(sinkWidget, &cw->second->ports);
    }

#ifdef PA_SINK_SET_FORMATS
    sinkWidget->setDigital(info.flags & PA_SINK_SET_FORMATS);
#endif

    sinkWidget->prepareMenu();

    sinkWidget->updating = false;

    if (isNew) {
        updateDeviceVisibility();
    }

    return isNew;
}

static void suspended_callback(pa_stream *s, void *userdata)
{
    MainWindow *w = static_cast<MainWindow *>(userdata);

    if (pa_stream_is_suspended(s)) {
        w->updateVolumeMeter(pa_stream_get_device_index(s), PA_INVALID_INDEX, -1);
    }
}

static void read_callback(pa_stream *stream, size_t length, void *userdata)
{
    MainWindow *mainWindow = static_cast<MainWindow *>(userdata);

    const void *data;
    if (pa_stream_peek(stream, &data, &length) < 0) {
        show_error(MainWindow::tr("Failed to read data from stream").toUtf8().constData());
        return;
    }

    if (!data) {
        /* nullptr data means either a hole or empty buffer.
         * Only drop the stream when there is a hole (length > 0) */
        if (length) {
            pa_stream_drop(stream);
        }

        return;
    }

    assert(length > 0);
    assert(length % sizeof(float) == 0);

    const double value = reinterpret_cast<const float *>(data)[length / sizeof(float) -1];

    pa_stream_drop(stream);

    mainWindow->updateVolumeMeter(pa_stream_get_device_index(stream), pa_stream_get_monitor_stream(stream), qBound(0., value, 1.));
}

pa_stream *MainWindow::createMonitorStreamForSource(uint32_t source_idx, uint32_t stream_idx = -1, bool suspend = false)
{
    pa_sample_spec sampleSpec;
    sampleSpec.channels = 1;
    sampleSpec.format = PA_SAMPLE_FLOAT32;
    sampleSpec.rate = 25;

    pa_buffer_attr attributes{};
    attributes.fragsize = sizeof(float);
    attributes.maxlength = (uint32_t) -1;

    const QByteArray streamName = tr("Peak detect").toUtf8();

    pa_stream *stream = pa_stream_new(get_context(), streamName.constData(), &sampleSpec, nullptr);
    if (!stream) {
        QMessageBox::warning(this, tr("Error creating monitor"), tr("Failed to create monitoring stream"));
        return nullptr;
    }

    if (stream_idx != (uint32_t) -1) {
        pa_stream_set_monitor_stream(stream, stream_idx);
    }

    pa_stream_set_read_callback(stream, read_callback, this);
    pa_stream_set_suspended_callback(stream, suspended_callback, this);

    pa_stream_flags_t flags =
            pa_stream_flags_t(PA_STREAM_DONT_MOVE | PA_STREAM_PEAK_DETECT | PA_STREAM_ADJUST_LATENCY |
                              (suspend ? PA_STREAM_DONT_INHIBIT_AUTO_SUSPEND : PA_STREAM_NOFLAGS) |
                              (!m_showVolumeMetersCheckButton->isChecked() ? PA_STREAM_START_CORKED : PA_STREAM_NOFLAGS));

    const QByteArray sourceDevice = QByteArray::number(source_idx);
    if (pa_stream_connect_record(stream, sourceDevice.constData(), &attributes, flags) < 0) {
        QMessageBox::warning(this, tr("Error creating monitor"), tr("Failed to connect monitoring stream"));
        pa_stream_unref(stream);
        return nullptr;
    }

    return stream;
}

void MainWindow::createMonitorStreamForPlayback(PlaybackWidget *playbackWidget, uint32_t sink_idx)
{
    if (!m_sinkWidgets.count(sink_idx)) {
        qWarning() << "Unknown sink index" << sink_idx;
        return;
    }

    if (playbackWidget->peak) {
        pa_stream_disconnect(playbackWidget->peak);
        playbackWidget->peak = nullptr;
    }

    playbackWidget->peak = createMonitorStreamForSource(m_sinkWidgets[sink_idx]->monitor_index, playbackWidget->index);
}

void MainWindow::updateSource(const pa_source_info &info)
{
    bool isNew = false;
    SourceWidget *sourceWidget = nullptr;
    if (m_sourceWidgets.count(info.index)) {
        sourceWidget = m_sourceWidgets[info.index];
    } else {
        m_sourceWidgets[info.index] = sourceWidget = new SourceWidget(this);
        connect(sourceWidget, &SourceWidget::requestBop, m_popPlayer, &WavPlay::playSound);

        sourceWidget->setChannelMap(info.channel_map, !!(info.flags & PA_SOURCE_DECIBEL_VOLUME));
        m_sourcesVBox->layout()->addWidget(sourceWidget);

        sourceWidget->index = info.index;
        isNew = true;

        sourceWidget->setBaseVolume(info.base_volume);
        sourceWidget->setVolumeMeterVisible(m_showVolumeMetersCheckButton->isChecked());

        if (pa_context_get_server_protocol_version(get_context()) >= 13) {
            sourceWidget->peak = createMonitorStreamForSource(info.index, -1, !!(info.flags & PA_SOURCE_NETWORK));
        }
    }

    sourceWidget->updating = true;

    sourceWidget->card_index = info.card;
    sourceWidget->name = info.name;
    sourceWidget->description = info.description;
    sourceWidget->type = info.monitor_of_sink != PA_INVALID_INDEX ? SOURCE_MONITOR : (info.flags & PA_SOURCE_HARDWARE ? SOURCE_HARDWARE : SOURCE_VIRTUAL);

    sourceWidget->boldNameLabel->setText(QLatin1String(""));
    sourceWidget->nameLabel->setText(QString::asprintf("%s", info.description).toHtmlEscaped());
    sourceWidget->nameLabel->setToolTip(QString::fromUtf8(info.description));

    sourceWidget->iconImage->setPixmap(utils::findIcon(info, "audio-input-microphone").pixmap(iconSize()));

    sourceWidget->setVolume(info.volume);
    sourceWidget->muteToggleButton->setChecked(info.mute);

    sourceWidget->setDefault(sourceWidget->name == m_defaultSourceName);

    QVector<pa_source_port_info> ports;
    for (uint32_t i = 0; i < info.n_ports; ++i) {
        ports.append(*info.ports[i]);
    }

    std::sort(ports.begin(), ports.end(), [](const pa_source_port_info &lhs, const pa_source_port_info &rhs) {
        if (lhs.priority == rhs.priority) {
            return strcmp(lhs.name, rhs.name) > 0;
        }
        return lhs.priority > rhs.priority;
    });

    sourceWidget->ports.clear();
    for (const pa_source_port_info &port_priority : ports) {
        sourceWidget->ports.push_back(std::pair<QByteArray, QByteArray>(port_priority.name, port_priority.description));
    }

    sourceWidget->activePort = info.active_port ? info.active_port->name : "";

    std::map<uint32_t, CardWidget *>::iterator cardWidget = m_cardWidgets.find(info.card);
    if (cardWidget != m_cardWidgets.end()) {
        updatePorts(sourceWidget, &cardWidget->second->ports);
    }

    sourceWidget->prepareMenu();

    sourceWidget->updating = false;

    if (isNew) {
        updateDeviceVisibility();
    }
}

void MainWindow::updatePlaybackWidget(const pa_sink_input_info &info)
{
    if (utils::readProperty(info, "module-stream-restore.id") == "sink-input-by-media-role:event") {
//            qDebug("%s", tr("Ignoring sink-input due to it being designated as an event and thus handled by the Event widget").toUtf8().constData());
        return;
    }

    bool is_new = false;
    PlaybackWidget *playbackWidget;
    if (m_playbackWidgets.count(info.index)) {
        playbackWidget = m_playbackWidgets[info.index];

        if (pa_context_get_server_protocol_version(get_context()) >= 13) {
            if (playbackWidget->playbackIndex() != info.sink) {
                createMonitorStreamForPlayback(playbackWidget, info.sink);
            }
        }
    } else {
        m_playbackWidgets[info.index] = playbackWidget = new PlaybackWidget(this);
        connect(playbackWidget, &PlaybackWidget::requestBop, m_popPlayer, &WavPlay::playSound);
        playbackWidget->setChannelMap(info.channel_map, true);
        m_streamsVBox->layout()->addWidget(playbackWidget);

        playbackWidget->index = info.index;
        playbackWidget->clientIndex = info.client;
        is_new = true;
        playbackWidget->setVolumeMeterVisible(m_showVolumeMetersCheckButton->isChecked());

        if (pa_context_get_server_protocol_version(get_context()) >= 13) {
            createMonitorStreamForPlayback(playbackWidget, info.sink);
        }
    }

    playbackWidget->updating = true;

    playbackWidget->type = info.client != PA_INVALID_INDEX ? SINK_INPUT_CLIENT : SINK_INPUT_VIRTUAL;

    playbackWidget->setPlaybackIndex(info.sink);

    if (m_clientNames.contains(info.client)) {
        playbackWidget->boldNameLabel->setText(QStringLiteral("<b>%1</b>").arg(m_clientNames[info.client]));
        playbackWidget->nameLabel->setText(QString::asprintf(": %s", info.name).toHtmlEscaped());
    } else {
        playbackWidget->boldNameLabel->clear();
        playbackWidget->nameLabel->setText(QString::fromUtf8(info.name));
    }

    playbackWidget->nameLabel->setToolTip(QString::fromUtf8(info.name));

    playbackWidget->iconImage->setPixmap(utils::findIcon(info, "audio-card").pixmap(iconSize()));

    playbackWidget->setVolume(info.volume);
    playbackWidget->muteToggleButton->setChecked(info.mute);

    playbackWidget->updating = false;

    if (is_new) {
        updateDeviceVisibility();
    }
}

void MainWindow::updateSourceOutput(const pa_source_output_info &info)
{
    static const QSet<QString> mixers({
            "org.PulseAudio.pavucontrol",
            "org.gnome.VolumeControl",
            "org.kde.kmixd"
        });

    if (mixers.contains(utils::readProperty(info, PA_PROP_APPLICATION_ID))) {
        return;
    }

    bool isNew = false;
    SourceOutputWidget *sourceOutputWidget;
    if (m_sourceOutputWidgets.count(info.index)) {
        sourceOutputWidget = m_sourceOutputWidgets[info.index];
    } else {
        m_sourceOutputWidgets[info.index] = sourceOutputWidget = new SourceOutputWidget(this);
        sourceOutputWidget->setChannelMap(info.channel_map, true);
        m_recsVBox->layout()->addWidget(sourceOutputWidget);

        sourceOutputWidget->index = info.index;
        sourceOutputWidget->clientIndex = info.client;
        isNew = true;
        sourceOutputWidget->setVolumeMeterVisible(m_showVolumeMetersCheckButton->isChecked());
    }

    sourceOutputWidget->updating = true;

    sourceOutputWidget->type = info.client != PA_INVALID_INDEX ? SOURCE_OUTPUT_CLIENT : SOURCE_OUTPUT_VIRTUAL;

    sourceOutputWidget->setSourceIndex(info.source);

    if (m_clientNames.contains(info.client)) {
        sourceOutputWidget->boldNameLabel->setText(QStringLiteral("<b>%1</b> source output client").arg(m_clientNames[info.client]));
        sourceOutputWidget->nameLabel->setText(QString::asprintf(": %s", info.name).toHtmlEscaped() + "[source output]");
    } else {
        sourceOutputWidget->boldNameLabel->clear();
        sourceOutputWidget->nameLabel->setText(QString::fromUtf8(info.name));
    }

    sourceOutputWidget->nameLabel->setToolTip(QString::fromUtf8(info.name));

    sourceOutputWidget->iconImage->setPixmap(utils::findIcon(info, "audio-input-microphone").pixmap(iconSize()));

    sourceOutputWidget->setVolume(info.volume);
    sourceOutputWidget->muteToggleButton->setChecked(info.mute);

    sourceOutputWidget->updating = false;

    if (isNew) {
        updateDeviceVisibility();
    }
}

void MainWindow::updateClient(const pa_client_info &info)
{
    m_clientNames[info.index] = QString::fromUtf8(info.name).toHtmlEscaped();

    for (const std::pair<const uint32_t, PlaybackWidget*> &playbackWidget : m_playbackWidgets) {
        PlaybackWidget *w = playbackWidget.second;

        if (!w) {
            continue;
        }

        if (w->clientIndex == info.index) {
            w->boldNameLabel->setText(QStringLiteral("<b>%1</b>").arg(QString::fromUtf8(info.name).toHtmlEscaped()));
        }
    }
}

void MainWindow::updateServer(const pa_server_info &info)
{
    m_defaultSourceName = info.default_source_name ? info.default_source_name : "";
    m_defaultSinkName = info.default_sink_name ? info.default_sink_name : "";

    for (const std::pair<const uint32_t, SinkWidget*> &sinkWidget : m_sinkWidgets) {
        if (!sinkWidget.second) {
            continue;
        }

        sinkWidget.second->updating = true;
        if (sinkWidget.second->name == m_defaultSinkName) {
            sinkWidget.second->setDefault(true);
        } else {
            sinkWidget.second->setDefault(false);
        }

        sinkWidget.second->updating = false;
    }

    for (const std::pair<const uint32_t, SourceWidget*> &sourceWidget : m_sourceWidgets) {
        SourceWidget *w = sourceWidget.second;

        if (!w) {
            continue;
        }

        w->updating = true;
        w->setDefault(w->name == m_defaultSourceName);
        w->updating = false;
    }
}

bool MainWindow::createEventRoleWidget()
{
    if (m_eventRoleWidget) {
        return false;
    }

    pa_channel_map cm = {
        1, { PA_CHANNEL_POSITION_MONO }
    };

    m_eventRoleWidget = new RoleWidget(this);
    connect(m_eventRoleWidget, &RoleWidget::requestBop, m_popPlayer, &WavPlay::playSound);
    m_streamsVBox->layout()->addWidget(m_eventRoleWidget);
    m_eventRoleWidget->role = "sink-input-by-media-role:event";
    m_eventRoleWidget->setChannelMap(cm, true);

    m_eventRoleWidget->boldNameLabel->clear();
    m_eventRoleWidget->nameLabel->setText(tr("System Sounds"));

    m_eventRoleWidget->iconImage->setPixmap(QIcon::fromTheme("multimedia-volume-control").pixmap(iconSize()));

    m_eventRoleWidget->device = "";

    m_eventRoleWidget->updating = true;

    pa_cvolume volume;
    volume.channels = 1;
    volume.values[0] = PA_VOLUME_NORM;

    m_eventRoleWidget->setVolume(volume);
    m_eventRoleWidget->muteToggleButton->setChecked(false);

    m_eventRoleWidget->updating = false;
    return true;
}

void MainWindow::deleteEventRoleWidget()
{
    delete m_eventRoleWidget;
    m_eventRoleWidget = nullptr;
}

int MainWindow::iconSize() {
    return style()->pixelMetric(QStyle::PM_ToolBarIconSize);
}

void MainWindow::updateRole(const pa_ext_stream_restore_info &info)
{
    pa_cvolume volume;
    bool is_new = false;

    if (strcmp(info.name, "sink-input-by-media-role:event") != 0) {
        return;
    }

    is_new = createEventRoleWidget();

    m_eventRoleWidget->updating = true;

    m_eventRoleWidget->device = info.device ? info.device : "";

    volume.channels = 1;
    volume.values[0] = pa_cvolume_max(&info.volume);

    m_eventRoleWidget->setVolume(volume);
    m_eventRoleWidget->muteToggleButton->setChecked(info.mute);

    m_eventRoleWidget->updating = false;

    if (is_new) {
        updateDeviceVisibility();
    }
}

void MainWindow::updateDeviceInfo(const pa_ext_device_restore_info &info)
{
    if (!m_sinkWidgets.count(info.index)) {
        return;
    }

    SinkWidget *sinkWidget;
    pa_format_info *format;

    sinkWidget = m_sinkWidgets[info.index];

    sinkWidget->updating = true;

    /* Unselect everything */
    for (int j = 1; j < PAVU_NUM_ENCODINGS; ++j) {
        sinkWidget->encodings[j].widget->setChecked(false);
    }


    for (uint8_t i = 0; i < info.n_formats; ++i) {
        format = info.formats[i];

        for (int j = 1; j < PAVU_NUM_ENCODINGS; ++j) {
            if (format->encoding == sinkWidget->encodings[j].encoding) {
                sinkWidget->encodings[j].widget->setChecked(true);
                break;
            }
        }
    }

    sinkWidget->updating = false;
}


void MainWindow::updateVolumeMeter(uint32_t source_index, uint32_t sink_input_idx, double v)
{
    if (sink_input_idx != PA_INVALID_INDEX) {
        PlaybackWidget *playbackWidget;

        if (m_playbackWidgets.count(sink_input_idx)) {
            playbackWidget = m_playbackWidgets[sink_input_idx];
            playbackWidget->updatePeak(v);
        }
    } else {
        for (const std::pair<const uint32_t, SinkWidget*> &sinkWidget : m_sinkWidgets) {
            if (sinkWidget.second->monitor_index == source_index) {
                sinkWidget.second->updatePeak(v);
            }
        }

        for (const std::pair<const uint32_t, SourceWidget*> &sourceWidget : m_sourceWidgets) {
            if (sourceWidget.second->index == source_index) {
                sourceWidget.second->updatePeak(v);
            }
        }

        for (const std::pair<const uint32_t, SourceOutputWidget*> &sourceOutputWidget : m_sourceOutputWidgets) {
            if (sourceOutputWidget.second->sourceIndex() == source_index) {
                sourceOutputWidget.second->updatePeak(v);
            }
        }
    }
}

void MainWindow::setConnectionState(bool connected)
{
    if (m_connected == connected) {
        return;
    }
    m_connected = connected;

    if (m_connected) {
        m_connectingLabel->hide();
        m_notebook->show();
    } else {
        m_notebook->hide();
        m_connectingLabel->show();
    }
}

// TODO: hack to quickly port away from glib
static bool has_updated = true;

void MainWindow::updateDeviceVisibility()
{
    if (!has_updated) {
        return;
    }

    has_updated = false;
    QMetaObject::invokeMethod(this, &MainWindow::reallyUpdateDeviceVisibility);
}

void MainWindow::reallyUpdateDeviceVisibility()
{
    has_updated = true;

    bool is_empty = true;

    for (const std::pair<const uint32_t, PlaybackWidget*> &siw : m_playbackWidgets) {
        PlaybackWidget *playbackWidget = siw.second;

        if (m_sinkWidgets.size() > 1) {
            playbackWidget->directionLabel->show();
            playbackWidget->deviceButton->show();
        } else {
            playbackWidget->directionLabel->hide();
            playbackWidget->deviceButton->hide();
        }

        if (m_showPlaybackType == SINK_INPUT_ALL || playbackWidget->type == m_showPlaybackType) {
            playbackWidget->show();
            is_empty = false;
        } else {
            playbackWidget->hide();
        }
    }

    if (m_eventRoleWidget) {
        is_empty = false;
    }

    if (is_empty) {
        m_noStreamsLabel->show();
    } else {
        m_noStreamsLabel->hide();
    }

    is_empty = true;

    for (const std::pair<const uint32_t, SourceOutputWidget*> &sow : m_sourceOutputWidgets) {
        SourceOutputWidget *sourceOutputWidget = sow.second;

        if (m_sourceWidgets.size() > 1) {
            sourceOutputWidget->directionLabel->show();
            sourceOutputWidget->deviceButton->show();
        } else {
            sourceOutputWidget->directionLabel->hide();
            sourceOutputWidget->deviceButton->hide();
        }

        if (m_showSourceOutputType == SOURCE_OUTPUT_ALL || sourceOutputWidget->type == m_showSourceOutputType) {
            sourceOutputWidget->show();
            is_empty = false;
        } else {
            sourceOutputWidget->hide();
        }
    }

    if (is_empty) {
        m_noRecsLabel->show();
    } else {
        m_noRecsLabel->hide();
    }

    is_empty = true;

    for (const std::pair<const uint32_t, SinkWidget*> &sw : m_sinkWidgets) {
        SinkWidget *sinkWidget = sw.second;

        if (m_showSinkType == SINK_ALL || sinkWidget->type == m_showSinkType) {
            sinkWidget->show();
            is_empty = false;
        } else {
            sinkWidget->hide();
        }
    }

    if (is_empty) {
        m_noSinksLabel->show();
    } else {
        m_noSinksLabel->hide();
    }

    is_empty = true;

    for (const std::pair<const uint32_t, CardWidget*> &cardWidget : m_cardWidgets) {
        cardWidget.second->show();
        is_empty = false;
    }

    if (is_empty) {
        m_noCardsLabel->show();
    } else {
        m_noCardsLabel->hide();
    }

    is_empty = true;

    for (const std::pair<const uint32_t, SourceWidget*> &sw : m_sourceWidgets) {
        SourceWidget *sourceWidget = sw.second;

        if (m_showSourceType == SOURCE_ALL ||
                sourceWidget->type == m_showSourceType ||
                (m_showSourceType == SOURCE_NO_MONITOR && sourceWidget->type != SOURCE_MONITOR)) {
            sourceWidget->show();
            is_empty = false;
        } else {
            sourceWidget->hide();
        }
    }

    if (is_empty) {
        m_noSourcesLabel->show();
    } else {
        m_noSourcesLabel->hide();
    }

    adjustSize();
}

void MainWindow::removeCard(uint32_t index)
{
    if (!m_cardWidgets.count(index)) {
        return;
    }

    delete m_cardWidgets[index];
    m_cardWidgets.erase(index);
    updateDeviceVisibility();
}

void MainWindow::removeSink(uint32_t index)
{
    if (!m_sinkWidgets.count(index)) {
        return;
    }

    delete m_sinkWidgets[index];
    m_sinkWidgets.erase(index);
    updateDeviceVisibility();
}

void MainWindow::removeSource(uint32_t index)
{
    if (!m_sourceWidgets.count(index)) {
        return;
    }

    delete m_sourceWidgets[index];
    m_sourceWidgets.erase(index);
    updateDeviceVisibility();
}

void MainWindow::removePlaybackWidget(uint32_t index)
{
    if (!m_playbackWidgets.count(index)) {
        return;
    }

    delete m_playbackWidgets[index];
    m_playbackWidgets.erase(index);
    updateDeviceVisibility();
}

void MainWindow::removeSourceOutput(uint32_t index)
{
    if (!m_sourceOutputWidgets.count(index)) {
        return;
    }

    delete m_sourceOutputWidgets[index];
    m_sourceOutputWidgets.erase(index);
    updateDeviceVisibility();
}

void MainWindow::removeClient(uint32_t index)
{
    m_clientNames.remove(index);
}

void MainWindow::removeAllWidgets()
{
    for (const std::pair<const uint32_t, PlaybackWidget*> &playbackWidget : m_playbackWidgets) {
        playbackWidget.second->deleteLater();
    }
    m_playbackWidgets.clear();

    for (const std::pair<const uint32_t, SourceOutputWidget*> &sourceOutputWidget : m_sourceOutputWidgets) {
        sourceOutputWidget.second->deleteLater();
    }
    m_sourceOutputWidgets.clear();

    for (const std::pair<const uint32_t, SinkWidget*> &sinkWidget : m_sinkWidgets) {
        sinkWidget.second->deleteLater();
    }
    m_sinkWidgets.clear();

    for (const std::pair<const uint32_t, SourceWidget*> &sourceWidget : m_sourceWidgets) {
        sourceWidget.second->deleteLater();
    }
    m_sourceWidgets.clear();

    for (const std::pair<const uint32_t, CardWidget*> &cardWidget : m_cardWidgets) {
        cardWidget.second->deleteLater();
    }
    m_cardWidgets.clear();

    m_clientNames.clear();
    deleteEventRoleWidget();

    updateDeviceVisibility();
}

void MainWindow::setConnectingMessage(const char *string)
{
    QByteArray markup = "<i>";

    if (!string) {
        markup += tr("Establishing connection to PulseAudio. Please wait...").toUtf8().constData();
    } else {
        markup += string;
    }

    markup += "</i>";
    m_connectingLabel->setText(QString::fromUtf8(markup));
}

void MainWindow::onSinkTypeComboBoxChanged(int index)
{
    Q_UNUSED(index);

    m_showSinkType = (SinkType) m_sinkTypeComboBox->currentIndex();

    if (m_showSinkType == (SinkType) - 1) {
        m_sinkTypeComboBox->setCurrentIndex((int) SINK_ALL);
    }

    updateDeviceVisibility();
}

void MainWindow::onSourceTypeComboBoxChanged(int index)
{
    Q_UNUSED(index);

    m_showSourceType = (SourceType) m_sourceTypeComboBox->currentIndex();

    if (m_showSourceType == (SourceType) - 1) {
        m_sourceTypeComboBox->setCurrentIndex((int) SOURCE_NO_MONITOR);
    }

    updateDeviceVisibility();
}

void MainWindow::onPlaybackTypeComboBoxChanged(int index)
{
    Q_UNUSED(index);

    m_showPlaybackType = (PlaybackType) m_playbackTypeComboBox->currentIndex();

    if (m_showPlaybackType == (PlaybackType) - 1) {
        m_playbackTypeComboBox->setCurrentIndex((int) SINK_INPUT_CLIENT);
    }

    updateDeviceVisibility();
}

void MainWindow::onSourceOutputTypeComboBoxChanged(int index)
{
    Q_UNUSED(index);

    m_showSourceOutputType = (SourceOutputType) m_sourceOutputTypeComboBox->currentIndex();

    if (m_showSourceOutputType == (SourceOutputType) - 1) {
        m_sourceOutputTypeComboBox->setCurrentIndex((int) SOURCE_OUTPUT_CLIENT);
    }

    updateDeviceVisibility();
}


void MainWindow::onShowVolumeMetersCheckButtonToggled(bool toggled)
{
    Q_UNUSED(toggled);

    bool state = m_showVolumeMetersCheckButton->isChecked();
    pa_operation *operation = nullptr;

    for (const std::pair<const uint32_t, SinkWidget*> &sw : m_sinkWidgets) {
        SinkWidget *sinkWidget = sw.second;

        if (sinkWidget->peak) {
            operation = pa_stream_cork(sinkWidget->peak, (int)!state, nullptr, nullptr);

            if (operation) {
                pa_operation_unref(operation);
            }
        }

        sinkWidget->setVolumeMeterVisible(state);
    }

    for (const std::pair<const uint32_t, SourceWidget*> &sw : m_sourceWidgets) {
        SourceWidget *sourceWidget = sw.second;

        if (sourceWidget->peak) {
            operation = pa_stream_cork(sourceWidget->peak, (int)!state, nullptr, nullptr);

            if (operation) {
                pa_operation_unref(operation);
            }
        }

        sourceWidget->setVolumeMeterVisible(state);
    }

    for (const std::pair<const uint32_t, PlaybackWidget*> &siw : m_playbackWidgets) {
        PlaybackWidget *playbackWidget = siw.second;

        if (playbackWidget->peak) {
            operation = pa_stream_cork(playbackWidget->peak, (int)!state, nullptr, nullptr);

            if (operation) {
                pa_operation_unref(operation);
            }
        }

        playbackWidget->setVolumeMeterVisible(state);
    }

    for (const std::pair<const uint32_t, SourceOutputWidget*> &sow : m_sourceOutputWidgets) {
        SourceOutputWidget *sourceOutputWidget = sow.second;

        if (sourceOutputWidget->peak) {
            operation = pa_stream_cork(sourceOutputWidget->peak, (int)!state, nullptr, nullptr);

            if (operation) {
                pa_operation_unref(operation);
            }
        }

        sourceOutputWidget->setVolumeMeterVisible(state);
    }
}
