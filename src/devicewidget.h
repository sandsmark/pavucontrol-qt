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

#ifndef devicewidget_h
#define devicewidget_h

#include "pavucontrol.h"

#include "minimalstreamwidget.h"
#include <QTimer>
#include <vector>

class MainWindow;
class Channel;
class QAction;
class QSpinBox;
class QCheckBox;
class QComboBox;

class DeviceWidget : public MinimalStreamWidget
{
    Q_OBJECT
public:
    DeviceWidget(MainWindow *parent, const QByteArray &deviceType);

    void setChannelMap(const pa_channel_map &m, bool can_decibel);
    void setVolume(const pa_cvolume &volume, bool force = false);
    virtual void updateChannelVolume(int channel, pa_volume_t v);

    void hideLockedChannels(bool hide = true);

    QString name;
    QByteArray description;
    uint32_t index, card_index;

    bool offsetButtonEnabled;

    pa_channel_map channelMap;
    pa_cvolume volume;

    Channel *channels[PA_CHANNELS_MAX];

public Q_SLOTS:
    virtual void onMuteToggleButton();
    virtual void onLockToggleButton();
    virtual void onDefaultToggleButton();
    virtual void setDefault(bool isDefault);
    // virtual bool onContextTriggerEvent(GdkEventButton*);
    virtual void setLatencyOffset(int64_t offset);
    void onOffsetChange();
    bool timeoutEvent();

public:
    QTimer timeout;

    virtual void executeVolumeUpdate() = 0;
    virtual void setBaseVolume(pa_volume_t v);

    std::vector< std::pair<QByteArray, QByteArray>> ports;
    QByteArray activePort;
    QSpinBox *offsetButton;
    QToolButton *defaultToggleButton;

    QWidget *portSelect;
    QWidget *offsetSelect;
    QWidget *encodingSelect;

    QCheckBox *advancedOptions;
    QComboBox *portList;

    bool anyAvailablePorts = false;

    // TODO, this is just 1-1 from the .ui, can do smarter
    QCheckBox *encodingFormatPCM;
    QCheckBox *encodingFormatAC3;
    QCheckBox *encodingFormatEAC3;
    QCheckBox *encodingFormatDTS;
    QCheckBox *encodingFormatMPEG;
    QCheckBox *encodingFormatAAC;

    void prepareMenu();

    void renamePopup();

protected:
    MainWindow *mpMainWindow;

    virtual void onPortChange() = 0;

    QAction *rename;

private:
    QString mDeviceType;
    QWidget *advancedWidget;
};

#endif
