/*!
	Copyright (c) 2007-2010, Matevž Jekovec, Canorus development team
	All Rights Reserved. See AUTHORS for a complete list of authors.

	Licensed under the GNU GENERAL PUBLIC LICENSE. See LICENSE.GPL for details.
*/

#include <QRegExp>
#include <QList>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>

#include "export/lilypondexport.h"

#include "score/document.h"
#include "score/sheet.h"
#include "score/staff.h"
#include "score/voice.h"
#include "score/mark.h"
#include "score/articulation.h"
#include "score/fingering.h"
#include "score/dynamic.h"
#include "score/text.h"
#include "score/tempo.h"
#include "score/ritardando.h"
#include "score/tuplet.h"
#include "score/barline.h"
#include "score/repeatmark.h"

/*!
 * Helper class to store scan results for repeat syntax 
 */
class TimeRange {
public:
	int start;
	int end;
	TimeRange( int s, int e );
	virtual ~TimeRange();
};
TimeRange::TimeRange( int s, int e ) {
	start = s;
	end = e;
}

TimeRange::~TimeRange(){
}


class CARepeat {
public:
	CARepeat( int timeStart, int timeEnd, int voltaNumber );
	virtual ~CARepeat();
	QPair<int,int> time;
	int voltaNumber;
	QVector<TimeRange*> alternatives;	// start and end times of alternatives
	bool closePreviousEventually( CARepeat* r, int time );
	QPair<int,int> pointer;
	QString outString(QVector<CARepeat*> *r, int time);
private:
	int _matchTime;
	int _repIdx;
	int _altIdx;
};

QString CARepeat::outString( QVector<CARepeat*> *r, int time ) {

	if (time == 0) {
		_repIdx = 0;
		_altIdx = 0;
		if (r->size()== 0) {
			_matchTime = 0;
			return QString("");
		}
		_matchTime = (*r)[_repIdx]->time.first;
		if (time == _matchTime) {
			return QString(" \repeat volta 2 { " );
		}
	}

	// if (time==_matchTime


	return QString("hallo");
}

CARepeat::CARepeat( int start, int end, int v = 0 ) {
	time.first = start;
	time.second = end;
	voltaNumber = v;
	alternatives.clear();
	_repIdx = 0;
	_altIdx = 0;
}

CARepeat::~CARepeat(){
}

bool CARepeat::closePreviousEventually( CARepeat* r, int currentTime ) {
	int a = (*r).alternatives.size();
	switch (a) {
	case 0:
			if ((r->time.second == 0) && (r->time.first != currentTime) ) {
				r->time.second = currentTime;
				return true;
			}
			return false;
	default:
			int l = a-1;
			if (((r->alternatives[l])->end == 0) && (r->alternatives[l]->start != currentTime)) {
				r->alternatives[l]->end = currentTime;
				return true;
			}
			return false;
	}
}

/*!
	\class CALilyPondExport
	\brief LilyPond export filter
	This class is used to export the document or parts of the document to LilyPond syntax.
	The most common use is to simply call one of the constructors
	\code
	CALilyPondExport( myDocument, &textStream );
	\endcode

	\a textStream is usually the file stream or the content of the score source view widget.

	\sa CALilyPondImport
*/

/*!
	Constructor for the lilypond export. Called when viewing a single voice source in
	Lily syntax or exporting the whole document to a file.

	Uses \a out for the output.
*/
CALilyPondExport::CALilyPondExport( QTextStream *out )
 : CAExport(out) {
	setIndentLevel( 0 );
	setCurDocument( 0 );
	_voltaFunctionWritten = false;
	_voltaBracketFinishAtRepeat = false;
	_voltaBracketFinishAtBar = false;
	_timeSignatureFound = false;
}

/*!
	Exports the given voice music elements to LilyPond syntax.

	\sa CALilypondImport
*/
void CALilyPondExport::exportVoiceImpl(CAVoice *v) {
	setCurVoice(v);

	_curStreamTime = 0;
	_lastPlayableLength = CAPlayableLength::Undefined;
	bool anacrusisCheck = true;	// process upbeat eventually
	CATimeSignature *time = 0;
	int barNumber = 1;

	bool searchForRepeatEvents = true;
	_repeats.clear();

	// Write \relative note for the first note
	_lastNotePitch = writeRelativeIntro();

	// start of the voice block
	out() << "{\n";
	indentMore();
	indent();
	QList<QPair<int,QString>> oStr;

	for (int i=0; i<v->musElementList().size(); i++, out() << " ") { // append blank after each element
		// 
		// Check if a repeat opening is neccessary.
		// If yes, output \repeat volta x ...
		// In any case set after search searchForRepeatEvents false:
		if ( searchForRepeatEvents ) {

			for (int r=i; r<v->musElementList().size(); r++ ) {
				switch (v->musElementList()[r]->musElementType()) {
					case CAMusElement::Barline:	{
						int time = v->musElementList()[r]->timeStart();
						CABarline *bbar = static_cast<CABarline*>(v->musElementList()[r]);
						if (bbar->barlineType() == CABarline::RepeatOpen) {
							_repeats << new CARepeat( time, 0, 2 );			// new repeat with timeEnd unknown
						}
						if (bbar->barlineType() == CABarline::RepeatClose) {
							qWarning() << " RepeatClose at Element " << r << endl;
							if (_repeats.size() == 0) {
								_repeats << new CARepeat( 0, time, 2 );
								qWarning() << " neuer Repeat bei t=" << time;
							} else if (_repeats.last()->time.second == 0 ) {
								_repeats.last()->time.second = time;
								_repeats.last()->closePreviousEventually( _repeats.last(), time );	// maybe an alternative is open
							}
						}
						if (bbar->barlineType() == CABarline::RepeatCloseOpen) {
							if (_repeats.size() == 0) {
								_repeats << new CARepeat( 0, time, 2 );
							}
							qWarning() << " neuer Repeat bei t=" << time;
							_repeats << new CARepeat( time, 0, 2 );
						}
						for (int g=0; g<bbar->markList().size();g++) {
							if (bbar->markList()[g]->markType()==CAMark::RepeatMark) {
								CARepeatMark *rm = static_cast<CARepeatMark*>(bbar->markList()[g]);
								if (bbar->markList()[g]->markType()==CAMark::RepeatMark) {
									qWarning() << " found volta number " << rm->voltaNumber();
									if (_repeats.size() == 0) {
										qWarning() << " Repeat:  neue Alternative bei t=" << time;
										_repeats.last()->closePreviousEventually( _repeats.last(), time );	// maybe an alternative is open
										_repeats << new CARepeat( 0, time, 2 );
										_repeats.last()->alternatives << new TimeRange( time, 0 );
										_repeats.last()->voltaNumber = rm->voltaNumber();
									} else {
										_repeats.last()->closePreviousEventually( _repeats.last(), time );	// maybe an alternative is open
										if (_repeats.last()->alternatives.size()) {
										_repeats.last()->closePreviousEventually( _repeats.last(), time );	// maybe an alternative is open
										}
										_repeats.last()->alternatives << new TimeRange( time, 0 );
										if (_repeats.last()->alternatives.size()) {
										_repeats.last()->closePreviousEventually( _repeats.last(), time );	// maybe an alternative is open
										}
										_repeats.last()->voltaNumber = rm->voltaNumber();
									}
								}
							}
						}
						_repeats.last()->closePreviousEventually( _repeats.last(), time );	// maybe an alternative is open
						break;
					}
					default:			break;
				}
			}
			// The search is done once. When
			searchForRepeatEvents = false;

			for (int r=0;r<_repeats.size();r++) {
				oStr.append(qMakePair( _repeats[r]->time.first, QString(" \repeat volta 2 { ")));
				int aSize = _repeats[r]->alternatives.size();
				switch (aSize) {
				case 0:		oStr.append(qMakePair( _repeats[r]->time.second, QString(        " } ")));
							break;
				case 1:		oStr.append(qMakePair( (((_repeats[r])->alternatives)[0])->start, _altOpening ));
							oStr.append(qMakePair( (((_repeats[r])->alternatives)[0])->end,   QString( " } ")));
							break;
				default:	for (int a=0;a<aSize;a++) {
								if (a==0) {
									oStr.append(qMakePair( (((_repeats[r])->alternatives)[a])->start, _altOpening ));
								} else if (a==(aSize-1)) {
									oStr.append(qMakePair( (((_repeats[r])->alternatives)[a])->start, QString(              " } { ")));
									oStr.append(qMakePair( (((_repeats[r])->alternatives)[a])->end,   QString(              " } } ")));
								} else {
									oStr.append(qMakePair( (((_repeats[r])->alternatives)[a])->start, QString(              " } { ")));
								}
							}
				}
			}
		}


		///////////////////////////////////////////////////////////////
		// (CAMusElement)
		switch (v->musElementList()[i]->musElementType()) {
		case CAMusElement::Clef: {
			// CAClef
			CAClef *clef = static_cast<CAClef*>(v->musElementList()[i]);
			if (clef->timeStart()!=_curStreamTime) break;	//! \todo If the time isn't the same, insert hidden rests to fill the needed time
			out() << "\\clef \"";
			out() << clefTypeToLilyPond( clef->clefType(), clef->c1(), clef->offset() );
			out() << "\"";

			break;
		}
		case CAMusElement::KeySignature: {
			// CAKeySignature
			CAKeySignature *key = static_cast<CAKeySignature*>(v->musElementList()[i]);
			if (key->timeStart()!=_curStreamTime) break;	//! \todo If the time isn't the same, insert hidden rests to fill the needed time
			out() << "\\key "
				<< diatonicPitchToLilyPond( key->diatonicKey().diatonicPitch() ) << " "
				<< diatonicKeyGenderToLilyPond( key->diatonicKey().gender() );

			break;
		}
		case CAMusElement::TimeSignature: {
			// CATimeSignature, remember for anacrusis processing
			time = static_cast<CATimeSignature*>(v->musElementList()[i]);
			if (time->timeStart()!=_curStreamTime) break;	//! \todo If the time isn't the same, insert hidden rests to fill the needed time
			out() << "\\time " << time->beats() << "/" << time->beat();
			// set this flag to allow the time signature engraver
			_timeSignatureFound = true;

			break;
		}
		case CAMusElement::Barline: {
			// CABarline
			CABarline *bar = static_cast<CABarline*>(v->musElementList()[i]);
			if (bar->timeStart()!=_curStreamTime) break;	//! \todo If the time isn't the same, insert hidden rests to fill the needed time

			if ( _voltaBracketFinishAtRepeat &&
					(bar->barlineType() == CABarline::RepeatClose || bar->barlineType() == CABarline::RepeatCloseOpen)) {
				out() << " \\set Score.repeatCommands = #'((volta #f))  ";
				_voltaBracketFinishAtRepeat = false;
			}
			if ( _voltaBracketFinishAtBar ) {
				out() << " \\set Score.repeatCommands = #'((volta #f))  ";
				_voltaBracketFinishAtBar = false;
			}

			if (bar->barlineType() == CABarline::Single)
				out() << "| % bar " << barNumber << "\n	";
			else
				out() << "\\bar \"" << barlineTypeToLilyPond(bar->barlineType()) << "\"" << " % bar "<< barNumber << "\n	";
			barNumber++;

			// set this flag to allow the time signature engraver and bar line engraver
			_timeSignatureFound = true;
			break;
		}
		case CAMusElement::Note:
		case CAMusElement::Rest:
		case CAMusElement::MidiNote:
		case CAMusElement::Slur:
		case CAMusElement::Tuplet:
		case CAMusElement::Syllable:
		case CAMusElement::FunctionMark:
		case CAMusElement::FiguredBassMark:
		case CAMusElement::Mark:
		case CAMusElement::Undefined:
			break;
		}

		if ( v->musElementList()[i]->isPlayable() ) {
			if (anacrusisCheck) {			// first check upbeat bar, only once
				doAnacrusisCheck( time );
				anacrusisCheck = false;
			}
			exportMarksBeforeElement( v->musElementList()[i] );	// A volta bracket has to come before a playable
			exportPlayable( static_cast<CAPlayable*>(v->musElementList()[i]) );
		} else {
			exportMarksAfterElement(v->musElementList()[i]);
		}
	}


	qWarning() << "              Repeat start analysis";
	for (int r=0; r < _repeats.size(); r++ ) {
		qWarning() << " Repeat size " << _repeats.size() << " timeStart " << _repeats[r]->time.first << " timeEnd " << _repeats[r]->time.second <<
																						" volta " << _repeats[r]->voltaNumber << "\n"; 
		for (int a=0; a < _repeats[r]->alternatives.size(); a++ ) {
			qWarning() << "  Repeat:   Alternative size " << _repeats[r]->alternatives.size() << "  timeStart " << _repeats[r]->alternatives[a]->start <<
																						" timeEnd " << _repeats[r]->alternatives[a]->end << "\n";
		}
	}
	//QList<QPair<int,QString*>> oStr;
	for (int ol=0; ol<oStr.length(); ol++) {
		qWarning() << oStr[ol].first << (oStr[ol].second);
	}
	qWarning() << "              Repeat end analysis";

	// end of the voice block
	indentLess();
	indent();
	out() << "\n}";
}

void CALilyPondExport::exportPlayable( CAPlayable *elt ) {
	if ( elt->isFirstInTuplet() ) {
		out() << "\\times " << elt->tuplet()->actualNumber() << "/" << elt->tuplet()->number() << " { ";
	}

	switch (elt->musElementType()) {
	case CAMusElement::Note: {
		// CANote
		CANote *note = static_cast<CANote*>(elt);
		if (note->timeStart()!=_curStreamTime) break;	//! \todo If the time isn't the same, insert hidden rests to fill the needed time
		if (note->isPartOfChord() && note->isFirstInChord()) {
			out() << "<";
		}

		// write the note name
		out() << relativePitchToString( note->diatonicPitch(), _lastNotePitch);
		if (note->forceAccidentals()) {
			out() << "!";
		}

		if ( !note->isPartOfChord() && _lastPlayableLength != note->playableLength() ) {
			out() << playableLengthToLilyPond( note->playableLength() );
		}

		if (note->tieStart())
			out() << "~";

		// export note-specific marks
		exportNoteMarks( note );

		_lastNotePitch = note->diatonicPitch();
		if (!note->isPartOfChord())
			_lastPlayableLength = note->playableLength();

		// finish the chord stanza if that's the last note of the chord
		if (note->isPartOfChord() && note->isLastInChord()) {
			out() << ">";

			if ( _lastPlayableLength != note->playableLength() ) {
				out() << playableLengthToLilyPond( note->playableLength() );
			}

			_lastNotePitch = note->getChord().at(0)->diatonicPitch();
			_lastPlayableLength = note->playableLength();
		}

		// place slurs and phrasing slurs
		if ((!note->isPartOfChord() && note->slurEnd()) ||
		   ( note->isPartOfChord() && note->isLastInChord() && note->getChord().at(0)->slurEnd() ) ) {
			out() << ")";
		}
		if ((!note->isPartOfChord() && note->phrasingSlurEnd()) ||
		   ( note->isPartOfChord() && note->isLastInChord() && note->getChord().at(0)->phrasingSlurEnd() ) ) {
			out() << "\\)";
		}
		if ((!note->isPartOfChord() && note->slurStart()) ||
		   ( note->isPartOfChord() && note->isLastInChord() && note->getChord().at(0)->slurStart() ) ) {
			out() << "(";
		}
		if ((!note->isPartOfChord() && note->phrasingSlurStart()) ||
		   ( note->isPartOfChord() && note->isLastInChord() && note->getChord().at(0)->phrasingSlurStart() ) ) {
			out() << "\\(";
		}

		// export chord marks at the end
		if (!note->isPartOfChord() || note->isLastInChord()) {
			exportMarksAfterElement( note->getChord()[0] );
		}

		// add to the stream time, if the note is not part of the chord or is the last one in the chord
		if (!note->isPartOfChord() ||
		    (note->isPartOfChord() && note->isLastInChord()) )
			_curStreamTime += note->timeLength();

		break;
	}
	case CAMusElement::Rest: {
		// CARest
		CARest *rest = static_cast<CARest*>(elt);
		if (rest->timeStart()!=_curStreamTime) break;	//! \todo If the time isn't the same, insert hidden rests to fill the needed time

		out() << restTypeToLilyPond(rest->restType());

		if ( _lastPlayableLength!=rest->playableLength() ) {
			out() << playableLengthToLilyPond( rest->playableLength() );
		}

		exportMarksAfterElement( rest );

		_lastPlayableLength = rest->playableLength();
		_curStreamTime += rest->timeLength();

		break;
	}
	case CAMusElement::Clef:
	case CAMusElement::Barline:
	case CAMusElement::TimeSignature:
	case CAMusElement::KeySignature:
	case CAMusElement::MidiNote:
	case CAMusElement::Slur:
	case CAMusElement::Tuplet:
	case CAMusElement::Syllable:
	case CAMusElement::FunctionMark:
	case CAMusElement::FiguredBassMark:
	case CAMusElement::Mark:
	case CAMusElement::Undefined:
		break;
	}

	if ( elt->isLastInTuplet() ) {
		out() << "} ";
	}
}

/*!
	Exports the marks for the given \a elt chord, rest or non-playable music
	element.

	For exporting note-specific marks (eg. fingering), see exportNoteMarks().

	\sa exportNoteMarks()
*/
void CALilyPondExport::exportMarksAfterElement( CAMusElement *elt ) {
	for (int i=0; i<elt->markList().size(); i++) {
		CAMark *curMark = elt->markList()[i];

		switch ( curMark->markType() ) {
		case CAMark::Text: {

			// don't export the mark when it is an volta bracket and was sent out already before the playable
			QRegExp vr = QRegExp(_regExpVoltaRepeat);
			QRegExp vb = QRegExp(_regExpVoltaBar);
			if (vr.indexIn(  qPrintable( static_cast<CAText*>(curMark)->text()) ) < 0 &&
				vb.indexIn(  qPrintable( static_cast<CAText*>(curMark)->text()) ) < 0) {
				out() << "^\"" << static_cast<CAText*>(curMark)->text() << "\" ";
			}
			break;
		}
		case CAMark::Dynamic: {
			CADynamic *d = static_cast<CADynamic*>(curMark);
			if ( CADynamic::dynamicTextFromString(d->text())==CADynamic::Custom ) break;

			out() << "\\";
			out() << d->text();
			out() << " ";
			break;
		}
		case CAMark::RehersalMark: {
			out() << "\\mark \\default ";
			break;
		}
		case CAMark::Fermata: {
			out() << "\\fermata ";
			break;
		}
		case CAMark::Articulation: {
			switch (static_cast<CAArticulation*>(curMark)->articulationType()) {
			case CAArticulation::Accent:
				out() << "-> "; break;
			case CAArticulation::Marcato:
				out() << "-^ "; break;
			case CAArticulation::Staccato:
				out() << "-. "; break;
			case CAArticulation::Tenuto:
				out() << "-- "; break;
			case CAArticulation::Prall:
				out() << "\\prall "; break;
			case CAArticulation::PrallUp:
				out() << "\\prallup "; break;
			case CAArticulation::PrallDown:
				out() << "\\pralldown "; break;
			case CAArticulation::UpPrall:
				out() << "\\upprall "; break;
			case CAArticulation::DownPrall:
				out() << "\\downprall "; break;
			case CAArticulation::PrallPrall:
				out() << "\\prallprall "; break;
			case CAArticulation::LinePrall:
				out() << "\\lineprall "; break;
			case CAArticulation::PrallMordent:
				out() << "\\prallmordent "; break;
			case CAArticulation::Mordent:
				out() << "\\mordent "; break;
			case CAArticulation::UpMordent:
				out() << "\\upmordent "; break;
			case CAArticulation::DownMordent:
				out() << "\\downmordent "; break;
			case CAArticulation::Trill:
				out() << "\\trill "; break;
			case CAArticulation::Turn:
				out() << "\\turn "; break;
			case CAArticulation::ReverseTurn:
				out() << "\\reverseturn "; break;
            case CAArticulation::Breath:
                out() << "\\breathe "; break;

			case CAArticulation::Staccatissimo:
			case CAArticulation::Espressivo:
			case CAArticulation::Portato:	
			case CAArticulation::UpBow:	
			case CAArticulation::DownBow:	
			case CAArticulation::Flageolet:	
			case CAArticulation::Open:	
			case CAArticulation::Stopped:	
			case CAArticulation::Undefined:
				break;
			}

			break;
		}
		case CAMark::Ritardando: {
			CARitardando *r = static_cast<CARitardando*>(curMark);
			out() << "^\\markup{ \\text \\italic \"" << ((r->ritardandoType()==CARitardando::Ritardando)?"rit.":"accel.") << "\"} ";
			
			break;
		}
		case CAMark::Tempo:
		case CAMark::Crescendo:
		case CAMark::Pedal:
		case CAMark::InstrumentChange:
		case CAMark::Undefined:
		case CAMark::BookMark:
		case CAMark::RepeatMark:
		case CAMark::Fingering:
			break;
		}
	}
}

/*!
	Exports the note-specific marks like fingering.

	\sa exportMarksAfterElement()
 */
void CALilyPondExport::exportNoteMarks( CANote *elt ) {
	for (int i=0; i<elt->markList().size(); i++) {
		CAMark *curMark = elt->markList()[i];

		switch ( curMark->markType() ) {
		case CAMark::Fingering: {
			CAFingering::CAFingerNumber n = static_cast<CAFingering*>(curMark)->finger();
			if ( n<1 || n>5 ) break;

			out() << "-";
			out() << QString::number( static_cast<CAFingering*>(curMark)->finger() );
			out() << " ";
			break;
		}
		case CAMark::Text:
		case CAMark::Tempo:
		case CAMark::Ritardando:
		case CAMark::Crescendo:
		case CAMark::Pedal:
		case CAMark::InstrumentChange:
		case CAMark::Undefined:
		case CAMark::BookMark:
		case CAMark::RepeatMark:
		case CAMark::Dynamic:
		case CAMark::RehersalMark:
		case CAMark::Articulation:
		case CAMark::Fermata:
			break;
		}
	}
}

/*!
	Exports a volta bracket which is currently just a \a elt mark beginning with voltaBar or voltaRepeat.
*/
void CALilyPondExport::exportMarksBeforeElement( CAMusElement *elt ) {
	for (int i=0; i<elt->markList().size(); i++) {
		CAMark *curMark = elt->markList()[i];

		switch ( curMark->markType() ) {
		case CAMark::Text: {

			QRegExp vr = QRegExp(_regExpVoltaRepeat);
			QRegExp vb = QRegExp(_regExpVoltaBar);
			QString txt;
			if (vb.indexIn(  qPrintable( static_cast<CAText*>(curMark)->text()) ) >= 0) {
				txt = vb.cap(1);
				_voltaBracketFinishAtBar = true;
			} else if (vr.indexIn(  qPrintable( static_cast<CAText*>(curMark)->text()) ) >= 0) {
				txt = vr.cap(1);
				_voltaBracketFinishAtRepeat = true;
			}
			if ( _voltaBracketFinishAtRepeat || _voltaBracketFinishAtBar ) {
				out() << "\\voltaStart \\markup \\text { \""<< txt << "\" }  ";
			};
			break;
		}
		case CAMark::Tempo: {
			CATempo *t = static_cast<CATempo*>(curMark);
			out() << "\\tempo " << playableLengthToLilyPond(t->beat()) << " = " << t->bpm() << " ";
			
			break;
                }

                case CAMark::Fingering:
		case CAMark::Ritardando:
		case CAMark::Crescendo:
		case CAMark::Pedal:
		case CAMark::InstrumentChange:
		case CAMark::Undefined:
		case CAMark::BookMark:
		case CAMark::RepeatMark:
		case CAMark::Dynamic:
		case CAMark::RehersalMark:
		case CAMark::Articulation:
		case CAMark::Fermata:
			break;
		}
	}
}

/*!
	Exports the lyrics in form:
	SopranoLyricsOne = {
		My bu -- ny is o -- ver the o -- cean __ My bu -- ny.
	}
*/
void CALilyPondExport::exportLyricsContextBlock( CALyricsContext *lc ) {
	// Print Canorus voice name as a comment to help with debugging/tweaking
	indent();
	out() << "\n% " << lc->name() << "\n";
	QString name = lc->name();
	spellNumbers(name);
	out() << name << " = \\lyricmode {\n";
	indentMore();

	indent();
	exportLyricsContextImpl(lc);

	indentLess();
	out() << "\n}\n";
}

/*!
	Exports the syllables only without the SopranoLyircsOne = {} frame.
*/
void CALilyPondExport::exportLyricsContextImpl( CALyricsContext *lc ) {
	for (int i=0; i<lc->syllableList().size(); i++) {
		if (i>0) out() << " "; // space between syllables
		out() << syllableToLilyPond(lc->syllableList()[i]);
	}
}

/*!
	Writes the partial command before the first note if there is an upbeat.
*/
void CALilyPondExport::doAnacrusisCheck(CATimeSignature *time) {

				if (!time) return;			// without time signature no upbeat

				// compute the lenght of the beat note,	eigth/quarter/half are supported
				int beatNoteLen = CAPlayableLength::playableLengthToTimeLength( CAPlayableLength::Quarter );
				switch (time->beat()) {
				case 4:			break;
				case 8:			beatNoteLen /= 2;	break;
				case 2:			beatNoteLen *= 2;	break;
				default:		return;		// at strange base notes no upbeat
				}

				int oneBar = time->beats()*beatNoteLen;
				int barlen = 0;
				for (int i=0; i<curVoice()->musElementList().size(); i++) {
					if (curVoice()->musElementList()[i]->isPlayable()) {
						barlen += curVoice()->musElementList()[i]->timeLength();
					}
					// if it's a whole bar or beyond no upbeat (probably a staff without barlines)
					if (barlen >= oneBar) return;

					if (curVoice()->musElementList()[i]->musElementType() == CAMusElement::Barline) break;
				}
				CAPlayableLength res = CAPlayableLength( CAPlayableLength::HundredTwentyEighth );
				out() << "\\partial "
				<<res.musicLength()
				<<"*"<<barlen/res.playableLengthToTimeLength(res)
				<<" ";
}

/*!
	Writes the voice's \relative note intro and returns the note pitch for the current voice.
	This function is usually used for writing the beginning of the voice.
	\warning This function doesn't write "{" paranthesis to mark the start of the voice!
*/
CADiatonicPitch CALilyPondExport::writeRelativeIntro() {
	int i;

	// find the first playable element and set the key signature if found any
	for ( i=0;
	      (i<curVoice()->musElementList().size() &&
	      (curVoice()->musElementList()[i]->musElementType()!=CAMusElement::Note));
	      i++);

	// no playable elements present, return default c' pitch
	if (i==curVoice()->musElementList().size())
		return CADiatonicPitch( 28 );

	CADiatonicPitch notePitch = static_cast<CANote*>(curVoice()->musElementList()[i])->diatonicPitch();
	notePitch.setNoteName( ((notePitch.noteName() + 3) / 7) * 7 );
	out() << "\\relative "
	      << relativePitchToString( notePitch, CADiatonicPitch(21) ) << " "; // LilyPond default C is c1

	return notePitch;
}

/*!
	This function returns the relative note pitch in LilyPond syntax on the given
	Canorus pitch and previous note pitch.

	eg. pitch=10, accs=1, prevPitch=5 => returns "f'"

	\sa notePitchToLilyPond()
*/
const QString CALilyPondExport::relativePitchToString( CADiatonicPitch p, CADiatonicPitch prevPitch) {
	// write the note name
	QString stringPitch = diatonicPitchToLilyPond(p);

	// write , or ' to lower/higher a note
	int delta = prevPitch.noteName() - p.noteName();
	while (delta > 3) { // add the needed amount of the commas
		stringPitch += ",";
		delta -= 7;
	}
	while (delta < -3) { // add the needed amount of the apostrophes
		stringPitch += "'";
		delta += 7;
	}

	return stringPitch;
}


/*!
	Converts the given clefType to LilyPond syntax.
*/
const QString CALilyPondExport::clefTypeToLilyPond( CAClef::CAClefType clefType, int c1orig, int offset ) {
	QString type;
	int c1 = c1orig + offset;

	switch (clefType) {
		case CAClef::G:
			if (c1==-2) type = "treble";
			else if (c1==-4) type = "french";
			break;
		case CAClef::F:
			if (c1==10) type = "bass";
			else if (c1==8) type = "varbaritone";
			else if (c1==12) type = "subbass";
			break;
		case CAClef::C:
			if (c1==0) type = "soprano";
			else if (c1==2) type = "mezzosoprano";
			else if (c1==4) type = "alto";
			else if (c1==6) type = "tenor";
			else if (c1==8) type = "baritone";
			break;
		case CAClef::PercussionHigh:
		case CAClef::PercussionLow:
			type = "percussion";
			break;
		case CAClef::Tab:
			type = "tab";
			break;
	}

	if ( offset > 0 )
		type.append(QString("^") + QString::number(offset+1));

	if ( offset < 0 )
		type.append(QString("_") + QString::number((offset-1)*(-1)));

	return type;
}

/*!
	Returns the key signature gender major/minor in LilyPond syntax.

	\sa keySignaturePitchToLilyPond()
*/
const QString CALilyPondExport::diatonicKeyGenderToLilyPond( CADiatonicKey::CAGender gender ) {
	switch (gender) {
		case CADiatonicKey::Major: return QString("\\major");
		case CADiatonicKey::Minor: return QString("\\minor");
	}

	return QString();
}

/*!
	Converts the note length to LilyPond syntax.
*/
const QString CALilyPondExport::playableLengthToLilyPond( CAPlayableLength playableLength ) {
	QString length = "4";
	switch (playableLength.musicLength()) {
		case CAPlayableLength::Breve:
			length = "\\breve";
			break;
		case CAPlayableLength::Whole:
			length = "1";
			break;
		case CAPlayableLength::Half:
			length = "2";
			break;
		case CAPlayableLength::Quarter:
			length = "4";
			break;
		case CAPlayableLength::Eighth:
			length = "8";
			break;
		case CAPlayableLength::Sixteenth:
			length = "16";
			break;
		case CAPlayableLength::ThirtySecond:
			length = "32";
			break;
		case CAPlayableLength::SixtyFourth:
			length = "64";
			break;
		case CAPlayableLength::HundredTwentyEighth:
			length = "128";
			break;
		case CAPlayableLength::Undefined:
			length = "4";
			break;
	}

	for (int j=0; j<playableLength.dotted(); j++) length += ".";

	return length;
}

/*!
	Converts the note pitch to LilyPond syntax.
*/
const QString CALilyPondExport::diatonicPitchToLilyPond( CADiatonicPitch pitch ) {
	QString name;

	name = (char)((pitch.noteName()+2)%7 + 'a');

	for (int i=0; i < pitch.accs(); i++)
		name += "is";	// append as many -is-es as necessary

	for (int i=0; i > pitch.accs(); i--) {
		if ( (name == "e") || (name == "a") )
			name += "s";	// for pitches E and A, only append single -s the first time
		/*
		// this seems to be language dependent, or just not like this, taken out for now -- Georg
		//
		else if (name[0]=='a')
			name += "as";	// for pitch A, append -as instead of -es
		*/
		else
			name += "es";	// otherwise, append normally as many es-es as necessary
	}

	return name;
}

/*!
	Converts the rest type to LilyPond syntax.
*/
const QString CALilyPondExport::restTypeToLilyPond(CARest::CARestType type) {
	switch (type) {
		case CARest::Normal:
			return "r";
			break;
		case CARest::Hidden:
			return "s";
			break;
		case CARest::Undefined:
			return "r";
			break;
	}
	return "r";
}

/*!
	Converts the barline type to LilyPond syntax.
*/
const QString CALilyPondExport::barlineTypeToLilyPond(CABarline::CABarlineType type) {
	switch (type) {
		case CABarline::Single:
			return "|";
			break;
		case CABarline::Double:
			return "||";
			break;
		case CABarline::End:
			return "|.";
			break;
		case CABarline::RepeatOpen:	// possible repeat bar lines: ".|:" ":..:" ":|.|:" ":|.:"  ":|."
			return ".|:";
			break;
		case CABarline::RepeatClose:
			return ":|.";
			break;
		case CABarline::RepeatCloseOpen:
			return ":|.|:";
			break;
		case CABarline::Dotted:
			return ":";
			break;
		case CABarline::Undefined:
			return "|";
			break;
	}
	return "|";
}

const QString CALilyPondExport::syllableToLilyPond( CASyllable *s ) {
	QString text = s->text();
	// escape Lilys non-parsable characters
	text = text.replace("\"", "\\\"");

	// wrap the text with quotes
	text = (QString("\"")+text+"\"");

	// replace underscore with space
	text = text.replace("_", " ");
	
	if (s->hyphenStart())
		text += " --";
	else if (s->melismaStart())
		text += " __";

	return text;
}

/*!
	Exports the current sheet to Lilypond syntax.
*/
void CALilyPondExport::exportSheetImpl(CASheet *sheet)
{
	out().setCodec("UTF-8");
	setCurSheet( sheet );

	// we need to check if the document is not set, for example at exporting the first sheet
	if (sheet->document()) {
		setCurDocument( sheet->document() );
	}

	// Print file name and Canorus version in comments at the top of the file
	out() << "% This document was generated by Canorus, version " << CANORUS_VERSION << "\n";

	// Version of Lilypond syntax being generated.
	out() << "\\version \"2.10.0\"\n";

	writeDocumentHeader();


	for ( int c = 0; c < sheet->contextList().size(); ++c ) {
		if (sheet->contextList()[c]->contextType() == CAContext::Staff) {
			//scanForRepeats(static_cast<CAStaff*>(sheet->contextList()[c]));
			break;
		}
	}

	/* Write the volta helper function in case we need it
	if (!_voltaFunctionWritten)
		voltaFunction();
	 */

	// Export voices as Lilypond variables: \StaffOneVoiceOne = \relative c { ... }
	for ( int c = 0; c < sheet->contextList().size(); ++c ) {
		setCurContextIndex( c );
		switch (sheet->contextList()[c]->contextType()) {
			case CAContext::Staff:
				exportStaffVoices( static_cast<CAStaff*>(sheet->contextList()[c]) );
				break;
			case CAContext::LyricsContext:
				exportLyricsContextBlock( static_cast<CALyricsContext*>(sheet->contextList()[c]) );
				break;
			case CAContext::FunctionMarkContext:
			case CAContext::FiguredBassContext:
				break;
		}
	}

	exportScoreBlock( sheet );
}

/*!
	Export document title, subtitle, composer, copyright etc.
*/
void CALilyPondExport::writeDocumentHeader() {
	out() << "\n\\header {\n";
	indentMore();
	indent(); out() << "title          = " << markupString( curDocument()->title() ) << "\n";
	indent(); out() << "subtitle       = " << markupString( curDocument()->subtitle() ) << "\n";
	indent(); out() << "composer       = " << markupString( curDocument()->composer() ) << "\n";
	indent(); out() << "arranger       = " << markupString( curDocument()->arranger().isEmpty()?"":(tr("arr.", "arrangement")+" "+curDocument()->arranger()) ) << "\n";
	indent(); out() << "poet           = " << markupString( curDocument()->poet() ) << "\n";
	indent(); out() << "texttranslator = " << markupString( curDocument()->textTranslator() ) << "\n";
	indent(); out() << "dedication     = " << markupString( curDocument()->dedication() ) << "\n";
	indent(); out() << "copyright      = " << markupString( curDocument()->copyright() ) << "\n";
	indentLess();

	out() << "}\n";
}

/*!
	Export document title, subtitle, composer, copyright etc.
*/
void CALilyPondExport::scanForRepeats(CAStaff *staff) {
	out() << "\n % \\repeat volta xxx \n";

	CABarline *bl;
	QList<CAMark*> ml;

	// barlineRefs aus score/staff.h
	for (int b = 0; b < staff->barlineRefs().size(); b++ ) {
		out() << "% " << (staff->barlineRefs()[b])->musElementType() << "  " ;
		bl = static_cast<CABarline*>(staff->barlineRefs()[b]);
		bl->barlineType();
		out() << CABarline::barlineTypeToString(bl->barlineType());
		if (	bl->barlineType()==CABarline::RepeatClose ||
			bl->barlineType()==CABarline::RepeatOpen ||
			bl->barlineType()==CABarline::RepeatCloseOpen ) {
			out() << "\n % \\repeat volta X " << CABarline::barlineTypeToString( bl->barlineType()) << "\n";
		}
		ml = bl->markList();
		for (int e = 0; e < ml.size(); e++ ) {
			if ( ml[e]->markType() == CAMark::RepeatMark && static_cast<CARepeatMark*>(ml[e])->repeatMarkType() == CARepeatMark::Volta) {
				out() << "\n % \\repeat volta X " << CARepeatMark::repeatMarkTypeToString( static_cast<CARepeatMark*>(ml[e])->repeatMarkType()) << "\n";
			}
		}
		
	}
	// inline QList<CAMusElement *>& barlineRefs() { return _barlineList; }
}

/*!
	Encapsulates the given string into \markup {}.
*/
QString CALilyPondExport::markupString( QString in ) {
	return QString("\\markup {\"") + escapeWeirdChars( in ) + QString("\"}");
}

/*!
	Replaces characters like backslashes and double brackets with their escaped variant.
*/
QString CALilyPondExport::escapeWeirdChars( QString in ) {
	return in.replace("\\", "\\\\").replace("\"", "\\\"");
}

/*!
	Exports all the voices in the staff to Lilypond.
	Each voice in the staff is stored as a Lilypond variable:

	StaffOneVoiceOne = \relative c { ... }
*/
void CALilyPondExport::exportStaffVoices(CAStaff *staff)
{
	for ( int v = 0; v < staff->voiceList().size(); ++v ) {
		setCurVoice( staff->voiceList()[v] );

		// Print Canorus voice name as a comment to help with debugging/tweaking
		indent();
		out() << "\n% " << curVoice()->name() << "\n";
		
		// Write out the voice name and the equals signexportVoice
		// Variable name is staff index and voice index
		QString voiceName;
		voiceVariableName( voiceName, curContextIndex(), v );
		out() << voiceName << " = ";

		exportVoiceImpl( curVoice() );
		out() << "\n"; // exportVoiceImpl doesn't put endline at the end
	}
}


/*!
	Modify \a name to contain a string "StaffXVoiceY" where
	X and Y are spelled-out versions of \a staffNum and \a voiceNum, respectively.
	This is for generating names of Lilypond variables from Canorus staff and voices indices,
	since Lilypond variable names must contain only alphabetical characters.

	Example: voiceVariableName( name, 1, 2 );
	--> name is "StaffOneVoiceTwo"
*/
void CALilyPondExport::voiceVariableName( QString &name, int staffNum, int voiceNum )
{
	QTextStream( &name ) << "Context" << staffNum << "Voice" << voiceNum ;
	spellNumbers( name );
}


/*!
	Exports the \score block for LilyPond from the current sheet.
	Looks like this:

	\code
	\score {
		<<
			\new Staff {
				<<
					\new Voice { \voiceOne \StaffOneVoiceOne }
					\new Voice { \voiceTwo \StaffOneVoiceTwo }
				>>
			}
			\new Staff {
				<<
					\new Voice { \voiceOne \StaffTwoVoiceOne }
					\new Voice { \voiceTwo \StaffTwoVoiceTwo }
				>>
			}
			...
		>>
	}
	\endcode
*/
void CALilyPondExport::exportScoreBlock( CASheet *sheet ) {
	out() << "\n\\score {\n";
	indentMore();
	int contextCount = sheet->contextList().size();
	if ( contextCount < 1 ) {
		out() << "% No Contexts. This should probably raise an error.\n";
	}
	else {
		// open multiple contexts
		indent();
		out() <<  "<<\n";
		indent(); out() << "\\pointAndClickOff\n"; // remove point-and-click to decrease PDF size
		indent(); out() << "\\set Score.markFormatter = #format-mark-box-alphabet\n"; // draw nice box around rehersal marks as in Canorus GUI
		indentMore();

		// Output each staff
		for( int c = 0; c < contextCount; ++c ) {
			setCurContext( sheet->contextList()[c] );

			switch (curContext()->contextType()) {
				case CAContext::Staff: {
					CAStaff *s = static_cast<CAStaff*>(curContext());

					indent();
					out() <<  "\\new Staff {\n" ;
					indentMore();
					indent(); out() << "\\set Staff.instrumentName = #\"" << escapeWeirdChars( s->name() ) << "\"\n";

					// More than one voice? Add simultaneous symbol
					int voiceCount = s->voiceList().size();
					if ( voiceCount > 1 ) {
						indent();
						out() <<  "<<\n" ;
						indentMore();
					}

					// Output voices
					for( int v = 0; v < voiceCount; ++v ) {

						// Print Canorus voice name as a comment to aid with debugging etc.
						QString curVoiceName( s->voiceList()[v]->name() );
						indent();
						out() << "% " << curVoiceName << "\n";

						// curVoiceLilyCommand is "\voiceOne", "\voiceTwo", etc. to get proper stem directions
						// Only use this if there is more than one voice and less then five (Lily limitation).
						QString curVoiceLilyCommand;
						if ( voiceCount > 1 && v < 4 ) {
							curVoiceLilyCommand.setNum( v + 1 );
							curVoiceLilyCommand = "\\voice" + curVoiceLilyCommand;
							spellNumbers( curVoiceLilyCommand );
						}
						
						// disable autoBeam, if lyrics applied to voice because beams mean melisma singing for singers
						if ( s->voiceList()[v]->lyricsContextList().size() ) {
							if (!curVoiceLilyCommand.isEmpty()) { curVoiceLilyCommand += " "; }
							curVoiceLilyCommand += "\\autoBeamOff";
						}

						// Print Lily variable name
						QString voiceName;
						voiceVariableName( voiceName, c, v );
						indent();
						out() <<  "\\new Voice = \"" << voiceName << "Virtual\" { " << curVoiceLilyCommand << " \\" << voiceName << " }\n" ;
					}
					indentLess();

					// End simultaneous voice
					if ( voiceCount > 1 ) {
						indent();
						out() <<  ">>\n" ;
						indentLess();
					}

					// End \new Staff {
					indent();
					out() <<  "}\n" ;

					break;
				}
				case CAContext::LyricsContext: { // only position the lyrics contexts. Voice assignment at the end of the score block!
					CALyricsContext *lc = static_cast<CALyricsContext*>(curContext());
					QString lcName = lc->name();
					spellNumbers( lcName );

					indent();
					out() << "% " << lc->name() << "\n";
					indent();
					out() << "\\new Lyrics = \"" << lcName << "Virtual\"\n";

					break;
				}
				case CAContext::FunctionMarkContext:
				case CAContext::FiguredBassContext:
					break;
			}

		} // for(contexts)

		// After positioning the lyrics contexts, set their associated voices!
		for (int i=0; i<contextCount; i++) {
			if (i==0) {
				indent();
				out() << "\n";
				indent();
				out() << "% Voice assignment:\n";
				indent();
				 // needed for automatic treating of slurs as melisma for lyrics - multiple syllables below the slured notes are allowed in Canorus, but not recommended
				out() << "\\set Score.melismaBusyProperties = #'()\n";
			}

			CALyricsContext *lc;
			lc = dynamic_cast<CALyricsContext*>(sheet->contextList()[ i ]);
			if (lc) {
				QString lcName = lc->name();
				spellNumbers(lcName);

				QString voiceName;
				voiceVariableName(
					voiceName,
					curSheet()->contextList().indexOf(lc->associatedVoice()->staff()),
					lc->associatedVoice()->staff()->voiceList().indexOf(lc->associatedVoice())
				);

				indent();
				out() << "\\context Lyrics = \"" << lcName << "Virtual\" { \\lyricsto \"" << voiceName << "Virtual\" \\" << lcName << " }\n";
			}
		}

		// End simultaneous contexts
		indentLess();

		// close multiple contexts
		indent();
		out() << ">>\n";
		indentLess();
	}

	// End score block
	out() << "}\n";

	// Conditional layout block to supress default time signature and default bar lines.
	if (!_timeSignatureFound) {

		out() << "\n";
		out() << "\\layout {\n";
		out() << "	\\context {\n";
		out() << "		\\Staff\n";
		out() << "		\\remove \"Time_signature_engraver\"\n";
		out() << "		\\remove \"Bar_engraver\"\n";
		out() << "	}\n";
		out() << "}\n";
    }

	// We put some syntax reminders how to adjust the music on the page.
	// Can go away if we have a proper gui for setting this.
	out() << "\n";
	out() << "% To adjust the points size of notes and fonts, it can be done like this:\n";
	out() << "% #(set-global-staff-size 16.0)\n";
	out() << "\n";
	out() << "% Some examples to adjust the page size:\n";
	out() << "% \\paper { #(set-paper-size \"a3\") }\n";
	out() << "% \\paper { #(set-paper-size \"a4\" 'landscape) }\n";
	out() << "% But to move the music on the page this needs to be done:\n";
	out() << "% \\paper{\n";
	out() << "%	paper-width = 16\\cm\n";
	out() << "%	line-width = 12\\cm\n";
	out() << "%	left-margin = 2\\cm\n";
	out() << "%	top-margin = 3\\cm\n";
	out() << "%	bottom-margin = 3\\cm\n";
	out() << "%	ragged-last-bottom = ##t\n";
	out() << "%	page-count = #2\n";
	out() << "% }\n\n";
}


/*!
	Output tabs according to _indentLevel.
*/
void CALilyPondExport::indent()
{
	for ( int i = 0; i < curIndentLevel(); ++i ) {
		out() << "\t";
	}
}


/*!
	Spell out numbers in a QString: "Staff1Voice2" -> "StaffOneVoiceTwo"
	This is necessary because Lilypond variable names can only contain letters.
*/
void CALilyPondExport::spellNumbers( QString &s )
{
	s.replace( "0" , "Zero" );
	s.replace( "1" , "One" );
	s.replace( "2" , "Two" );
	s.replace( "3" , "Three" );
	s.replace( "4" , "Four" );
	s.replace( "5" , "Five" );
	s.replace( "6" , "Six" );
	s.replace( "7" , "Seven" );
	s.replace( "8" , "Eight" );
	s.replace( "9" , "Nine" );
}

void CALilyPondExport::voltaFunction( void )
{
	out() << "\n";
	out() << "% Volta: Read about a preliminary hack to export volta brackets to lilypond:\n";
	out() << "% If you put a text mark with the name 'voltaRepeat xyz' or 'voltaBar xyz' on a note,\n";
	out() << "% then a volta bracket will appear.\n";
	out() << "% voltaRepeat lasts until the next repeat bar line, voltaBar until the next bar line.\n";
	out() << "% See lilypond example voltaCustom.\n";
	out() << "%\n";
	out() << "voltaStart =\n";
   	out() << "	#(define-music-function (parser location repMarkupA ) (markup? )\n";
	out() << "		#{\n";
	out() << "			\\set Score.repeatCommands = #(list (list 'volta $repMarkupA) )\n";
	out() << "		#})\n";
	out() << "% Usage in lilypond:\n";
	out() << "% Start volta:      \\voltaStart \\markup \\text \\italic { \"first time\" }\n";
	out() << "% End volta:        \\set Score.repeatCommands = #'((volta #f))\n";
	out() << "\n";

	_voltaFunctionWritten = true;
}

const QString CALilyPondExport::_altOpening = QString(" } \alternatives { { ");
const QString CALilyPondExport::_regExpVoltaRepeat = QString("voltaRepeat (.*)");
const QString CALilyPondExport::_regExpVoltaBar = QString("voltaBar (.*)");

