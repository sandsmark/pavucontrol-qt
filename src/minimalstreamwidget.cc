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
#include "elidinglabel.h"

#include <QGridLayout>
#include <QProgressBar>
#include <QLabel>
#include <QDebug>
#include <QVBoxLayout>
#include <QToolButton>
#include <QStyle>
#include <QPropertyAnimation>

/*** MinimalStreamWidget ***/
MinimalStreamWidget::MinimalStreamWidget(QWidget *parent) :
    QFrame(parent),
    peak(nullptr),
    updating(false)
{
    setFrameShadow(QFrame::Raised);
    setFrameShape(QFrame::StyledPanel);
    mainLayout = new QVBoxLayout;
    setLayout(mainLayout);

    topLayout = new QHBoxLayout;
    mainLayout->addLayout(topLayout);

    iconImage = new QLabel;
    topLayout->addWidget(iconImage);

    boldNameLabel = new QLabel;
    topLayout->addWidget(boldNameLabel);

    nameLabel = new ElidingLabel(tr("Device Title"));
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

    m_peakProgressBar = new QProgressBar;
    m_peakProgressBar->setVisible(false);
    m_peakProgressBar->setTextVisible(false);
    m_peakProgressBar->setMaximumHeight(4 +
            2 * m_peakProgressBar->style()->pixelMetric(QStyle::PM_DefaultFrameWidth)
        );

    m_peakAnimation = new QPropertyAnimation(m_peakProgressBar, "value", m_peakProgressBar);
    m_peakAnimation->setDuration(100);
}

void MinimalStreamWidget::initPeakProgressBar(QVBoxLayout *channelsGrid)
{
    channelsGrid->addWidget(m_peakProgressBar);
}

void MinimalStreamWidget::updatePeak(double v)
{
    if (m_peakProgressBar->maximum() != m_peakProgressBar->width()) {
        m_peakProgressBar->setMaximum(m_peakProgressBar->width());
    }

    int value = qRound(v * m_peakProgressBar->maximum());
    m_peakAnimation->setEndValue(value);
    if (!m_peakAnimation->state() != QAbstractAnimation::Running) {
        m_peakAnimation->start();
    }
}

void MinimalStreamWidget::setVolumeMeterVisible(bool v)
{
    m_peakProgressBar->setVisible(v);
}
