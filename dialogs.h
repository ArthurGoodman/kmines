/*
 * Copyright (c) 1996-2002 Nicolas HADACEK (hadacek@kde.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef DIALOGS_H
#define DIALOGS_H

#include <qpushbutton.h>

#include "generic/ghighscores.h"
#include "generic/gsettings.h"
#include "generic/gmisc_ui.h"

#include "defines.h"


class KComboBox;

//-----------------------------------------------------------------------------
class Smiley : public QPushButton, public KMines
{
 Q_OBJECT
 public:
    Smiley(QWidget *parent, const char *name = 0)
        : QPushButton(QString::null, parent, name) {}

 public slots:
    void setMood(Mood);

 private:
    static const char **XPM_NAMES[NbMoods];
};

//-----------------------------------------------------------------------------
class DigitalClock : public KGameLCDClock
{
 Q_OBJECT
 public:
    DigitalClock(QWidget *parent);

    void reset(bool customGame);

    bool cheating() const { return _cheating; }
    uint nbActions() const { return _nbActions; }
    KExtHighscores::Score score() const;

 public slots:
    void start();
    void setCheating();
    void addAction() { _nbActions++; }

 private slots:
    void timeoutClock();

 private:
    KExtHighscores::Score _first, _last;
    uint _nbActions;
    bool _cheating, _customGame;
};

//-----------------------------------------------------------------------------
class CustomConfig : public KConfigWidget, public KMines
{
 Q_OBJECT
 public:
    CustomConfig();

    static Level readLevel();

 private slots:
    void updateNbMines();
    void typeChosen(int);

 private:
    bool _block;
    KConfigItem *_width, *_height, *_mines;
    KComboBox   *_gameType;
};

//-----------------------------------------------------------------------------
class GameConfig : public KConfigWidget, public KMines
{
 Q_OBJECT
 public:
    GameConfig();

 private slots:
    void magicRevealToggled();

 private:
    KConfigItem *_magic;
};

class AppearanceConfig : public KConfigWidget, public KMines
{
 Q_OBJECT
 public:
    AppearanceConfig();
};

#endif
