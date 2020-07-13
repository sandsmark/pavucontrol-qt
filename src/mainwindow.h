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
class OutputWidget;
class InputDeviceWidget;
class PlaybackWidget;
class RecordingWidget;
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
    bool updateOutputWidget(const pa_sink_info &info);
    void updateInputDeviceWidget(const pa_source_info &info);
    void updatePlaybackWidget(const pa_sink_input_info &info);
    void updateRecordingWidget(const pa_source_output_info &info);
    void updateClient(const pa_client_info &info);
    void updateServer(const pa_server_info &info);
    void updateVolumeMeter(uint32_t source_index, uint32_t sink_input_index, double v);
    void updateRole(const pa_ext_stream_restore_info &info);
    void updateDeviceInfo(const pa_ext_device_restore_info &info);

    void removeCard(uint32_t index);
    void removeOutputWidget(uint32_t index);
    void removeInputDevice(uint32_t index);
    void removePlaybackWidget(uint32_t index);
    void removeRecordingWidget(uint32_t index);
    void removeClient(uint32_t index);

    void removeAllWidgets();

    void setConnectingMessage(const char *string = nullptr);

    std::map<uint32_t, CardWidget *> m_cardWidgets;
    std::map<uint32_t, OutputWidget *> m_outputWidgets;
    std::map<uint32_t, InputDeviceWidget *> m_inputDeviceWidgets;
    std::map<uint32_t, PlaybackWidget *> m_playbackWidgets;
    std::map<uint32_t, RecordingWidget *> m_recordingWidgets;

    QMap<int, QString> m_clientNames;
    PlaybackType m_showPlaybackType;
    OutputType m_showOutputType;
    RecordingType m_showRecordingType;
    InputDeviceType m_showInputDeviceType;

protected Q_SLOTS:
    virtual void onPlaybackTypeComboBoxChanged(int index);
    virtual void onRecordingTypeComboBoxChanged(int index);
    virtual void onOutputTypeComboBoxChanged(int index);
    virtual void onInputDeviceTypeComboBoxChanged(int index);
    virtual void onShowVolumeMetersCheckButtonToggled(bool toggled);

public:
    void setConnectionState(bool connected);
    void updateDeviceVisibility();
    void reallyUpdateDeviceVisibility();
    pa_stream *createMonitorStreamForSource(uint32_t source_idx, uint32_t stream_idx, bool suspend);
    void createMonitorStreamForPlayback(PlaybackWidget *playbackWidget, uint32_t sink_idx);

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
    QComboBox *m_playbackTypeComboBox;
    QComboBox *m_recordingTypeComboBox;
    QComboBox *m_outputTypeComboBox;
    QComboBox *m_inputDeviceTypeComboBox ;
    QCheckBox *m_showVolumeMetersCheckButton;

    QLabel *m_connectingLabel;
    QLabel *m_noStreamsLabel;
    QLabel *m_noRecsLabel;
    QLabel *m_noOutputsLabel;
    QLabel *m_noInputDevicesLabel;
    QLabel *m_noCardsLabel;

    QWidget *m_cardsVBox;
    QWidget *m_outputsVBox;
    QWidget *m_inputDevicesVBox;
    QWidget *m_streamsVBox;
    QWidget *m_recsVBox;

    WavPlay *m_popPlayer;
};


#endif
