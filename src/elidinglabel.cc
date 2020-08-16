/***
  This file is part of pavucontrol-qt.

  pavucontrol-qt is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  pavucontrol-qt is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with pavucontrol-qt. If not, see <https://www.gnu.org/licenses/>.
***/

#include "elidinglabel.h"
#include <QPainter>
#include <QStyleOption>

ElidingLabel::ElidingLabel(const QString &text, QWidget *parent):
    QLabel(text, parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    // set a min width to prevent the window from widening with long texts
    setMinimumWidth(fontMetrics().averageCharWidth() * 10);
}

// A simplified version of QLabel::paintEvent() without pixmap or shortcut but with eliding
void ElidingLabel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    QStyleOption opt;
    opt.initFrom(this);

    style()->drawItemText(&painter,
            opt.rect,
            alignment(),
            opt.palette,
            isEnabled(),
            opt.fontMetrics.elidedText(text(), Qt::ElideRight, opt.rect.width()),
            foregroundRole()
        );
}
