/*
 * Copyright (C) 2016 ~ 2017 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LOADINGITEM_H
#define LOADINGITEM_H

#include "settingsitem.h"
#include <QProgressBar>

namespace dcc {
namespace widgets {
class NormalLabel;
class LoadingIndicator;
}
}

namespace dcc {
namespace update {

class LoadingItem : public dcc::widgets::SettingsItem
{
    Q_OBJECT

public:
    explicit LoadingItem(QFrame* parent = 0);
    void setProgressValue(int value);
    void setProgressBarVisible(bool visible);
    void setMessage(const QString& message);

private:
    dcc::widgets::NormalLabel* m_messageLabel;
    QProgressBar *m_progress;
};

} // namespace widgets
} // namespace dcc

#endif // LOADINGITEM_H
