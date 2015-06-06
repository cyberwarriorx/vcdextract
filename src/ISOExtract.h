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

#ifndef ISO_EXTRACT_H
#define ISO_EXTRACT_H

#include <windows.h>
#include "iso.h"
#include "DBClass.h"

#define MSF_TO_FAD(m,s,f) ((m * 4500) + (s * 75) + f)

typedef struct
{
   int numtracks;
   trackinfo_struct *trackinfo;
} cdinfo_struct;

class ISOExtractClass
{
private:
	enum IMAGETYPE
	{
		IT_BINCUE=0,
		IT_ISO
	};

	ISOExtractClass::IMAGETYPE imageType;
	FILE *imageFp;
	bool oldTime;
	int extractIP(const char *dir);
	void isoVarRead(void *var, unsigned char **buffer, size_t varsize);
   int copyDirRecord(unsigned char *buffer, dirrec_struct *dirrec);
	int readPVD(pvd_struct *pvd);
	int readPathTable(pvd_struct *pvd, ptr_struct **ptr, int *numptr);
	int readDirRecords(unsigned long lba, unsigned long dirrecsize, dirrec_struct **dirrec, unsigned long *numdirrec, unsigned long parent);
	int loadCompleteRecordSet(pvd_struct *pvd, dirrec_struct **dirrec, unsigned long *numdirrec);
	void setPathSaveTime(HANDLE hPath, dirrec_struct *dirrec);
	int extractFiles(cdinfo_struct *cdinfo, dirrec_struct *dirrec, unsigned long numdirrec, const char *dir);
	int createPaths(const char *dir, ptr_struct *ptr, int numptr, dirrec_struct *dirrec, unsigned long numdirrec);
	int extractCDDA(cdinfo_struct *cdinfo, dirrec_struct *dirrec, unsigned long numdirrec, const char *dir);
	int createDB(cdinfo_struct *cdinfo, pvd_struct *pvd, dirrec_struct *dirrec, unsigned long numdirrec, DBClass *db);
   int parseCueFile(const char *filename, cdinfo_struct *cdinfo);
   void MakeCuePathFilename(const char *filename, const char *cueFilename, char *outFilename);
public:
   ISOExtractClass();
   ~ISOExtractClass();
	void setMaintainOldTime(bool oldTime);
   int importDisc(const char *filename, const char *dir, DBClass *db);
	int readRawSector(int offset, unsigned char *buffer, int *readsize);
	int readUserSector(int offset, unsigned char *buffer, int *readsize);
	int readSectorSubheader(int offset, xa_subheader_struct *subheader);
};

#endif