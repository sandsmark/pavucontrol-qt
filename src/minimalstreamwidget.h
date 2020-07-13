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

#ifndef minimalstreamwidget_h
#define minimalstreamwidget_h

#include "pavucontrol.h"
#include <QGroupBox>
#include <QFrame>

class QProgressBar;
class QLabel;
class QVBoxLayout;
class QHBoxLayout;
class QToolButton;

class MinimalStreamWidget : public QFrame//QGroupBox
{
    Q_OBJECT
public:
    MinimalStreamWidget(QWidget *parent = nullptr);
    void initPeakProgressBar(QVBoxLayout *channelsList);

    QProgressBar *peakProgressBar;
    double lastPeak;
    pa_stream *peak;

    bool updating;

    virtual void onMuteToggleButton() = 0;
    virtual void onLockToggleButton() = 0;
    virtual void updateChannelVolume(int channel, pa_volume_t v) = 0;

    bool volumeMeterEnabled;
    void enableVolumeMeter();
    void updatePeak(double v);
    void setVolumeMeterVisible(bool v);

    QVBoxLayout *channelsList;
    QLabel *iconImage;
    QLabel *boldNameLabel;
    QLabel *nameLabel;
    QVBoxLayout *mainLayout;
    QHBoxLayout *topLayout;
    QToolButton *muteToggleButton;
    QToolButton *lockToggleButton;

private :
    bool volumeMeterVisible;

};

#endif
