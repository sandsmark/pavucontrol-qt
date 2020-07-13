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

class InputDeviceWidget : public DeviceWidget
{
    Q_OBJECT
public:
    InputDeviceWidget(MainWindow *parent);
    static InputDeviceWidget *create(MainWindow *mainWindow);

    InputDeviceType type;
    bool can_decibel;

    virtual void onMuteToggleButton();
    virtual void executeVolumeUpdate();
    virtual void onDefaultToggleButton();

protected:
    virtual void onPortChange();
};
