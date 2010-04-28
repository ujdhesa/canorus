/*!
	Copyright (c) 2006-2010, Matevž Jekovec, Canorus development team
	All Rights Reserved. See AUTHORS for a complete list of authors.

	Licensed under the GNU GENERAL PUBLIC LICENSE. See COPYING for details.
*/

#include <QFont>
#include <QPainter>

#include "layout/drawableaccidental.h"
#include "score/muselement.h"
#include "layout/drawablecontext.h"
#include "layout/drawableclef.h"
#include "canorus.h"

/*!
	Default constructor.

	\param accs Type of the accidental: 0 - natural, -1 - flat, +1 - sharp, -2 doubleflat, +2 - cross etc.
	\param musElement Pointer to the according musElement which the accidental represents (usually a CANote or CAKeySignature).
	\param drawableContext Pointer to the according drawable context which the accidental belongs to (usually CADrawableStaff or CADrawableFiguredBass).
*/
CADrawableAccidental::CADrawableAccidental(signed char accs, CAMusElement *musElement, CADrawableContext *drawableContext)
 : CADrawableMusElement(musElement, drawableContext, DrawableAccidental),
   _accs(accs) {

	QFont font("Emmentaler");
	font.setPixelSize(34);
	QGraphicsSimpleTextItem *item = 0;

	switch (_accs) {
		case 0:
			item = new QGraphicsSimpleTextItem( QString(CACanorus::fetaCodepoint("accidentals.natural")), this);
			break;
		case 1:
			item = new QGraphicsSimpleTextItem( QString(CACanorus::fetaCodepoint("accidentals.sharp")), this);
			break;
		case -1:
			item = new QGraphicsSimpleTextItem( QString(CACanorus::fetaCodepoint("accidentals.flat")), this);
			break;
		case 2:
			item = new QGraphicsSimpleTextItem( QString(CACanorus::fetaCodepoint("accidentals.doublesharp")), this);
			break;
		case -2:
			item = new QGraphicsSimpleTextItem( QString(CACanorus::fetaCodepoint("accidentals.flatflat")), this);
			break;
	}
	item->setFont(font);
	addToGroup(item);
	item->setPos(0, -47);

	/* 	setSelectable( false );

 	setWidth( 8 );
 	setHeight( 14 );

 	if (accs==0) {
 		setYPos( y - height()/2 );
 	} else if (accs==1) {
 		setYPos( y - height()/2 );
 	} else if (accs==-1) {
  		setYPos( y - height()/2 - 5 );
 	} else if (accs==2) {
 		setHeight( 6 );
  		setYPos( y - height()/2 );
 	} else if (accs==-2) {
  		setYPos( y - height()/2 - 5 );
 		setXPos( x );
 		setWidth( 12 );
 	}

 	_centerX = x;
 	_centerY = y;
*/}

CADrawableAccidental::~CADrawableAccidental() {
}

/*void CADrawableAccidental::draw(QPainter *p, CADrawSettings s) {
	QFont font("Emmentaler");
	font.setPixelSize(qRound(34*s.z));
	p->setPen(QPen(s.color));
	p->setFont(font);

	switch (_accs) {
		case 0:
			p->drawText(s.x, s.y + qRound(height()/2*s.z), QString(CACanorus::fetaCodepoint("accidentals.natural")));
			break;
		case 1:
			p->drawText(s.x, s.y + qRound((height()/2 + 0.3)*s.z), QString(CACanorus::fetaCodepoint("accidentals.sharp")));
			break;
		case -1:
			p->drawText(s.x, s.y + qRound((height()/2 + 5)*s.z), QString(CACanorus::fetaCodepoint("accidentals.flat")));
			break;
		case 2:
			p->drawText(s.x, s.y + qRound(height()/2*s.z), QString(CACanorus::fetaCodepoint("accidentals.doublesharp")));
			break;
		case -2:
			p->drawText(s.x, s.y + qRound((height()/2 + 5)*s.z), QString(CACanorus::fetaCodepoint("accidentals.flatflat")));
			break;
	}
}*/
