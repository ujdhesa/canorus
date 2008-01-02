/*!
	Copyright (c) 2006-2007, Matevž Jekovec, Canorus development team
	All Rights Reserved. See AUTHORS for a complete list of authors.
	
	Licensed under the GNU GENERAL PUBLIC LICENSE. See LICENSE.GPL for details.
*/

#include "core/resource.h"
#include <QFile>

/*!
	\class CAResource
	\brief Different resources included in the file
	
	CAResource is a wrapper for any file included or linked from the document
	itself. Additional files are usually transcripts of the score in a pdf format,
	audio and video files, score in other formats, images in the score etc.
	
	CAResource contains a valid path (_fileName) to the actual file. If the
	resource is external (not included in file), _linked is True and a _fileName
	is the absolute path of the linked file. If the resource is internal (included
	in the file), _linked is False and a _fileName is an absolute path to the extracted
	file in the system temporary directory.
	
	\sa CAResourceContainer
*/

/*!
	Default constructor.
*/
CAResource::CAResource( QString fileName, QString name, bool linked, CAResourceType t ) {
	setName(name);
	setFileName(fileName);
	setLinked(linked);
	setResourceType(t);
}

CAResource::~CAResource() {
	if (isLinked())
		QFile::remove( fileName() );
}
