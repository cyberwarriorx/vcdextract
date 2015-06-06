/*  Copyright 2013-2015 Theo Berkau

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
#include <commctrl.h>
#include "FileListClass.h"
#include "iso.h"

#include <vector>
using namespace std;

#ifndef _CONSOLE
#include <QTreeWidget>
#include <QListWidget>
#endif

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

typedef struct
{
   unsigned int fileoffset;
   tracktype type;
   unsigned int fadstart;
   char filename[MAX_PATH];
} trackinfo_struct;

class DBClass
{
private:
   HWND hwnd;
   HTREEITEM *htreelist;
   char dlfdir[MAX_PATH];
   
   // File format
   char id[4];
   int version;
   char ipfilename[MAX_PATH];
   pvd_struct pvd;
   unsigned long filelistnum;
   enum SessionType sessionType;
   vector <FileListClass> filelist;
   vector <FileListClass> tracklist;
	bool doMode2;
	bool oldTime;
   char *stripEndWhiteSpace(unsigned char *string, int length);
	void doDirectoryMode1(FILE *fp, int dirIndex, int level);
	void doDirectoryMode2(FILE *fp, int dirIndex, int level);
	void writeDate(FILE *fp, char *string, unsigned char *value, size_t value_size);
public:
   DBClass();
   ~DBClass(void);
   void setHWND(HWND hwnd);
   bool save(const char *filename);
   bool load(const char *filename);
	bool saveSCR(const char *filename, bool oldTime=false);
#ifndef _CONSOLE
   bool listFiles(unsigned long parent, QListWidget *list);
   bool listDirTree(QTreeWidget *tree);
#endif
   bool changeFileFlags();
#ifndef _CONSOLE
   bool loadDiscLayout(const char *filename, QTreeWidget *tree);
#endif
   bool saveDiscLayout(const char *filename);
   char *getDLFDirectory();
   void setDLFDirectory( const char *dlfdir );
   char *getIPFilename();
   void setIPFilename( const char *filename );
   void setPVD( pvd_struct * pvd );
	void addFile( dirrec_struct * dirrec, int i, class ISOExtractClass *iec );
   void setFileNumber( unsigned long num );
   void clearFiles();
   void addTrack( trackinfo_struct *trackinfo, int i);
   void clearTracks();
};
