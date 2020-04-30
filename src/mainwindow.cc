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
#include "sinkinputwidget.h"
#include "sourceoutputwidget.h"
#include "rolewidget.h"
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

/* Used for profile sorting */
struct profile_prio_compare {
    bool operator()(pa_card_profile_info2 const *const lhs, pa_card_profile_info2 const *const rhs) const
    {

        if (lhs->priority == rhs->priority) {
            return strcmp(lhs->name, rhs->name) > 0;
        }

        return lhs->priority > rhs->priority;
    }
};

struct sink_port_prio_compare {
    bool operator()(const pa_sink_port_info &lhs, const pa_sink_port_info &rhs) const
    {

        if (lhs.priority == rhs.priority) {
            return strcmp(lhs.name, rhs.name) > 0;
        }

        return lhs.priority > rhs.priority;
    }
};

struct source_port_prio_compare {
    bool operator()(const pa_source_port_info &lhs, const pa_source_port_info &rhs) const
    {

        if (lhs.priority == rhs.priority) {
            return strcmp(lhs.name, rhs.name) > 0;
        }

        return lhs.priority > rhs.priority;
    }
};

QWidget *createTab(QWidget *contentList, QLabel *defaultLabel, QWidget *typeSelect)
{
    contentList->setLayout(new QVBoxLayout);

    QWidget *tab = new QWidget;
    QFormLayout *tabLayout = new QFormLayout(tab);
    tabLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidget(contentList);
    tabLayout->addRow(scrollArea);
    tabLayout->addRow(QObject::tr("Show:"), typeSelect);

    scrollArea->setLayout(new QVBoxLayout);
    scrollArea->setWidgetResizable(true);
    contentList->layout()->addWidget(defaultLabel);

    contentList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    scrollArea->layout()->addWidget(contentList);
    scrollArea->setWidget(contentList);

    return tab;
}

MainWindow::MainWindow(QWidget *parent):
    QDialog(parent),
    showSinkInputType(SINK_INPUT_CLIENT),
    showSinkType(SINK_ALL),
    showSourceOutputType(SOURCE_OUTPUT_CLIENT),
    showSourceType(SOURCE_NO_MONITOR),
    eventRoleWidget(nullptr),
    canRenameDevices(false),
    m_connected(false),
    m_config_filename(nullptr)
{
    setLayout(new QVBoxLayout);

    ////////////////////
    // Create widgets
    m_notebook = new QTabWidget;

    m_sinkInputTypeComboBox = new QComboBox;
    m_sinkInputTypeComboBox->addItems( {
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
    m_notebook->addTab(createTab(m_streamsVBox, m_noStreamsLabel, m_sinkInputTypeComboBox), tr("&Playback"));
    m_notebook->addTab(createTab(m_recsVBox, m_noRecsLabel, m_sourceOutputTypeComboBox), tr("&Recording"));
    m_notebook->addTab(createTab(m_sinksVBox, m_noSinksLabel, m_sinkTypeComboBox), tr("&Output Devices"));
    m_notebook->addTab(createTab(m_sourcesVBox, m_noSourcesLabel, m_sourceTypeComboBox), tr("&Input Devices"));
    m_notebook->addTab(createTab(m_cardsVBox, m_noCardsLabel, m_showVolumeMetersCheckButton), tr("&Configuration"));

    layout()->addWidget(m_notebook);
    layout()->addWidget(m_connectingLabel);

    m_sinkInputTypeComboBox->setCurrentIndex((int) showSinkInputType);
    m_sourceOutputTypeComboBox->setCurrentIndex((int) showSourceOutputType);
    m_sinkTypeComboBox->setCurrentIndex((int) showSinkType);
    m_sourceTypeComboBox->setCurrentIndex((int) showSourceType);


    connect(m_sinkInputTypeComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &MainWindow::onSinkInputTypeComboBoxChanged);
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

    const QVariant sinkInputTypeSelection = config.value(QStringLiteral("window/sinkInputType"));

    if (sinkInputTypeSelection.isValid()) {
        m_sinkInputTypeComboBox->setCurrentIndex(sinkInputTypeSelection.toInt());
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
    config.setValue(QStringLiteral("window/sinkInputType"), m_sinkInputTypeComboBox->currentIndex());
    config.setValue(QStringLiteral("window/sourceOutputType"), m_sourceOutputTypeComboBox->currentIndex());
    config.setValue(QStringLiteral("window/sinkType"), m_sinkTypeComboBox->currentIndex());
    config.setValue(QStringLiteral("window/sourceType"), m_sourceTypeComboBox->currentIndex());
    config.setValue(QStringLiteral("window/showVolumeMeters"), m_showVolumeMetersCheckButton->isChecked());

    clientNames.clear();
}

class DeviceWidget;
static void updatePorts(DeviceWidget *w, std::map<QByteArray, PortInfo> *ports)
{
    std::map<QByteArray, PortInfo>::iterator it;
    PortInfo p;

    for (std::pair<QByteArray, QByteArray> &port : w->ports) {
        QByteArray desc;
        it = ports->find(port.first);

        if (it == ports->end()) {
            continue;
        }

        p = it->second;
        desc = p.description;

        if (p.available == PA_PORT_AVAILABLE_YES) {
            desc +=  MainWindow::tr(" (plugged in)").toUtf8().constData();
        } else if (p.available == PA_PORT_AVAILABLE_NO) {
            if (p.name == "analog-output-speaker" ||
                    p.name == "analog-input-microphone-internal") {
                desc += MainWindow::tr(" (unavailable)").toUtf8().constData();
            } else {
                desc += MainWindow::tr(" (unplugged)").toUtf8().constData();
            }
        }

        port.second = desc;
    }

    it = ports->find(w->activePort);

    if (it != ports->end()) {
        p = it->second;
        w->setLatencyOffset(p.latency_offset);
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
    CardWidget *w;
    bool is_new = false;
    const char *description, *icon;
    std::set<pa_card_profile_info2 *, profile_prio_compare> profile_priorities;

    if (cardWidgets.count(info.index)) {
        w = cardWidgets[info.index];
    } else {
        cardWidgets[info.index] = w = new CardWidget(this);
        m_cardsVBox->layout()->addWidget(w);
        w->index = info.index;
        is_new = true;
    }

    w->updating = true;

    description = pa_proplist_gets(info.proplist, PA_PROP_DEVICE_DESCRIPTION);
    w->name = description ? description : info.name;
    w->nameLabel->setText(QString::fromUtf8(w->name));

    icon = pa_proplist_gets(info.proplist, PA_PROP_DEVICE_ICON_NAME);
    setIconByName(w->iconImage, icon, "audio-card");

    w->hasSinks = w->hasSources = false;
    profile_priorities.clear();

    for (pa_card_profile_info2 **p_profile = info.profiles2; *p_profile != nullptr; ++p_profile) {
        w->hasSinks = w->hasSinks || ((*p_profile)->n_sinks > 0);
        w->hasSources = w->hasSources || ((*p_profile)->n_sources > 0);
        profile_priorities.insert(*p_profile);
    }

    w->ports.clear();

    for (uint32_t i = 0; i < info.n_ports; ++i) {
        PortInfo p;

        p.name = info.ports[i]->name;
        p.description = info.ports[i]->description;
        p.priority = info.ports[i]->priority;
        p.available = info.ports[i]->available;
        p.direction = info.ports[i]->direction;
        p.latency_offset = info.ports[i]->latency_offset;

        for (pa_card_profile_info2 **p_profile = info.ports[i]->profiles2; *p_profile != nullptr; ++p_profile) {
            p.profiles.push_back((*p_profile)->name);
        }

        w->ports[p.name] = p;
    }

    w->profiles.clear();

    for (pa_card_profile_info2 *p_profile : profile_priorities) {
        bool hasNo = false, hasOther = false;
        std::map<QByteArray, PortInfo>::iterator portIt;
        QByteArray desc = p_profile->description;

        for (portIt = w->ports.begin(); portIt != w->ports.end(); portIt++) {
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

        w->profiles.push_back(std::pair<QByteArray, QByteArray>(p_profile->name, desc));

        if (p_profile->n_sinks == 0 && p_profile->n_sources == 0) {
            w->noInOutProfile = p_profile->name;
        }
    }

    w->activeProfile = info.active_profile ? info.active_profile->name : "";

    /* Because the port info for sinks and sources is discontinued we need
     * to update the port info for them here. */
    if (w->hasSinks) {
        std::map<uint32_t, SinkWidget *>::iterator it;

        for (it = sinkWidgets.begin() ; it != sinkWidgets.end(); it++) {
            SinkWidget *sw = it->second;

            if (sw->card_index == w->index) {
                sw->updating = true;
                updatePorts(sw, &w->ports);
                sw->updating = false;
            }
        }
    }

    if (w->hasSources) {
        std::map<uint32_t, SourceWidget *>::iterator it;

        for (it = sourceWidgets.begin() ; it != sourceWidgets.end(); it++) {
            SourceWidget *sw = it->second;

            if (sw->card_index == w->index) {
                sw->updating = true;
                updatePorts(sw, &w->ports);
                sw->updating = false;
            }
        }
    }

    w->prepareMenu();

    if (is_new) {
        updateDeviceVisibility();
    }

    w->updating = false;
}

bool MainWindow::updateSink(const pa_sink_info &info)
{
    SinkWidget *w;
    bool is_new = false;

    const char *icon;
    std::map<uint32_t, CardWidget *>::iterator cw;
    std::set<pa_sink_port_info, sink_port_prio_compare> port_priorities;

    if (sinkWidgets.count(info.index)) {
        w = sinkWidgets[info.index];
    } else {
        sinkWidgets[info.index] = w = new SinkWidget(this);
        w->setChannelMap(info.channel_map, !!(info.flags & PA_SINK_DECIBEL_VOLUME));
        m_sinksVBox->layout()->addWidget(w);
        w->index = info.index;
        w->monitor_index = info.monitor_source;
        is_new = true;

        w->setBaseVolume(info.base_volume);
        w->setVolumeMeterVisible(m_showVolumeMetersCheckButton->isChecked());
    }

    w->updating = true;

    w->card_index = info.card;
    w->name = info.name;
    w->description = info.description;
    w->type = info.flags & PA_SINK_HARDWARE ? SINK_HARDWARE : SINK_VIRTUAL;

    w->boldNameLabel->setText(QLatin1String(""));
    w->nameLabel->setText(QString::asprintf("%s", info.description).toHtmlEscaped());
    w->nameLabel->setToolTip(QString::fromUtf8(info.description));

    icon = pa_proplist_gets(info.proplist, PA_PROP_DEVICE_ICON_NAME);
    setIconByName(w->iconImage, icon, "audio-card");

    w->setVolume(info.volume);
    w->muteToggleButton->setChecked(info.mute);

    w->setDefault(w->name == defaultSinkName);

    port_priorities.clear();

    for (uint32_t i = 0; i < info.n_ports; ++i) {
        port_priorities.insert(*info.ports[i]);
    }

    w->ports.clear();

    for (const pa_sink_port_info &port_priority : port_priorities) {
        w->ports.push_back(std::pair<QByteArray, QByteArray>(port_priority.name, port_priority.description));
    }

    w->activePort = info.active_port ? info.active_port->name : "";

    cw = cardWidgets.find(info.card);

    if (cw != cardWidgets.end()) {
        updatePorts(w, &cw->second->ports);
    }

#ifdef PA_SINK_SET_FORMATS
    w->setDigital(info.flags & PA_SINK_SET_FORMATS);
#endif

    w->prepareMenu();

    w->updating = false;

    if (is_new) {
        updateDeviceVisibility();
    }

    return is_new;
}

static void suspended_callback(pa_stream *s, void *userdata)
{
    MainWindow *w = static_cast<MainWindow *>(userdata);

    if (pa_stream_is_suspended(s)) {
        w->updateVolumeMeter(pa_stream_get_device_index(s), PA_INVALID_INDEX, -1);
    }
}

static void read_callback(pa_stream *s, size_t length, void *userdata)
{
    MainWindow *w = static_cast<MainWindow *>(userdata);
    const void *data;
    double v;

    if (pa_stream_peek(s, &data, &length) < 0) {
        show_error(MainWindow::tr("Failed to read data from stream").toUtf8().constData());
        return;
    }

    if (!data) {
        /* nullptr data means either a hole or empty buffer.
         * Only drop the stream when there is a hole (length > 0) */
        if (length) {
            pa_stream_drop(s);
        }

        return;
    }

    assert(length > 0);
    assert(length % sizeof(float) == 0);

    v = ((const float *) data)[length / sizeof(float) -1];

    pa_stream_drop(s);

    if (v < 0) {
        v = 0;
    }

    if (v > 1) {
        v = 1;
    }

    w->updateVolumeMeter(pa_stream_get_device_index(s), pa_stream_get_monitor_stream(s), v);
}

pa_stream *MainWindow::createMonitorStreamForSource(uint32_t source_idx, uint32_t stream_idx = -1, bool suspend = false)
{
    pa_stream *s;
    char t[16];
    pa_buffer_attr attr;
    pa_sample_spec ss;
    pa_stream_flags_t flags;

    ss.channels = 1;
    ss.format = PA_SAMPLE_FLOAT32;
    ss.rate = 25;

    memset(&attr, 0, sizeof(attr));
    attr.fragsize = sizeof(float);
    attr.maxlength = (uint32_t) -1;

    snprintf(t, sizeof(t), "%u", source_idx);

    if (!(s = pa_stream_new(get_context(), tr("Peak detect").toUtf8().constData(), &ss, nullptr))) {
        show_error(tr("Failed to create monitoring stream").toUtf8().constData());
        return nullptr;
    }

    if (stream_idx != (uint32_t) -1) {
        pa_stream_set_monitor_stream(s, stream_idx);
    }

    pa_stream_set_read_callback(s, read_callback, this);
    pa_stream_set_suspended_callback(s, suspended_callback, this);

    flags = (pa_stream_flags_t)(PA_STREAM_DONT_MOVE | PA_STREAM_PEAK_DETECT | PA_STREAM_ADJUST_LATENCY |
                                (suspend ? PA_STREAM_DONT_INHIBIT_AUTO_SUSPEND : PA_STREAM_NOFLAGS) |
                                (!m_showVolumeMetersCheckButton->isChecked() ? PA_STREAM_START_CORKED : PA_STREAM_NOFLAGS));

    if (pa_stream_connect_record(s, t, &attr, flags) < 0) {
        show_error(tr("Failed to connect monitoring stream").toUtf8().constData());
        pa_stream_unref(s);
        return nullptr;
    }

    return s;
}

void MainWindow::createMonitorStreamForSinkInput(SinkInputWidget *w, uint32_t sink_idx)
{
    if (!sinkWidgets.count(sink_idx)) {
        return;
    }

    if (w->peak) {
        pa_stream_disconnect(w->peak);
        w->peak = nullptr;
    }

    w->peak = createMonitorStreamForSource(sinkWidgets[sink_idx]->monitor_index, w->index);
}

void MainWindow::updateSource(const pa_source_info &info)
{
    SourceWidget *w;
    bool is_new = false;
    std::map<uint32_t, CardWidget *>::iterator cw;
    std::set<pa_source_port_info, source_port_prio_compare> port_priorities;

    if (sourceWidgets.count(info.index)) {
        w = sourceWidgets[info.index];
    } else {
        sourceWidgets[info.index] = w = new SourceWidget(this);
        w->setChannelMap(info.channel_map, !!(info.flags & PA_SOURCE_DECIBEL_VOLUME));
        m_sourcesVBox->layout()->addWidget(w);

        w->index = info.index;
        is_new = true;

        w->setBaseVolume(info.base_volume);
        w->setVolumeMeterVisible(m_showVolumeMetersCheckButton->isChecked());

        if (pa_context_get_server_protocol_version(get_context()) >= 13) {
            w->peak = createMonitorStreamForSource(info.index, -1, !!(info.flags & PA_SOURCE_NETWORK));
        }
    }

    w->updating = true;

    w->card_index = info.card;
    w->name = info.name;
    w->description = info.description;
    w->type = info.monitor_of_sink != PA_INVALID_INDEX ? SOURCE_MONITOR : (info.flags & PA_SOURCE_HARDWARE ? SOURCE_HARDWARE : SOURCE_VIRTUAL);

    w->boldNameLabel->setText(QLatin1String(""));
    w->nameLabel->setText(QString::asprintf("%s", info.description).toHtmlEscaped());
    w->nameLabel->setToolTip(QString::fromUtf8(info.description));

    setIconFromProplist(w->iconImage, info.proplist, "audio-input-microphone");

    w->setVolume(info.volume);
    w->muteToggleButton->setChecked(info.mute);

    w->setDefault(w->name == defaultSourceName);

    port_priorities.clear();

    for (uint32_t i = 0; i < info.n_ports; ++i) {
        port_priorities.insert(*info.ports[i]);
    }


    w->ports.clear();

    for (const pa_source_port_info &port_priority : port_priorities) {
        w->ports.push_back(std::pair<QByteArray, QByteArray>(port_priority.name, port_priority.description));
    }

    w->activePort = info.active_port ? info.active_port->name : "";

    cw = cardWidgets.find(info.card);

    if (cw != cardWidgets.end()) {
        updatePorts(w, &cw->second->ports);
    }

    w->prepareMenu();

    w->updating = false;

    if (is_new) {
        updateDeviceVisibility();
    }
}


void MainWindow::setIconFromProplist(QLabel *icon, pa_proplist *l, const char *fallback)
{
    QByteArray name = pa_proplist_gets(l, PA_PROP_MEDIA_ICON_NAME);

    if (!name.isEmpty()) {
        setIconByName(icon, name, fallback);
        return;
    }

    name = pa_proplist_gets(l, PA_PROP_WINDOW_ICON_NAME);
    if (!name.isEmpty()) {
        setIconByName(icon, name, fallback);
        return;
    }

    name = pa_proplist_gets(l, PA_PROP_APPLICATION_ICON_NAME);
    if (!name.isEmpty()) {
        setIconByName(icon, name, fallback);
        return;
    }

    const QByteArray role = pa_proplist_gets(l, PA_PROP_MEDIA_ROLE);
    if (role.isEmpty()) {
        setIconByName(icon, name, fallback);
        return;
    }


    if (role == "video" || role == "phone") {
        setIconByName(icon, role, fallback);
        return;
    }

    if (role == "music") {
        setIconByName(icon, "audio", fallback);
        return;
    }

    if (role == "game") {
        setIconByName(icon, "applications-games", fallback);
        return;
    }
    if (role == "event") {
        setIconByName(icon, "dialog-information", fallback);
        return;
    }

    setIconByName(icon, role, fallback);
}


void MainWindow::updateSinkInput(const pa_sink_input_info &info)
{
    const char *t;
    SinkInputWidget *w;
    bool is_new = false;

    if ((t = pa_proplist_gets(info.proplist, "module-stream-restore.id"))) {
        if (strcmp(t, "sink-input-by-media-role:event") == 0) {
            qDebug("%s", tr("Ignoring sink-input due to it being designated as an event and thus handled by the Event widget").toUtf8().constData());
            return;
        }
    }

    if (sinkInputWidgets.count(info.index)) {
        w = sinkInputWidgets[info.index];

        if (pa_context_get_server_protocol_version(get_context()) >= 13) {
            if (w->sinkIndex() != info.sink) {
                createMonitorStreamForSinkInput(w, info.sink);
            }
        }
    } else {
        sinkInputWidgets[info.index] = w = new SinkInputWidget(this);
        w->setChannelMap(info.channel_map, true);
        m_streamsVBox->layout()->addWidget(w);

        w->index = info.index;
        w->clientIndex = info.client;
        is_new = true;
        w->setVolumeMeterVisible(m_showVolumeMetersCheckButton->isChecked());

        if (pa_context_get_server_protocol_version(get_context()) >= 13) {
            createMonitorStreamForSinkInput(w, info.sink);
        }
    }

    w->updating = true;

    w->type = info.client != PA_INVALID_INDEX ? SINK_INPUT_CLIENT : SINK_INPUT_VIRTUAL;

    w->setSinkIndex(info.sink);

    if (clientNames.contains(info.client)) {
        w->boldNameLabel->setText(QStringLiteral("<b>%1</b>").arg(clientNames[info.client]));
        w->nameLabel->setText(QString::asprintf(": %s", info.name).toHtmlEscaped());
    } else {
        w->boldNameLabel->setText(QLatin1String(""));
        w->nameLabel->setText(QString::fromUtf8(info.name));
    }

    w->nameLabel->setToolTip(QString::fromUtf8(info.name));

    setIconFromProplist(w->iconImage, info.proplist, "audio-card");

    w->setVolume(info.volume);
    w->muteToggleButton->setChecked(info.mute);

    w->updating = false;

    if (is_new) {
        updateDeviceVisibility();
    }
}

void MainWindow::updateSourceOutput(const pa_source_output_info &info)
{
    SourceOutputWidget *w;
    const char *app;
    bool is_new = false;

    if ((app = pa_proplist_gets(info.proplist, PA_PROP_APPLICATION_ID))) {
        if (strcmp(app, "org.PulseAudio.pavucontrol") == 0
                || strcmp(app, "org.gnome.VolumeControl") == 0
                || strcmp(app, "org.kde.kmixd") == 0) {
            return;
        }
    }

    if (sourceOutputWidgets.count(info.index)) {
        w = sourceOutputWidgets[info.index];
    } else {
        sourceOutputWidgets[info.index] = w = new SourceOutputWidget(this);
        w->setChannelMap(info.channel_map, true);
        m_recsVBox->layout()->addWidget(w);

        w->index = info.index;
        w->clientIndex = info.client;
        is_new = true;
        w->setVolumeMeterVisible(m_showVolumeMetersCheckButton->isChecked());
    }

    w->updating = true;

    w->type = info.client != PA_INVALID_INDEX ? SOURCE_OUTPUT_CLIENT : SOURCE_OUTPUT_VIRTUAL;

    w->setSourceIndex(info.source);

    if (clientNames.contains(info.client)) {
        w->boldNameLabel->setText(QStringLiteral("<b>%1</b>").arg(clientNames[info.client]));
        w->nameLabel->setText(QString::asprintf(": %s", info.name).toHtmlEscaped());
    } else {
        w->boldNameLabel->setText(QLatin1String(""));
        w->nameLabel->setText(QString::fromUtf8(info.name));
    }

    w->nameLabel->setToolTip(QString::fromUtf8(info.name));

    setIconFromProplist(w->iconImage, info.proplist, "audio-input-microphone");

    w->setVolume(info.volume);
    w->muteToggleButton->setChecked(info.mute);

    w->updating = false;

    if (is_new) {
        updateDeviceVisibility();
    }
}

void MainWindow::updateClient(const pa_client_info &info)
{
    clientNames[info.index] = QString::fromUtf8(info.name).toHtmlEscaped();

    for (const std::pair<uint32_t, SinkInputWidget*> &sinkInputWidget : sinkInputWidgets) {
        SinkInputWidget *w = sinkInputWidget.second;

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
    defaultSourceName = info.default_source_name ? info.default_source_name : "";
    defaultSinkName = info.default_sink_name ? info.default_sink_name : "";

    for (const std::pair<uint32_t, SinkWidget*> &sinkWidget : sinkWidgets) {
        SinkWidget *w = sinkWidget.second;

        if (!w) {
            continue;
        }

        w->updating = true;
        w->setDefault(w->name == defaultSinkName);

        w->updating = false;
    }

    for (const std::pair<uint32_t, SourceWidget*> &sourceWidget : sourceWidgets) {
        SourceWidget *w = sourceWidget.second;

        if (!w) {
            continue;
        }

        w->updating = true;
        w->setDefault(w->name == defaultSourceName);
        w->updating = false;
    }
}

bool MainWindow::createEventRoleWidget()
{
    if (eventRoleWidget) {
        return false;
    }

    pa_channel_map cm = {
        1, { PA_CHANNEL_POSITION_MONO }
    };

    eventRoleWidget = new RoleWidget(this);
    m_streamsVBox->layout()->addWidget(eventRoleWidget);
    eventRoleWidget->role = "sink-input-by-media-role:event";
    eventRoleWidget->setChannelMap(cm, true);

    eventRoleWidget->boldNameLabel->setText(QLatin1String(""));
    eventRoleWidget->nameLabel->setText(tr("System Sounds"));

    setIconByName(eventRoleWidget->iconImage, "multimedia-volume-control", "multimedia-volume-control-symbolic");

    eventRoleWidget->device = "";

    eventRoleWidget->updating = true;

    pa_cvolume volume;
    volume.channels = 1;
    volume.values[0] = PA_VOLUME_NORM;

    eventRoleWidget->setVolume(volume);
    eventRoleWidget->muteToggleButton->setChecked(false);

    eventRoleWidget->updating = false;
    return true;
}

void MainWindow::deleteEventRoleWidget()
{
    delete eventRoleWidget;
    eventRoleWidget = nullptr;
}

void MainWindow::updateRole(const pa_ext_stream_restore_info &info)
{
    pa_cvolume volume;
    bool is_new = false;

    if (strcmp(info.name, "sink-input-by-media-role:event") != 0) {
        return;
    }

    is_new = createEventRoleWidget();

    eventRoleWidget->updating = true;

    eventRoleWidget->device = info.device ? info.device : "";

    volume.channels = 1;
    volume.values[0] = pa_cvolume_max(&info.volume);

    eventRoleWidget->setVolume(volume);
    eventRoleWidget->muteToggleButton->setChecked(info.mute);

    eventRoleWidget->updating = false;

    if (is_new) {
        updateDeviceVisibility();
    }
}

void MainWindow::updateDeviceInfo(const pa_ext_device_restore_info &info)
{
    if (sinkWidgets.count(info.index)) {
        SinkWidget *w;
        pa_format_info *format;

        w = sinkWidgets[info.index];

        w->updating = true;

        /* Unselect everything */
        for (int j = 1; j < PAVU_NUM_ENCODINGS; ++j) {
            w->encodings[j].widget->setChecked(false);
        }


        for (uint8_t i = 0; i < info.n_formats; ++i) {
            format = info.formats[i];

            for (int j = 1; j < PAVU_NUM_ENCODINGS; ++j) {
                if (format->encoding == w->encodings[j].encoding) {
                    w->encodings[j].widget->setChecked(true);
                    break;
                }
            }
        }

        w->updating = false;
    }
}


void MainWindow::updateVolumeMeter(uint32_t source_index, uint32_t sink_input_idx, double v)
{
    if (sink_input_idx != PA_INVALID_INDEX) {
        SinkInputWidget *w;

        if (sinkInputWidgets.count(sink_input_idx)) {
            w = sinkInputWidgets[sink_input_idx];
            w->updatePeak(v);
        }

    } else {

        for (const std::pair<uint32_t, SinkWidget*> &sinkWidget : sinkWidgets) {
            SinkWidget *w = sinkWidget.second;

            if (w->monitor_index == source_index) {
                w->updatePeak(v);
            }
        }

        for (const std::pair<uint32_t, SourceWidget*> &sourceWidget : sourceWidgets) {
            SourceWidget *w = sourceWidget.second;

            if (w->index == source_index) {
                w->updatePeak(v);
            }
        }

        for (const std::pair<uint32_t, SourceOutputWidget*> &sourceOutputWidget : sourceOutputWidgets) {
            SourceOutputWidget *w = sourceOutputWidget.second;

            if (w->sourceIndex() == source_index) {
                w->updatePeak(v);
            }
        }
    }
}

void MainWindow::setConnectionState(bool connected)
{
    if (m_connected != connected) {
        m_connected = connected;

        if (m_connected) {
            m_connectingLabel->hide();
            m_notebook->show();
        } else {
            m_notebook->hide();
            m_connectingLabel->show();
        }
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

    for (const std::pair<uint32_t, SinkInputWidget*> &sinkInputWidget : sinkInputWidgets) {
        SinkInputWidget *w = sinkInputWidget.second;

        if (sinkWidgets.size() > 1) {
            w->directionLabel->show();
            w->deviceButton->show();
        } else {
            w->directionLabel->hide();
            w->deviceButton->hide();
        }

        if (showSinkInputType == SINK_INPUT_ALL || w->type == showSinkInputType) {
            w->show();
            is_empty = false;
        } else {
            w->hide();
        }
    }

    if (eventRoleWidget) {
        is_empty = false;
    }

    if (is_empty) {
        m_noStreamsLabel->show();
    } else {
        m_noStreamsLabel->hide();
    }

    is_empty = true;

    for (const std::pair<uint32_t, SourceOutputWidget*> &sourceOutputWidget : sourceOutputWidgets) {
        SourceOutputWidget *w = sourceOutputWidget.second;

        if (sourceWidgets.size() > 1) {
            w->directionLabel->show();
            w->deviceButton->show();
        } else {
            w->directionLabel->hide();
            w->deviceButton->hide();
        }

        if (showSourceOutputType == SOURCE_OUTPUT_ALL || w->type == showSourceOutputType) {
            w->show();
            is_empty = false;
        } else {
            w->hide();
        }
    }

    if (is_empty) {
        m_noRecsLabel->show();
    } else {
        m_noRecsLabel->hide();
    }

    is_empty = true;

    for (const std::pair<uint32_t, SinkWidget*> &sinkWidget : sinkWidgets) {
        SinkWidget *w = sinkWidget.second;

        if (showSinkType == SINK_ALL || w->type == showSinkType) {
            w->show();
            is_empty = false;
        } else {
            w->hide();
        }
    }

    if (is_empty) {
        m_noSinksLabel->show();
    } else {
        m_noSinksLabel->hide();
    }

    is_empty = true;

    for (const std::pair<uint32_t, CardWidget*> &cardWidget : cardWidgets) {
        CardWidget *w = cardWidget.second;

        w->show();
        is_empty = false;
    }

    if (is_empty) {
        m_noCardsLabel->show();
    } else {
        m_noCardsLabel->hide();
    }

    is_empty = true;

    for (const std::pair<uint32_t, SourceWidget*> &sourceWidget : sourceWidgets) {
        SourceWidget *w = sourceWidget.second;

        if (showSourceType == SOURCE_ALL ||
                w->type == showSourceType ||
                (showSourceType == SOURCE_NO_MONITOR && w->type != SOURCE_MONITOR)) {
            w->show();
            is_empty = false;
        } else {
            w->hide();
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
    if (!cardWidgets.count(index)) {
        return;
    }

    delete cardWidgets[index];
    cardWidgets.erase(index);
    updateDeviceVisibility();
}

void MainWindow::removeSink(uint32_t index)
{
    if (!sinkWidgets.count(index)) {
        return;
    }

    delete sinkWidgets[index];
    sinkWidgets.erase(index);
    updateDeviceVisibility();
}

void MainWindow::removeSource(uint32_t index)
{
    if (!sourceWidgets.count(index)) {
        return;
    }

    delete sourceWidgets[index];
    sourceWidgets.erase(index);
    updateDeviceVisibility();
}

void MainWindow::removeSinkInput(uint32_t index)
{
    if (!sinkInputWidgets.count(index)) {
        return;
    }

    delete sinkInputWidgets[index];
    sinkInputWidgets.erase(index);
    updateDeviceVisibility();
}

void MainWindow::removeSourceOutput(uint32_t index)
{
    if (!sourceOutputWidgets.count(index)) {
        return;
    }

    delete sourceOutputWidgets[index];
    sourceOutputWidgets.erase(index);
    updateDeviceVisibility();
}

void MainWindow::removeClient(uint32_t index)
{
    clientNames.remove(index);
}

void MainWindow::removeAllWidgets()
{
    for (const std::pair<uint32_t, SinkInputWidget*> &sinkInputWidget : sinkInputWidgets) {
        removeSinkInput(sinkInputWidget.first);
    }

    for (const std::pair<uint32_t, SourceOutputWidget*> &sourceOutputWidget : sourceOutputWidgets) {
        removeSourceOutput(sourceOutputWidget.first);
    }

    for (const std::pair<uint32_t, SinkWidget*> &sinkWidget : sinkWidgets) {
        removeSink(sinkWidget.first);
    }

    for (const std::pair<uint32_t, SourceWidget*> &sourceWidget : sourceWidgets) {
        removeSource(sourceWidget.first);
    }

    for (const std::pair<uint32_t, CardWidget*> &cardWidget : cardWidgets) {
        removeCard(cardWidget.first);
    }

    clientNames.clear();
    deleteEventRoleWidget();
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

    showSinkType = (SinkType) m_sinkTypeComboBox->currentIndex();

    if (showSinkType == (SinkType) - 1) {
        m_sinkTypeComboBox->setCurrentIndex((int) SINK_ALL);
    }

    updateDeviceVisibility();
}

void MainWindow::onSourceTypeComboBoxChanged(int index)
{
    Q_UNUSED(index);

    showSourceType = (SourceType) m_sourceTypeComboBox->currentIndex();

    if (showSourceType == (SourceType) - 1) {
        m_sourceTypeComboBox->setCurrentIndex((int) SOURCE_NO_MONITOR);
    }

    updateDeviceVisibility();
}

void MainWindow::onSinkInputTypeComboBoxChanged(int index)
{
    Q_UNUSED(index);

    showSinkInputType = (SinkInputType) m_sinkInputTypeComboBox->currentIndex();

    if (showSinkInputType == (SinkInputType) - 1) {
        m_sinkInputTypeComboBox->setCurrentIndex((int) SINK_INPUT_CLIENT);
    }

    updateDeviceVisibility();
}

void MainWindow::onSourceOutputTypeComboBoxChanged(int index)
{
    Q_UNUSED(index);

    showSourceOutputType = (SourceOutputType) m_sourceOutputTypeComboBox->currentIndex();

    if (showSourceOutputType == (SourceOutputType) - 1) {
        m_sourceOutputTypeComboBox->setCurrentIndex((int) SOURCE_OUTPUT_CLIENT);
    }

    updateDeviceVisibility();
}


void MainWindow::onShowVolumeMetersCheckButtonToggled(bool toggled)
{
    Q_UNUSED(toggled);

    bool state = m_showVolumeMetersCheckButton->isChecked();
    pa_operation *o;

    for (const std::pair<uint32_t, SinkWidget*> &sinkWidget : sinkWidgets) {
        SinkWidget *sw = sinkWidget.second;

        if (sw->peak) {
            o = pa_stream_cork(sw->peak, (int)!state, nullptr, nullptr);

            if (o) {
                pa_operation_unref(o);
            }
        }

        sw->setVolumeMeterVisible(state);
    }

    for (const std::pair<uint32_t, SourceWidget*> &sourceWidget : sourceWidgets) {
        SourceWidget *sw = sourceWidget.second;

        if (sw->peak) {
            o = pa_stream_cork(sw->peak, (int)!state, nullptr, nullptr);

            if (o) {
                pa_operation_unref(o);
            }
        }

        sw->setVolumeMeterVisible(state);
    }

    for (const std::pair<uint32_t, SinkInputWidget*> &sinkInputWidget : sinkInputWidgets) {
        SinkInputWidget *sw = sinkInputWidget.second;

        if (sw->peak) {
            o = pa_stream_cork(sw->peak, (int)!state, nullptr, nullptr);

            if (o) {
                pa_operation_unref(o);
            }
        }

        sw->setVolumeMeterVisible(state);
    }

    for (const std::pair<uint32_t, SourceOutputWidget*> &sourceOutputWidget : sourceOutputWidgets) {
        SourceOutputWidget *sw = sourceOutputWidget.second;

        if (sw->peak) {
            o = pa_stream_cork(sw->peak, (int)!state, nullptr, nullptr);

            if (o) {
                pa_operation_unref(o);
            }
        }

        sw->setVolumeMeterVisible(state);
    }
}
