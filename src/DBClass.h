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

#include "FileListClass.h"
#include "iso.h"

#include <vector>
using namespace std;

enum SessionType
{
   ST_CDROM=1,
   ST_CDI=2,
   ST_ROMXA=3,
   ST_SEMIXA=4
};

enum tracktype
{
   TT_MODE0 = 0, TT_MODE1, TT_MODE2, TT_CDDA
};

enum errorcode
{
   ERR_NONE,
   ERR_OPENREAD=-1,
   ERR_OPENWRITE=-1,
   ERR_READ=-2,
   ERR_WRITE=-3,
   ERR_ALLOC=-4,
   ERR_CREATEDIR=-5,
   ERR_STREAMINFO=-6,
   ERR_COPY=-7,
   ERR_DATANOTLOADED=-8
};

typedef struct
{
	unsigned int fadstart;
	unsigned int fadend;
   unsigned int fileoffset;
	unsigned int sectorsize;
   tracktype type;
	FILE *fp;
	int filesize;
	int fileid;
	int interleavedsub;
   char filename[PATH_MAX];
} trackinfo_struct;

typedef struct
{
	tracktype type;
	xa_subheader_struct subheader;
} sectorinfo_struct;

class DBClass
{
private:
   char dlfdir[PATH_MAX];
   
   // File format
   char id[4];
   int version;
   char ipfilename[PATH_MAX];
   pvd_struct pvd;
   uint32_t filelistnum;
   enum SessionType sessionType;
   vector <FileListClass> filelist;
   vector <FileListClass> tracklist;
	bool doMode2;
	bool oldTime;
   char *stripEndWhiteSpace(unsigned char *string, int length);
	void doDirectoryMode1(FILE *fp, int dirIndex, int level);
	void doDirectoryMode2(FILE *fp, int dirIndex, int level);
	unsigned int dateStrToValue(unsigned char *value, int size);
	void writeDate(FILE *fp, char *string, unsigned char *value, size_t value_size);
public:
   DBClass();
   ~DBClass(void);
   bool save(const char *filename);
   bool load(const char *filename);
	enum errorcode saveSCR(const char *filename, bool oldTime=false);
   bool changeFileFlags();
   bool saveDiscLayout(const char *filename);
   char *getDLFDirectory();
   void setDLFDirectory( const char *dlfdir );
   char *getIPFilename();
   void setIPFilename( const char *filename );
   void setPVD( pvd_struct * pvd );
	void addFile( dirrec_struct * dirrec, int i, class ISOExtractClass *iec );
   void setFileNumber( uint32_t num );
   void clearFiles();
   void addTrack( trackinfo_struct *trackinfo, int i);
   void clearTracks();
};
