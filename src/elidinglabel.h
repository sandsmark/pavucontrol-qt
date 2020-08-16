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

#pragma once

#include <QLabel>

class ElidingLabel : public QLabel {
  Q_OBJECT

public:
    explicit ElidingLabel(const QString &text, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

