/*  Copyright 2013-2016 Theo Berkau

    This file is part of VCDEXTRACT.

    VCDEXTRACT is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    VCDEXTRACT is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with VCDEXTRACT; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <string.h>
#include "FileListClass.h"

FileListClass::FileListClass(void)
{
}

FileListClass::~FileListClass(void)
{
}

bool FileListClass::read(FILE *fp)
{
   fread(filename, sizeof(filename), 1, fp);
   fread(real_filename, sizeof(real_filename), 1, fp);
   fread(&size, sizeof(size), 1, fp);
   fread(&flags, sizeof(flags), 1, fp);
   fread(&parent, sizeof(parent), 1, fp);
   return true;
}

bool FileListClass::write(FILE *fp)
{
   fwrite(filename, sizeof(filename), 1, fp);
   fwrite(real_filename, sizeof(real_filename), 1, fp);
   fwrite(&size, sizeof(size), 1, fp);
   fwrite(&flags, sizeof(flags), 1, fp);
   fwrite(&parent, sizeof(parent), 1, fp);
   return true;
}

void FileListClass::setFilename( const char *filename )
{
   strcpy(this->filename, filename);
}

char *FileListClass::getFilename()
{
   return filename;
}

void FileListClass::setRealFilename( const char *filename )
{
   strcpy(this->real_filename, filename);
}

char *FileListClass::getRealFilename()
{
   return real_filename;
}

void FileListClass::setLBA( unsigned long lba)
{
   this->lba = lba;
}

unsigned long FileListClass::getLBA()
{
   return lba;
}

void FileListClass::setSize( unsigned long size)
{
   this->size = size;
}

unsigned long FileListClass::getSize()
{
   return size;
}

void FileListClass::setFlags( int flags )
{
   this->flags = flags;
}

int FileListClass::getFlags()
{
   return flags;
}

void FileListClass::setDateTime( volumedatetime_struct &dateTime)
{
	this->dateTime = dateTime;
}

volumedatetime_struct &FileListClass::getDateTime()
{
	return this->dateTime;
}

void FileListClass::setParent( unsigned long parent )
{
   this->parent = parent;
}

unsigned long FileListClass::getParent()
{
   return parent;
}

void FileListClass::setSourceType(FileListClass::SOURCETYPE sourceType)
{
	this->sourceType = sourceType;
	if (sourceType != 0)
		flags |= FF_SOURCETYPE;

}

FileListClass::SOURCETYPE FileListClass::getSourceType()
{
	return sourceType;
}

char *FileListClass::getSourceTypeString()
{
	switch(getSourceType())
	{
	   case ST_MONO_A:
			return "MONO_A";
		case ST_MONO_B:
			return "MONO_B";
		case ST_MONO_C:
			return "MONO_C";
		case ST_STEREO_A:
			return "STEREO_A";
		case ST_STEREO_B:
			return "STEREO_B";
		case ST_STEREO_C:
			return "STEREO_C";
		case ST_CDDA:
			return "CDDA";
		case ST_ISO11172:
			return "ISO11172";
		case ST_VIDEO:
			return "VIDEO";
		case ST_DATA:
			return "DATA";
		default: break;
	}

	return NULL;
}

void FileListClass::setCodingInformation(unsigned char codingInformation)
{
	this->codingInformation = codingInformation;
	if (codingInformation != 0)
		flags |= FF_CODINGINFO;
}

unsigned char FileListClass::getCodingInformation()
{
	return codingInformation;
}