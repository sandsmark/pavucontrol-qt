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

#include "minimalstreamwidget.h"

#include <QGridLayout>
#include <QProgressBar>
#include <QLabel>
#include <QDebug>
#include <QVBoxLayout>
#include <QToolButton>

/*** MinimalStreamWidget ***/
MinimalStreamWidget::MinimalStreamWidget(QWidget *parent) :
    QWidget(parent),
    lastPeak(0),
    peak(nullptr),
    updating(false),
    volumeMeterEnabled(false),
    volumeMeterVisible(true)
{
    mainLayout = new QVBoxLayout;
    setLayout(mainLayout);

    topLayout = new QHBoxLayout;
    mainLayout->addLayout(topLayout);

    iconImage = new QLabel;
    topLayout->addWidget(iconImage);

    boldNameLabel = new QLabel;
    topLayout->addWidget(boldNameLabel);

    nameLabel = new QLabel(tr("Device Title"));
    topLayout->addWidget(nameLabel);

    topLayout->addStretch();

    muteToggleButton = new QToolButton;
    topLayout->addWidget(muteToggleButton);
    muteToggleButton->setToolTip(tr("Mute audio"));
    muteToggleButton->setCheckable(true);
    muteToggleButton->setIcon(QIcon::fromTheme("audio-volume-muted"));

    lockToggleButton = new QToolButton;
    lockToggleButton->setToolTip(tr("Lock channels together"));
    lockToggleButton->setIcon(QIcon::fromTheme("changes-prevent-symbolic"));
    lockToggleButton->setCheckable(true);
    lockToggleButton->setChecked(true);
    topLayout->addWidget(lockToggleButton);

    channelsList = new QVBoxLayout;
    mainLayout->addLayout(channelsList);

    peakProgressBar = new QProgressBar;
    peakProgressBar->setTextVisible(false);
    peakProgressBar->setMaximumHeight(4 /* FIXME: hardcoded */);
    peakProgressBar->hide();
}

void MinimalStreamWidget::initPeakProgressBar(QVBoxLayout *channelsGrid)
{
    channelsGrid->addWidget(peakProgressBar);
//    channelsGrid->addWidget(peakProgressBar, channelsGrid->rowCount(), 0, 1, -1);
}

#define DECAY_STEP .04

void MinimalStreamWidget::updatePeak(double v)
{

    if (lastPeak >= DECAY_STEP)
        if (v < lastPeak - DECAY_STEP) {
            v = lastPeak - DECAY_STEP;
        }

    lastPeak = v;

    if (v >= 0) {
        peakProgressBar->setEnabled(true);
        int value = qRound(v * peakProgressBar->maximum());
        peakProgressBar->setValue(value);
    } else {
        peakProgressBar->setEnabled(false);
        peakProgressBar->setValue(0);
    }

    enableVolumeMeter();
}

void MinimalStreamWidget::enableVolumeMeter()
{
    if (volumeMeterEnabled) {
        return;
    }

    volumeMeterEnabled = true;

    if (volumeMeterVisible) {
        peakProgressBar->show();
    }
}

void MinimalStreamWidget::setVolumeMeterVisible(bool v)
{
    volumeMeterVisible = v;

    if (v) {
        if (volumeMeterEnabled) {
            peakProgressBar->show();
        }
    } else {
        peakProgressBar->hide();
    }
}
