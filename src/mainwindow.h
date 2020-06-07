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

#ifndef mainwindow_h
#define mainwindow_h

#include "pavucontrol.h"
#include <pulse/ext-stream-restore.h>
#include <pulse/ext-device-restore.h>

#include <QWidget>
#include <QMap>
//#include "ui_mainwindow.h"

class CardWidget;
class SinkWidget;
class SourceWidget;
class SinkInputWidget;
class SourceOutputWidget;
class RoleWidget;

class QLabel;
class QComboBox;
class QCheckBox;
class QTabWidget;
class WavPlay;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent);
    virtual ~MainWindow();

    void updateCard(const pa_card_info &info);
    bool updateSink(const pa_sink_info &info);
    void updateSource(const pa_source_info &info);
    void updateSinkInput(const pa_sink_input_info &info);
    void updateSourceOutput(const pa_source_output_info &info);
    void updateClient(const pa_client_info &info);
    void updateServer(const pa_server_info &info);
    void updateVolumeMeter(uint32_t source_index, uint32_t sink_input_index, double v);
    void updateRole(const pa_ext_stream_restore_info &info);
    void updateDeviceInfo(const pa_ext_device_restore_info &info);

    void removeCard(uint32_t index);
    void removeSink(uint32_t index);
    void removeSource(uint32_t index);
    void removeSinkInput(uint32_t index);
    void removeSourceOutput(uint32_t index);
    void removeClient(uint32_t index);

    void removeAllWidgets();

    void setConnectingMessage(const char *string = nullptr);

    std::map<uint32_t, CardWidget *> m_cardWidgets;
    std::map<uint32_t, SinkWidget *> m_sinkWidgets;
    std::map<uint32_t, SourceWidget *> m_sourceWidgets;
    std::map<uint32_t, SinkInputWidget *> m_sinkInputWidgets;
    std::map<uint32_t, SourceOutputWidget *> m_sourceOutputWidgets;

    QMap<int, QString> m_clientNames;
    SinkInputType m_showSinkInputType;
    SinkType m_showSinkType;
    SourceOutputType m_showSourceOutputType;
    SourceType m_showSourceType;

protected Q_SLOTS:
    virtual void onSinkInputTypeComboBoxChanged(int index);
    virtual void onSourceOutputTypeComboBoxChanged(int index);
    virtual void onSinkTypeComboBoxChanged(int index);
    virtual void onSourceTypeComboBoxChanged(int index);
    virtual void onShowVolumeMetersCheckButtonToggled(bool toggled);

public:
    void setConnectionState(bool connected);
    void updateDeviceVisibility();
    void reallyUpdateDeviceVisibility();
    pa_stream *createMonitorStreamForSource(uint32_t source_idx, uint32_t stream_idx, bool suspend);
    void createMonitorStreamForSinkInput(SinkInputWidget *sinkInputWidget, uint32_t sink_idx);

    RoleWidget *m_eventRoleWidget = nullptr;

    bool createEventRoleWidget();
    void deleteEventRoleWidget();

    QByteArray m_defaultSinkName, m_defaultSourceName;

    bool m_canRenameDevices = false;

    QTabWidget *m_notebook; // todo: pull in necessary code from pavucontrol.cc

private:
    int iconSize();

    static void setIconByName(QLabel *label, const QByteArray &name, const QByteArray &fallback);

    bool m_connected;
    char *m_config_filename;

    // UI elements
    QComboBox *m_sinkInputTypeComboBox;
    QComboBox *m_sourceOutputTypeComboBox;
    QComboBox *m_sinkTypeComboBox;
    QComboBox *m_sourceTypeComboBox ;
    QCheckBox *m_showVolumeMetersCheckButton;

    QLabel *m_connectingLabel;
    QLabel *m_noStreamsLabel;
    QLabel *m_noRecsLabel;
    QLabel *m_noSinksLabel;
    QLabel *m_noSourcesLabel;
    QLabel *m_noCardsLabel;

    QWidget *m_cardsVBox;
    QWidget *m_sinksVBox;
    QWidget *m_sourcesVBox;
    QWidget *m_streamsVBox;
    QWidget *m_recsVBox;

    WavPlay *m_popPlayer;
};


#endif
