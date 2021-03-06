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

#ifndef cardwidget_h
#define cardwidget_h

#include "pavucontrol.h"
#include <QGroupBox>

class QLabel;
class QCheckBox;
class QComboBox;

class PortInfo
{
public:
    QByteArray name;
    QByteArray description;
    uint32_t priority;
    int available;
    int direction;
    int64_t latency_offset;
    std::vector<QByteArray> profiles;
};

class CardWidget : public QGroupBox
{
    Q_OBJECT
public:
    CardWidget(QWidget *parent = nullptr);

    QString name;
    uint32_t index;
    bool updating;

    std::vector< std::pair<QByteArray, QByteArray>> profiles;
    QHash<QByteArray, PortInfo> ports;
    QByteArray activeProfile;
    QByteArray noInOutProfile;
    QByteArray lastActiveProfile;
    bool hasOutputs = false;
    bool hasSources = false;

    QLabel *iconImage;
    QLabel *nameLabel;
    QCheckBox *profileCB;
    QComboBox *profileList;

    void prepareMenu();

protected:
    void changeProfile(const QByteArray &name);
    void onProfileChange(int active);
    void onProfileCheck(bool on);

};

#endif
