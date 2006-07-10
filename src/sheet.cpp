/** @file sheet.cpp
 * 
 * Copyright (c) 2006, Matevž Jekovec, Canorus development team
 * All Rights Reserved. See AUTHORS for a complete list of authors.
 * 
 * Licensed under the GNU GENERAL PUBLIC LICENSE. See COPYING for details.
 */

#include "context.h"
#include "staff.h"
#include "sheet.h"

CASheet::CASheet(const QString name) {
	_name = name;
}

void CASheet::addStaff() {
	CAStaff *s = new CAStaff(this, 0, 50 + 200*(_contextList.size()));
	_contextList.append(s);
	_staffList.append(s);
}

void CASheet::clear() {
	for (int i=0; i<_contextList.size(); i++) {
		_contextList[i]->clear();
		delete _contextList[i];
	}
	
	_contextList.clear();	
}
