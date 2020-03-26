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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "sourceoutputwidget.h"
#include "mainwindow.h"
#include "sourcewidget.h"
#include <QMenu>

class SourceMenuItem : public QAction
{
    Q_OBJECT

public:
    SourceMenuItem(SourceOutputWidget *w
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

    SourceOutputWidget *widget;
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

SourceOutputWidget::SourceOutputWidget(MainWindow *parent) :
    StreamWidget(parent),
    menu{new QMenu{this}}
{

    directionLabel->setText(tr("<i>from</i>"));

    terminate->setText(tr("Terminate Recording"));

#if !HAVE_SOURCE_OUTPUT_VOLUMES
    /* Source Outputs do not have volume controls in versions of PA < 1.0 */
    muteToggleButton->hide();
    lockToggleButton->hide();
#endif
}

void SourceOutputWidget::setSourceIndex(uint32_t idx)
{
    mSourceIndex = idx;

    if (mpMainWindow->sourceWidgets.count(idx)) {
        SourceWidget *w = mpMainWindow->sourceWidgets[idx];
        deviceButton->setText(QString::fromUtf8(w->description));
    } else {
        deviceButton->setText(tr("Unknown input"));
    }
}

uint32_t SourceOutputWidget::sourceIndex()
{
    return mSourceIndex;
}

#if HAVE_SOURCE_OUTPUT_VOLUMES
void SourceOutputWidget::executeVolumeUpdate()
{
    pa_operation *o;

    if (!(o = pa_context_set_source_output_volume(get_context(), index, &volume, nullptr, nullptr))) {
        show_error(tr("pa_context_set_source_output_volume() failed").toUtf8().constData());
        return;
    }

    pa_operation_unref(o);
}

void SourceOutputWidget::onMuteToggleButton()
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
#endif

void SourceOutputWidget::onKill()
{
    pa_operation *o;

    if (!(o = pa_context_kill_source_output(get_context(), index, nullptr, nullptr))) {
        show_error(tr("pa_context_kill_source_output() failed").toUtf8().constData());
        return;
    }

    pa_operation_unref(o);
}


void SourceOutputWidget::buildMenu()
{
    for (const std::pair<uint32_t, SourceWidget*> &sourceWidget : mpMainWindow->sourceWidgets) {
        menu->addAction(new SourceMenuItem{this, sourceWidget.second->description.constData(), sourceWidget.second->index, sourceWidget.second->index == mSourceIndex, menu});
    }
}

void SourceOutputWidget::onDeviceChangePopup()
{
    menu->clear();
    buildMenu();
    menu->popup(QCursor::pos());
}

#include "sourceoutputwidget.moc"
