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

#include "recordingwidget.h"
#include "mainwindow.h"
#include "inputdevicewidget.h"
#include <QMenu>
#include <QLabel>
#include <QToolButton>

class RecordingMenuItem : public QAction
{
    Q_OBJECT

public:
    RecordingMenuItem(RecordingWidget *w
                   , const char *label
                   , uint32_t i
                   , bool active
                   , QObject *parent = nullptr)
        : QAction{QString::fromUtf8(label), parent}
        , widget(w)
        , index(i)
    {
        setCheckable(true);
        setChecked(active);
        connect(this, &QAction::toggled, [this] { onToggle(); });
    }

    RecordingWidget *widget;
    uint32_t index;

public slots:
    void onToggle()
    {
        if (widget->updating) {
            return;
        }

        if (!isChecked()) {
            return;
        }

        /*if (!mpMainWindow->sourceWidgets.count(widget->index))
          return;*/

        pa_operation *o;

        if (!(o = pa_context_move_source_output_by_index(get_context(), widget->index, index, nullptr, nullptr))) {
            show_error(tr("pa_context_move_source_output_by_index() failed").toUtf8().constData());
            return;
        }

        pa_operation_unref(o);
    }
};

RecordingWidget::RecordingWidget(MainWindow *parent) :
    StreamWidget(parent),
    menu{new QMenu{this}}
{

    directionLabel->setText(tr("<i>from</i>"));

    terminate->setText(tr("Terminate Recording"));
}

void RecordingWidget::setSourceIndex(uint32_t idx)
{
    mSourceIndex = idx;

    if (mpMainWindow->m_inputDeviceWidgets.count(idx)) {
        InputDeviceWidget *w = mpMainWindow->m_inputDeviceWidgets[idx];
        deviceButton->setText(QString::fromUtf8(w->description));
    } else {
        deviceButton->setText(tr("Unknown input"));
    }
}

uint32_t RecordingWidget::sourceIndex()
{
    return mSourceIndex;
}

void RecordingWidget::executeVolumeUpdate()
{
    pa_operation *o;

    if (!(o = pa_context_set_source_output_volume(get_context(), index, &volume, nullptr, nullptr))) {
        show_error(tr("pa_context_set_source_output_volume() failed").toUtf8().constData());
        return;
    }

    pa_operation_unref(o);
}

void RecordingWidget::onMuteToggleButton()
{
    StreamWidget::onMuteToggleButton();

    if (updating) {
        return;
    }

    pa_operation *o;

    if (!(o = pa_context_set_source_output_mute(get_context(), index, muteToggleButton->isChecked(), nullptr, nullptr))) {
        show_error(tr("pa_context_set_source_output_mute() failed").toUtf8().constData());
        return;
    }

    pa_operation_unref(o);
}

void RecordingWidget::onKill()
{
    pa_operation *o;

    if (!(o = pa_context_kill_source_output(get_context(), index, nullptr, nullptr))) {
        show_error(tr("pa_context_kill_source_output() failed").toUtf8().constData());
        return;
    }

    pa_operation_unref(o);
}


void RecordingWidget::buildMenu()
{
    for (const std::pair<const uint32_t, InputDeviceWidget*> &sourceWidget : mpMainWindow->m_inputDeviceWidgets) {
        menu->addAction(new RecordingMenuItem{this, sourceWidget.second->description.constData(), sourceWidget.second->index, sourceWidget.second->index == mSourceIndex, menu});
    }
}

void RecordingWidget::onDeviceChangePopup()
{
    menu->clear();
    buildMenu();
    menu->popup(QCursor::pos());
}

#include "recordingwidget.moc"
