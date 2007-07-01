/*
 * Copyright (c) 1996-2002 Nicolas HADACEK (hadacek@kde.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "status.h"
#include "status.moc"

#include <qpainter.h>
#include <qpixmap.h>

#include <QLayout>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTimer>
#include <QGridLayout>
#include <QTextStream>
#include <QHBoxLayout>

#include <klocale.h>
#include <kconfig.h>
#include <kmessagebox.h>
#include <kaction.h>
#include <kdebug.h>
#include <kfiledialog.h>
#include <ktemporaryfile.h>
#include <kio/netaccess.h>
#include <KScoreDialog>
#include <kgameclock.h>
#include <knotification.h>

#include "settings.h"
#include "solver/solver.h"
#include "dialogs.h"
#include "version.h"


Status::Status(QWidget *parent)
  : QWidget(parent), _oldLevel(Level::Easy)
{
    setObjectName( "status" );
    _timer  = new QTimer(this);
    connect(_timer, SIGNAL(timeout()), SLOT(replayStep()));

    _solver = new Solver(this);
    connect(_solver, SIGNAL(solvingDone(bool)), SLOT(solvingDone(bool)));

// new grid layout
    QGridLayout *gridLayout = new QGridLayout(this);
    gridLayout->setSpacing(6);
    gridLayout->setMargin(9);

// top layout
    QHBoxLayout *top = new QHBoxLayout();
    top->setMargin( 0 );
    top->setSpacing( 6 );

// status bar
    QSpacerItem *spacerItem = new QSpacerItem(21, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    top->addItem(spacerItem);

	// smiley
	smiley = new Smiley(this);
	connect(smiley, SIGNAL(clicked()), SLOT(smileyClicked()));
	smiley->setFocusPolicy(Qt::NoFocus);
	smiley->setWhatsThis( i18n("Press to start a new game"));
    smiley->setIconSize( QSize(64,64) );

    /*QSizePolicy sizePolicy(static_cast<QSizePolicy::Policy>(5), static_cast<QSizePolicy::Policy>(5));
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(smiley->sizePolicy().hasHeightForWidth());
        smiley->setSizePolicy(sizePolicy);
        smiley->setMinimumSize(QSize(32, 32));
        smiley->setMaximumSize(QSize(64, 64));*/

        top->addWidget(smiley);

    QSpacerItem *spacerItem1 = new QSpacerItem(21, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    top->addItem(spacerItem1);

    // Game clock
    dg = new KGameClock(this, KGameClock::MinSecOnly);
    connect(dg, SIGNAL(timeChanged(const QString &)), SLOT(timeChanged(const QString &)));

    // mines field
    _fieldContainer = new QWidget;
    
    QHBoxLayout* fieldLayout = new QHBoxLayout(_fieldContainer);
    _field = new Field(_fieldContainer);
    _field->readSettings();
    fieldLayout->addWidget(_field);

	connect( _field, SIGNAL(updateStatus(bool)), SLOT(updateStatus(bool)) );
	connect(_field, SIGNAL(gameStateChanged(GameState)),
			SLOT(gameStateChangedSlot(GameState)) );
    connect(_field, SIGNAL(setMood(Mood)), smiley, SLOT(setMood(Mood)));
    connect(_field,SIGNAL(addAction(const KGrid2D::Coord &, Field::ActionType)),
            SLOT(addAction(const KGrid2D::Coord &, Field::ActionType)));
	_field->setWhatsThis( i18n("Mines field."));

// resume button
    _resumeContainer = new QWidget;
    QGridLayout *r = new QGridLayout(_resumeContainer);
    r->setSpacing(0);
    r->setMargin(0);
    QFont f = font();
    f.setBold(true);
    QPushButton *pb
        = new QPushButton(i18n("Press to Resume"), _resumeContainer);
    pb->setFont(f);
    connect(pb, SIGNAL(clicked()), SIGNAL(pause()));
    r->addWidget(pb, 0, 0, Qt::AlignCenter);

    _stack = new QStackedWidget (this);
    _stack->addWidget(_fieldContainer);
    _stack->addWidget(_resumeContainer);
    _stack->setCurrentWidget(_fieldContainer);

    //_stack->setSizePolicy(QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding));

    gridLayout->addWidget(_stack, 1, 0, 1, 1);
    gridLayout->addLayout(top, 0, 0, 1, 1);
}

void Status::resizeEvent ( QResizeEvent * event )
{
}

void Status::smileyClicked()
{
    if ( _field->gameState()==Paused ) emit pause();
    else restartGame();
}

void Status::newGame(int t)
{
    if ( _field->gameState()==Paused ) emit pause();
    Level::Type type = (Level::Type)t;
    Settings::setLevel(type);
    if ( type!=Level::Custom ) newGame( Level(type) );
    else newGame( Settings::customLevel() );
}

void Status::newGame(const Level &level)
{
	_timer->stop();
    /*if ( level.type()!=Level::Custom )
        KExtHighscore::setGameType(level.type());*/
    _field->setLevel(level);
}

bool Status::checkBlackMark()
{
    bool bm = ( _field->gameState()==Playing );
    //if (bm) KExtHighscore::submitScore(KExtHighscore::Lost, this);
    return bm;
}

void Status::restartGame()
{
    if ( _field->gameState()==Paused ) emit pause();
    else if ( _field->gameState()==Replaying ) {
        _timer->stop();
        _field->setLevel(_oldLevel);
    } else {
        bool bm = checkBlackMark();
        _field->reset(bm);
    }
}

void Status::settingsChanged()
{
    _field->readSettings();
    smiley->readSettings();

    if ( Settings::level()!=Level::Custom ) return;
    Level l = Settings::customLevel();
    if ( l==_field->level() ) return;
    if ( _field->gameState()==Paused ) emit pause();
    newGame(l);
}

void Status::updateStatus(bool mine)
{
    //int r = _field->nbMines() - _field->nbMarked();
    //QColor color = (r<0 && !_field->isSolved() ? Qt::red : Qt::white);
    QString minesStatus = QString("%1/%2").arg(_field->nbMarked()).arg(_field->nbMines());
    emit displayMinesLeft(minesStatus);

    if ( _field->isSolved() && !mine )
        gameStateChanged(GameOver, true); // ends only for wins
}

void Status::setGameOver(bool won)
{
    if ( !won )
      KNotification::event("explosion", i18n("Explosion!"), QPixmap() , this);
    _field->showAllMines(won);
    smiley->setMood(won ? Happy : Sad);
    if ( _field->gameState()==Replaying ) return;

    _field->setGameOver();
    dg->pause();

    if ( _field->level().type()!=Level::Custom && !_field->cheating() && won) {
        KScoreDialog ksdialog(KScoreDialog::Name | KScoreDialog::Time, this);
        switch(Settings::level())
        {
            case Level::Easy :
                ksdialog.setConfigGroup("Easy");
                break;
            case Level::Normal :
                ksdialog.setConfigGroup("Normal");
                break;
            case Level::Expert :
                ksdialog.setConfigGroup("Expert");
                break;
        }
        KScoreDialog::FieldInfo scoreInfo;
        scoreInfo[KScoreDialog::Score].setNum(dg->seconds());
        ksdialog.hideField(KScoreDialog::Score);
        scoreInfo[KScoreDialog::Time] = dg->timeString();
        if(ksdialog.addScore( scoreInfo, KScoreDialog::AskName | KScoreDialog::LessIsMore))
              ksdialog.exec();
    }
   
    KNotification::event(won ? "won" : "lost",
                         won ? i18n("Game won!") : i18n("Game lost!"),QPixmap() , this);

    // game log
    _logRoot.setAttribute("count", _field->nbActions());

    if ( Settings::magicReveal() )
        _logRoot.setAttribute("complete_reveal", "true");
    QString sa = "none";
    if ( _field->solvingState()==Solved ) sa = "solving";
    else if ( _field->solvingState()==Advised ) sa = "advising";
    _logRoot.setAttribute("solver", sa);

    QDomElement f = _log.createElement("Field");
    _logRoot.appendChild(f);
    QDomText data = _log.createTextNode(_field->string());
    f.appendChild(data);
}

void Status::setStopped()
{
    smiley->setMood(Normal);
    updateStatus(false);
    //bool custom = ( _field->level().type()==Level::Custom );
    //dg->reset(custom);
    _field->setCheating(false);
    _field->resetNbAction();
    _field->setSolvingState(Regular);
    dg->restart();
    dg->pause();
}

void Status::setPlaying()
{
    smiley->setMood(Normal);
    dg->resume();
    if ( _field->gameState()==Paused ) return; // do not restart game log...

    // game log
    const Level &level = _field->level();
    _log = QDomDocument("kmineslog");
    _logRoot = _log.createElement("kmineslog");
    _logRoot.setAttribute("version", VERSION);
    QDateTime date = QDateTime::currentDateTime();
    _logRoot.setAttribute("date", date.toString(Qt::ISODate));
    _logRoot.setAttribute("width", level.width());
    _logRoot.setAttribute("height", level.height());
    _logRoot.setAttribute("mines", level.nbMines());
    _log.appendChild(_logRoot);
    _logList = _log.createElement("ActionList");
    _logRoot.appendChild(_logList);
}

void Status::timeChanged(const QString & timestring)
{
    emit displayTime(timestring);
}

void Status::gameStateChanged(GameState state, bool won)
{
    QWidget *w = _fieldContainer;

    switch (state) {
    case Playing:
        setPlaying();
        break;
    case GameOver:
        setGameOver(won);
        break;
    case Paused:
        smiley->setMood(Sleeping);
        dg->pause();
        w = _resumeContainer;
        break;
    case Stopped:
    case Init:
        setStopped();
        break;
    case Replaying:
        smiley->setMood(Normal);
        break;
    case NB_STATES:
        Q_ASSERT(false);
        break;
    }

    _stack->setCurrentWidget(w);
    emit gameStateChangedSignal(state);
}

void Status::addAction(const KGrid2D::Coord &c, Field::ActionType type)
{
    QDomElement action = _log.createElement("Action");
    action.setAttribute("time", dg->timeString());
    action.setAttribute("column", c.first);
    action.setAttribute("line", c.second);
    action.setAttribute("type", Field::ACTION_DATA[type].name);
    _logList.appendChild(action);
    _field->addNbAction();
}

void Status::advise()
{
    int res = KMessageBox::warningContinueCancel(this,
               i18n("When the solver gives "
               "you advice, your score will not be added to the highscores."),
                QString(), KStandardGuiItem::cont(), KStandardGuiItem::cancel(), "advice_warning");
    if ( res==KMessageBox::Cancel ) return;
    _field->setCheating(true);
    float probability;
    KGrid2D::Coord c = _solver->advise(*_field, probability);
    _field->setAdvised(c, probability);
}

void Status::solve()
{
    _field->setCheating(true);
    _solver->solve(*_field, false);
	_field->setSolvingState(Solved);
}

void Status::solvingDone(bool success)
{
    if ( !success ) gameStateChanged(GameOver, false);
}

void Status::saveLog()
{
    KUrl url = KFileDialog::getSaveUrl( KUrl(), QString(), this);
    if ( url.isEmpty() ) return;
    if ( KIO::NetAccess::exists(url, false, this) ) {
        KGuiItem gi = KStandardGuiItem::save();
        gi.setText(i18n("Overwrite"));
        int res = KMessageBox::warningYesNo(this,
                                 i18n("The file already exists. Overwrite?"),
                                 i18n("File Exists"), gi, KStandardGuiItem::cancel());
        if ( res==KMessageBox::No ) return;
    }
    KTemporaryFile tmp;
    tmp.open();
    QTextStream stream(&tmp);
    stream << _log.toString();
    stream.flush();
    KIO::NetAccess::upload(tmp.fileName(), url, this);
}

void Status::loadLog()
{
    KUrl url = KFileDialog::getOpenUrl(KUrl(), QString(), this);
    if ( url.isEmpty() ) return;
    QString tmpFile;
    bool success = false;
    QDomDocument doc;
    if( KIO::NetAccess::download(url, tmpFile, this) ) {
        QFile file(tmpFile);
        if ( file.open(QIODevice::ReadOnly) ) {
            int errorLine;
            bool ok = doc.setContent(&file, 0, &errorLine);
            if ( !ok ) {
               KMessageBox::sorry(this, i18n("Cannot read XML file on line %1",
                                   errorLine));
               return;
            }
            success = true;
        }
        KIO::NetAccess::removeTempFile(tmpFile);

    }
    if ( !success ) {
        KMessageBox::sorry(this, i18n("Cannot load file."));
        return;
    }

    if ( !checkLog(doc) )
        KMessageBox::sorry(this, i18n("Log file not recognized."));
    else {
        _log = doc;
        _logRoot = doc.namedItem("kmineslog").toElement();
        emit gameStateChangedSignal(GameOver);
    }
}

bool Status::checkLog(const QDomDocument &doc)
{
    // check root element
    if ( doc.doctype().name()!="kmineslog" ) return false;
    QDomElement root = doc.namedItem("kmineslog").toElement();
    if ( root.isNull() ) return false;
    bool ok;
    uint w = root.attribute("width").toUInt(&ok);
    if ( !ok || w>CustomConfig::maxWidth || w<CustomConfig::minWidth )
        return false;
    uint h = root.attribute("height").toUInt(&ok);
    if ( !ok || h>CustomConfig::maxHeight || h<CustomConfig::minHeight )
        return false;
    uint nb = root.attribute("mines").toUInt(&ok);
    if ( !ok || nb==0 || nb>Level::maxNbMines(w, h) ) return false;

    // check field
    QDomElement field = root.namedItem("Field").toElement();
    if ( field.isNull() ) return false;
    QString ftext = field.text();
    if ( !BaseField::checkField(w, h, nb, ftext) ) return false;

    // check action list
    QDomElement list = root.namedItem("ActionList").toElement();
    if ( list.isNull() ) return false;
    QDomNodeList actions = list.elementsByTagName("Action");
    if ( actions.count()==0 ) return false;
    for (int i=0; i<actions.count(); i++) {
        QDomElement a = actions.item(i).toElement();
        if ( a.isNull() ) return false;
        uint i0 = a.attribute("line").toUInt(&ok);
        if ( !ok || i0>=h ) return false;
        uint j = a.attribute("column").toUInt(&ok);
        if ( !ok || j>=w ) return false;
        QString type = a.attribute("type");
        uint k = 0;
        for (; k<Field::Nb_Actions; k++)
            if ( type==Field::ACTION_DATA[k].name ) break;
        if ( k==Field::Nb_Actions ) return false;
    }

    return true;
}


void Status::replayLog()
{
    uint w = _logRoot.attribute("width").toUInt();
    uint h = _logRoot.attribute("height").toUInt();
    uint n = _logRoot.attribute("mines").toUInt();
    Level level(w, h, n);
    QDomNode f = _logRoot.namedItem("Field");
    _oldLevel = _field->level();
    newGame(level);
    _field->setReplayField(f.toElement().text());
    QString s = _logRoot.attribute("complete_reveal");
    _completeReveal = ( s=="true" );

    f = _logRoot.namedItem("ActionList");
    _actions = f.toElement().elementsByTagName("Action");
    _index = 0;
    _timer->start(500);
}

void Status::replayStep()
{
    if ( ( int )_index>=_actions.count() ) {
        _timer->stop();
        _actions = QDomNodeList();
        return;
    }

    _timer->start(200);
    QDomElement a = _actions.item(_index).toElement();
    dg->setTime(a.attribute("time"));
    uint i = a.attribute("column").toUInt();
    uint j = a.attribute("line").toUInt();
    QString type = a.attribute("type");
    for (uint k=0; k<Field::Nb_Actions; k++)
        if ( type==Field::ACTION_DATA[k].name ) {
            _field->doAction((Field::ActionType)k,
                            KGrid2D::Coord(i, j), _completeReveal);
            break;
        }
    _index++;
}
