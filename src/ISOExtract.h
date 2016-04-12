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

#ifndef ISO_EXTRACT_H
#define ISO_EXTRACT_H

#include <windows.h>
#include <stdint.h>
#include "iso.h"
#include "DBClass.h"

#define MSF_TO_FAD(m,s,f) ((m * 4500) + (s * 75) + f)

typedef struct
{
	char catalogNo[14];
   int numtracks;
   trackinfo_struct *trackinfo;
} cdinfo_struct;

#pragma pack(push, 1)
typedef struct
{
	uint8_t signature[16];
	uint8_t version[2];
	uint16_t medium_type;
	uint16_t session_count;
	uint16_t unused1[2];
	uint16_t bca_length;
	uint32_t unused2[2];
	uint32_t bca_offset;
	uint32_t unused3[6];
	uint32_t disk_struct_offset;
	uint32_t unused4[3];
	uint32_t sessions_blocks_offset;
	uint32_t dpm_blocks_offset;
	uint32_t enc_key_offset;
} mds_header_struct;

typedef struct
{
	int32_t session_start;
	int32_t session_end;
	uint16_t session_number;
	uint8_t totalBlocks;
	uint8_t leadin_blocks;
	uint16_t first_track;
	uint16_t lastTrack;
	uint32_t unused;
	uint32_t trackBlocksOffset;
} mds_session_struct;

typedef struct
{
	uint8_t mode;
	uint8_t subchannel_mode;
	uint8_t addr_ctl;
	uint8_t unused1;
	uint8_t track_num;
	uint32_t unused2;
	uint8_t m;
	uint8_t s;
	uint8_t f;
	uint32_t extra_offset;
	uint16_t sector_size;
	uint8_t unused3[18];
	uint32_t start_sector;
	uint64_t start_offset;
	uint8_t session;
	uint8_t unused4[3];
	uint32_t footer_offset;
	uint8_t unused5[24];
} mds_track_struct;

typedef struct
{
	uint32_t filename_offset;
	uint32_t is_widechar;
	uint32_t unused1;
	uint32_t unused2;
} mds_footer_struct;

#pragma pack(pop)

#define CCD_MAX_SECTION 20
#define CCD_MAX_NAME 30
#define CCD_MAX_VALUE 20

typedef struct
{
	char section[CCD_MAX_SECTION];
	char name[CCD_MAX_NAME];
	char value[CCD_MAX_VALUE];
} ccd_dict_struct;

typedef struct
{
	ccd_dict_struct *dict;
	int num_dict;
} ccd_struct;

class ISOExtractClass
{
public:
	enum SORTTYPE
	{
		SORT_BY_DIRREC=0,
		SORT_BY_LBA=1,
	};

private:
	enum IMAGETYPE
	{
		IT_BINCUE=0,
		IT_ISO,
		IT_MDS,
		IT_CCD
	};

	int time_sectors;
	cdinfo_struct cdinfo;

	ISOExtractClass::IMAGETYPE imageType;
	ISOExtractClass::SORTTYPE sortType;
	bool oldTime;
	bool detailedStatus;
	int extractIP(const char *dir);
	void isoVarRead(void *var, unsigned char **buffer, size_t varsize);
   int copyDirRecord(unsigned char *buffer, dirrec_struct *dirrec);
	int readPVD(pvd_struct *pvd);
	int readPathTable(pvd_struct *pvd, ptr_struct **ptr, int *numptr);
	int readDirRecords(unsigned long lba, unsigned long dirrecsize, dirrec_struct **dirrec, unsigned long *numdirrec, unsigned long parent);
	int loadCompleteRecordSet(pvd_struct *pvd, dirrec_struct **dirrec, unsigned long *numdirrec);
	void setPathSaveTime(HANDLE hPath, dirrec_struct *dirrec);
	int extractFiles(dirrec_struct *dirrec, unsigned long numdirrec, const char *dir);
	int createPaths(const char *dir, ptr_struct *ptr, int numptr, dirrec_struct *dirrec, unsigned long numdirrec);
	int extractCDDA(dirrec_struct *dirrec, unsigned long numdirrec, const char *dir);
	int createDB(pvd_struct *pvd, dirrec_struct *dirrec, unsigned long numdirrec, DBClass *db);
	int parseCueFile(const char *filename, FILE *fp);
	enum tracktype getTrackType(uint64_t offset, FILE *fp);
	int loadMDSTracks(const char *mds_filename, FILE *iso_file, mds_session_struct *mds_session);
	int parseMDS(const char *mds_filename, FILE *iso_file);
	int loadParseCCD(FILE *ccd_fp, ccd_struct *ccd);
	int GetIntCCD(ccd_struct *ccd, char *section, char *name);
	int parseCCD(const char *ccd_filename, FILE *iso_file);
	void MakeCuePathFilename(const char *filename, const char *cueFilename, char *outFilename);
	void closeTrackHandles();
public:
   ISOExtractClass();
   ~ISOExtractClass();
	void setMaintainOldTime(bool oldTime);
	void setDetailedStatus(bool detailedStatus);
   int importDisc(const char *filename, const char *dir, DBClass *db);
	int readRawSector(unsigned int FAD, unsigned char *buffer, int *readsize, trackinfo_struct *track=NULL);
	trackinfo_struct *FADToTrack(unsigned int FAD);
	int readUserSector(int offset, unsigned char *buffer, int *readsize, trackinfo_struct *track=NULL);
	int readSectorSubheader(unsigned int FAD, xa_subheader_struct *subheader, trackinfo_struct *track=NULL);
	void setSortType(ISOExtractClass::SORTTYPE sortType);
};

#endif