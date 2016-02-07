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

#pragma once

#include <windows.h>
#include <stdio.h>
#include "iso.h"

#define FF_CDDA			(1 << 8)
#define FF_MODE2			(1 << 9)
#define FF_INTERLEAVE	(1 << 10)
#define FF_VIDEO			(1 << 11)
#define FF_AUDIO			(1 << 12)
#define FF_MPEG			(1 << 13)
#define FF_REALTIME		(1 << 14)

#define FF_SOURCETYPE	(1 << 16)
#define FF_FORM2			(1 << 17)
#define FF_CODINGINFO	(1 << 18)

class FileListClass
{
public:
	enum SOURCETYPE
	{
		ST_MONO_A, ST_MONO_B, ST_MONO_C, ST_STEREO_A, ST_STEREO_B, ST_STEREO_C, ST_CDDA, ST_ISO11172, ST_VIDEO, ST_DATA
	};

private:
   char filename[32];
   char real_filename[MAX_PATH];
   unsigned long lba;
   unsigned long size;
   int flags;
	unsigned char codingInformation;
   unsigned long parent;
	FileListClass::SOURCETYPE sourceType;
	volumedatetime_struct dateTime;
public:

   FileListClass(void);
   ~FileListClass(void);
   bool read(FILE *fp);
   bool write(FILE *fp);
   void setFilename( const char *filename );
   char *getFilename();
   void setRealFilename( const char *filename );
   char *getRealFilename();
   void setLBA( unsigned long lba);
   unsigned long getLBA();
   void setSize( unsigned long size);
   unsigned long getSize();
   void setFlags( int flags );
   int getFlags();
	void setDateTime( volumedatetime_struct &dateTime);
	volumedatetime_struct &getDateTime();
   void setParent( unsigned long parent );
   unsigned long getParent();
	void setSourceType(FileListClass::SOURCETYPE sourceType);
	FileListClass::SOURCETYPE getSourceType();
	char *getSourceTypeString();
	void setCodingInformation(unsigned char codingInformation);
	unsigned char getCodingInformation();
};
