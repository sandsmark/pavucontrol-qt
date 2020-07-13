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

#pragma once

#include "pavucontrol.h"
#include "devicewidget.h"


#include <pulse/format.h>

#define PAVU_NUM_ENCODINGS 6

class QCheckBox;

typedef struct {
    pa_encoding encoding;
    QCheckBox *widget;
} encodingList;

class OutputWidget : public DeviceWidget
{
    Q_OBJECT
public:
    OutputWidget(MainWindow *parent);

    OutputType type;
    uint32_t monitor_index;
    bool can_decibel;

    encodingList encodings[PAVU_NUM_ENCODINGS];

    void onMuteToggleButton() override;
    void executeVolumeUpdate() override;
    void onDefaultToggleButton() override;
    void setDigital(bool);

protected Q_SLOTS:
    void onPortChange() override;
    void onEncodingsChange();
};
