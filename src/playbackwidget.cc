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

#include "playbackwidget.h"

#include "mainwindow.h"
#include "outputwidget.h"

#include <QMenu>
#include <QLabel>
#include <QToolButton>
#include <QDebug>


class SinkMenuItem : public QAction
{
    Q_OBJECT

public:
    SinkMenuItem(PlaybackWidget *w
            , const char *label
            , uint32_t i
            , bool active
            , QObject *parent = nullptr)
        : QAction(QString::fromUtf8(label), parent)
          , widget(w)
          , index(i)
    {
        setCheckable(true);
        setChecked(active);
        connect(this, &QAction::toggled, [this] { onToggle(); });
    }

    PlaybackWidget *widget;
    uint32_t index;

public slots:
    void onToggle() {
        if (widget->updating) {
            return;
        }

        if (!isChecked()) {
            return;
        }

        /*if (!mpMainWindow->sinkWidgets.count(widget->index))
          return;*/

        pa_operation *o;

        if (!(o = pa_context_move_sink_input_by_index(get_context(), widget->index, index, nullptr, nullptr))) {
            show_error(tr("pa_context_move_sink_input_by_index() failed").toUtf8().constData());
            return;
        }

        pa_operation_unref(o);
    }
};

PlaybackWidget::PlaybackWidget(MainWindow *parent) :
    StreamWidget(parent),
    menu{new QMenu{this}}
{

    directionLabel->setText(tr("<i>on</i>"));

    terminate->setText(tr("Terminate Playback"));
}

void PlaybackWidget::setPlaybackIndex(uint32_t idx)
{
    mSinkIndex = idx;

    if (mpMainWindow->m_outputWidgets.count(idx)) {
        OutputWidget *w = mpMainWindow->m_outputWidgets[idx];
        deviceButton->setText(QString::fromUtf8(w->description));
    } else {
        deviceButton->setText(tr("Unknown output"));
    }
}

uint32_t PlaybackWidget::playbackIndex()
{
    return mSinkIndex;
}

static void onVolumeChanged(pa_context *c, int success, void *userdata)
{
    Q_UNUSED(c);
    if (!success) {
        qWarning() << "Volume change failed";
        return;
    }

    PlaybackWidget *that = reinterpret_cast<PlaybackWidget*>(userdata);
    emit that->requestBop(that->mSinkIndex, that->maxVolume);
}

void PlaybackWidget::executeVolumeUpdate()
{
    pa_operation *o;

    maxVolume = pa_cvolume_max(&volume);;

    if (!(o = pa_context_set_sink_input_volume(get_context(), index, &volume, onVolumeChanged, this))) {
        show_error(tr("pa_context_set_sink_input_volume() failed").toUtf8().constData());
        return;
    }

    pa_operation_unref(o);
}

void PlaybackWidget::onMuteToggleButton()
{
    StreamWidget::onMuteToggleButton();

    if (updating) {
        return;
    }

    pa_operation *o;

    if (!(o = pa_context_set_sink_input_mute(get_context(), index, muteToggleButton->isChecked(), nullptr, nullptr))) {
        show_error(tr("pa_context_set_sink_input_mute() failed").toUtf8().constData());
        return;
    }

    pa_operation_unref(o);
}

void PlaybackWidget::onKill()
{
    pa_operation *o;

    if (!(o = pa_context_kill_sink_input(get_context(), index, nullptr, nullptr))) {
        show_error(tr("pa_context_kill_sink_input() failed").toUtf8().constData());
        return;
    }

    pa_operation_unref(o);
}

void PlaybackWidget::buildMenu()
{
    for (OutputWidget *sinkWidget : mpMainWindow->m_outputWidgets) {
        menu->addAction(new SinkMenuItem{this, sinkWidget->description.constData(), sinkWidget->index, sinkWidget->index == mSinkIndex, menu});
    }
}

void PlaybackWidget::onDeviceChangePopup()
{
    menu->clear();
    buildMenu();
    menu->popup(QCursor::pos());
}

#include "playbackwidget.moc"
