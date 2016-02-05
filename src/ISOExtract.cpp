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

#include <shlobj.h>
#include <stdio.h>
#include <io.h>
#include <sys/stat.h>
#include <assert.h>
#include "ISOExtract.h"
//#include "ui_helper.h"
#include "iso.h"

extern DBClass curdb;

ISOExtractClass::ISOExtractClass()
{
}

ISOExtractClass::~ISOExtractClass()
{
}

void ISOExtractClass::setMaintainOldTime(bool oldTime)
{
	this->oldTime = oldTime;
}

int ISOExtractClass::readRawSector(int offset, unsigned char *buffer, int *readsize)
{
	if (imageType == IT_BINCUE)
	{
		fseek(imageFp, (offset * 2352), SEEK_SET); 
		if ((readsize[0] = (int)fread(buffer, sizeof(unsigned char), 2352, imageFp)) != 2352)
			return FALSE;
	}

	return TRUE;
}

int ISOExtractClass::readSectorSubheader(int offset, xa_subheader_struct *subheader)
{
	if (imageType == IT_BINCUE)
	{
		fseek(imageFp, (offset * 2352)+16, SEEK_SET); 
		if ((fread(subheader, sizeof(unsigned char), sizeof(xa_subheader_struct), imageFp)) != sizeof(xa_subheader_struct))
			return FALSE;
		return TRUE;
	}   

	return TRUE;
}

int ISOExtractClass::readUserSector(int offset, unsigned char *buffer, int *readsize)
{
   int size;
   int mode;

   if (imageType == IT_BINCUE)
   {
      unsigned char sync[12];
      static unsigned char truesync[12] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };

      fseek(imageFp, (offset * 2352), SEEK_SET); 
      if (fread((void *)sync, sizeof(unsigned char), 12, imageFp) != 12)
         return FALSE;

      if (memcmp(sync, truesync, 12) == 0)
      {
         // Data sector
         fseek(imageFp, 3, SEEK_CUR); 
         mode = fgetc(imageFp);
         if (mode == 1)
            size = 2048;
         else if (mode == 2)
         {
				// Figure it out based on subheader
				xa_subheader_struct subheader;
				if (!readSectorSubheader(offset, &subheader))
					return FALSE;

            if (subheader.sm & XAFLAG_FORM2)
				{
					if (subheader.sm & XAFLAG_AUDIO)
						size = 2304; // We don't need the last 20 bytes
					else
                  size = 2324;
				}
            else
               size = 2048;

            fseek(imageFp, 4, SEEK_CUR);
         }
         else
            return FALSE;
      }
      else
      {
         // Audio sector
         fseek(imageFp, (offset * 2352), SEEK_SET); 
         size = 2352;
      }
   }
   else
   {
      fseek(imageFp, offset * 2048, SEEK_SET); 
      size = 2048;
   }

   if ((readsize[0] = (int)fread(buffer, sizeof(unsigned char), size, imageFp)) != size)
      return FALSE;
   return TRUE;
}

int ISOExtractClass::extractIP(const char *dir)
{
   unsigned char sector[2048];
   char outfilename[MAX_PATH];
   FILE *outfp;
   int i;

   sprintf(outfilename, "%s\\IP.BIN", dir);
   if ((outfp = fopen(outfilename, "wb")) == NULL)
      goto error;

   for (i = 0; i < 16; i++)
   {
      int readsize;

      if (!readUserSector(i, sector, &readsize))
         goto error;
      if (fwrite((void *)sector, sizeof(unsigned char), 2048, outfp) != 2048)
         goto error;
   }

   fclose(outfp);
   return TRUE;
error:
   if (outfp)
      fclose(outfp);
   return FALSE;
}

void ISOExtractClass::isoVarRead(void *var, unsigned char **buffer, size_t varsize)
{
   memcpy(var, buffer[0], varsize);
   buffer[0] += varsize;
}

int ISOExtractClass::copyDirRecord(unsigned char *buffer, dirrec_struct *dirrec)
{
   unsigned char *temp_pointer;

   temp_pointer = buffer;

   isoVarRead(&dirrec->LengthOfDirectoryRecord, &buffer, sizeof(dirrec->LengthOfDirectoryRecord));
   isoVarRead(&dirrec->ExtendedAttributeRecordLength, &buffer, sizeof(dirrec->ExtendedAttributeRecordLength));
   isoVarRead(&dirrec->LocationOfExtentL, &buffer, sizeof(dirrec->LocationOfExtentL));
   isoVarRead(&dirrec->LocationOfExtentM, &buffer, sizeof(dirrec->LocationOfExtentM));
   isoVarRead(&dirrec->DataLengthL, &buffer, sizeof(dirrec->DataLengthL));
   isoVarRead(&dirrec->DataLengthM, &buffer, sizeof(dirrec->DataLengthM));
   memcpy(&dirrec->RecordingDateAndTime, buffer, sizeof(dirrec->RecordingDateAndTime));
   buffer += 7;
   isoVarRead(&dirrec->FileFlags, &buffer, sizeof(dirrec->FileFlags));
   isoVarRead(&dirrec->FileUnitSize, &buffer, sizeof(dirrec->FileUnitSize));
   isoVarRead(&dirrec->InterleaveGapSize, &buffer, sizeof(dirrec->InterleaveGapSize));
   isoVarRead(&dirrec->VolumeSequenceNumber, &buffer, sizeof(dirrec->VolumeSequenceNumber));
   isoVarRead(&dirrec->LengthOfFileIdentifier, &buffer, sizeof(dirrec->LengthOfFileIdentifier));

   memset(dirrec->FileIdentifier, 0, sizeof(dirrec->FileIdentifier));
   isoVarRead(dirrec->FileIdentifier, &buffer, dirrec->LengthOfFileIdentifier);

   // handle padding
   buffer += (1 - dirrec->LengthOfFileIdentifier % 2);

   memset(&dirrec->XAAttributes, 0, sizeof(dirrec->XAAttributes));

   // sadily, this is the best way I can think of for detecting XA attributes

//   printf("text length = %d left over data = %d\n", dirrec->LengthOfFileIdentifier, dirrec->LengthOfDirectoryRecord - (buffer - temp_pointer));

   if ((dirrec->LengthOfDirectoryRecord - (buffer - temp_pointer)) == 14)
   {
      isoVarRead(&dirrec->XAAttributes.group_id, &buffer, sizeof(dirrec->XAAttributes.group_id));
      isoVarRead(&dirrec->XAAttributes.user_id, &buffer, sizeof(dirrec->XAAttributes.user_id));
      isoVarRead(&dirrec->XAAttributes.attributes, &buffer, sizeof(dirrec->XAAttributes.attributes));

      // byte swap it
      dirrec->XAAttributes.attributes = ((dirrec->XAAttributes.attributes & 0xFF00) >> 8) +
                                        ((dirrec->XAAttributes.attributes & 0x00FF) << 8);

      isoVarRead(&dirrec->XAAttributes.signature, &buffer, sizeof(dirrec->XAAttributes.signature));
      isoVarRead(&dirrec->XAAttributes.file_number, &buffer, sizeof(dirrec->XAAttributes.file_number));
      isoVarRead(dirrec->XAAttributes.reserved, &buffer, sizeof(dirrec->XAAttributes.reserved));
   }

   return (int)(buffer - temp_pointer);
}

int ISOExtractClass::readPVD(pvd_struct *pvd)
{
   int readsize;
   unsigned char sector[2048];
   unsigned char *buffer;

   if (!readUserSector(16, sector, &readsize))
      return FALSE;

   buffer=sector;

   isoVarRead(&pvd->VolumeDescriptorType, &buffer, sizeof(pvd->VolumeDescriptorType));
   isoVarRead(&pvd->StandardIdentifier, &buffer, sizeof(pvd->StandardIdentifier));
   isoVarRead(&pvd->VolumeDescriptorVersion, &buffer, sizeof(pvd->VolumeDescriptorVersion));
   isoVarRead(&pvd->Ununsed, &buffer, sizeof(pvd->Ununsed));
   isoVarRead(&pvd->SystemIdentifier, &buffer, sizeof(pvd->SystemIdentifier));
   isoVarRead(&pvd->VolumeIdentifier, &buffer, sizeof(pvd->VolumeIdentifier));
   isoVarRead(&pvd->Unused2, &buffer, sizeof(pvd->Unused2));
   isoVarRead(&pvd->VolumeSpaceSizeL, &buffer, sizeof(pvd->VolumeSpaceSizeL));
   isoVarRead(&pvd->VolumeSpaceSizeM, &buffer, sizeof(pvd->VolumeSpaceSizeM));
   isoVarRead(&pvd->Unused3, &buffer, sizeof(pvd->Unused3));
   isoVarRead(&pvd->VolumeSetSizeL, &buffer, sizeof(pvd->VolumeSetSizeL));
   isoVarRead(&pvd->VolumeSetSizeM, &buffer, sizeof(pvd->VolumeSetSizeM));
   isoVarRead(&pvd->VolumeSequenceNumberL, &buffer, sizeof(pvd->VolumeSequenceNumberL));
   isoVarRead(&pvd->VolumeSequenceNumberM, &buffer, sizeof(pvd->VolumeSequenceNumberM));
   isoVarRead(&pvd->LogicalBlockSizeL, &buffer, sizeof(pvd->LogicalBlockSizeL));
   isoVarRead(&pvd->LogicalBlockSizeM, &buffer, sizeof(pvd->LogicalBlockSizeM));
   isoVarRead(&pvd->PathTableSizeL, &buffer, sizeof(pvd->PathTableSizeL));
   isoVarRead(&pvd->PathTableSizeM, &buffer, sizeof(pvd->PathTableSizeM));
   isoVarRead(&pvd->LocationOfTypeLPathTable, &buffer, sizeof(pvd->LocationOfTypeLPathTable));
   isoVarRead(&pvd->LocationOfOptionalTypeLPathTable, &buffer, sizeof(pvd->LocationOfOptionalTypeLPathTable));
   isoVarRead(&pvd->LocationOfTypeMPathTable, &buffer, sizeof(pvd->LocationOfTypeMPathTable));
   isoVarRead(&pvd->LocationOfOptionalTypeMPathTable, &buffer, sizeof(pvd->LocationOfOptionalTypeMPathTable));

//   memcpy(&pvd->DirectoryRecordForRootDirectory, buffer, sizeof(pvd->DirectoryRecordForRootDirectory));
//   buffer += sizeof(pvd->DirectoryRecordForRootDirectory);
   // copy over directory record
   buffer += copyDirRecord(buffer, &pvd->DirectoryRecordForRootDirectory);

   isoVarRead(&pvd->VolumeSetIdentifier, &buffer, sizeof(pvd->VolumeSetIdentifier));
   isoVarRead(&pvd->PublisherIdentifier, &buffer, sizeof(pvd->PublisherIdentifier));
   isoVarRead(&pvd->DataPreparerIdentifier, &buffer, sizeof(pvd->DataPreparerIdentifier));
   isoVarRead(&pvd->ApplicationIdentifier, &buffer, sizeof(pvd->ApplicationIdentifier));
   isoVarRead(&pvd->CopyrightFileIdentifier, &buffer, sizeof(pvd->CopyrightFileIdentifier));
   isoVarRead(&pvd->AbstractFileIdentifier, &buffer, sizeof(pvd->AbstractFileIdentifier));
   isoVarRead(&pvd->BibliographicFileIdentifier, &buffer, sizeof(pvd->BibliographicFileIdentifier));
   isoVarRead(&pvd->VolumeCreationDateAndTime, &buffer, sizeof(pvd->VolumeCreationDateAndTime));
   isoVarRead(&pvd->VolumeModificationDateAndTime, &buffer, sizeof(pvd->VolumeModificationDateAndTime));
   isoVarRead(&pvd->VolumeExpirationDateAndTime, &buffer, sizeof(pvd->VolumeExpirationDateAndTime));
   isoVarRead(&pvd->VolumeEffectiveDateAndTime, &buffer, sizeof(pvd->VolumeEffectiveDateAndTime));
   isoVarRead(&pvd->FileStructureVersion, &buffer, sizeof(pvd->FileStructureVersion));
   isoVarRead(&pvd->ReservedForFutureStandardization, &buffer, sizeof(pvd->ReservedForFutureStandardization));
   isoVarRead(&pvd->ApplicationUse, &buffer, sizeof(pvd->ApplicationUse));
   isoVarRead(&pvd->ReservedForFutureStandardization2, &buffer, sizeof(pvd->ReservedForFutureStandardization2));

   SetLastError(NO_ERROR);
   return TRUE;
}

int ISOExtractClass::readPathTable(pvd_struct *pvd, ptr_struct **ptr, int *numptr)
{
   int i,j=0;
   int num_entries=0;
   unsigned char sector[2352];
   int ptrsize;
   ptr_struct *tempptr;
   int maxptr = 10;
   unsigned char *buffer;

   if ((tempptr = (ptr_struct *)malloc(sizeof(ptr_struct) * maxptr)) == NULL)
      goto error;

   // Read in Path Table
   ptrsize = (pvd->PathTableSizeL / 2048)+(pvd->PathTableSizeL % 2048 ? 1 : 0);

   j = 0;

   for (i = 0; i < ptrsize; i++)
   {
      int readsize;

      if (!readUserSector(pvd->LocationOfTypeLPathTable+i, sector, &readsize))
         goto error;

      buffer = sector;

      // Process sector
      for (;;)
      {
         // See if there's a path available
         if (j >= (int)pvd->PathTableSizeL ||
             (int)(buffer-sector) >= readsize)
            break;

         // Make sure we have enough memory, allocate more if need be
         if (num_entries >= maxptr)
         {
            ptr_struct *temp;
            if ((temp = (ptr_struct *)malloc(sizeof(ptr_struct) * (maxptr*2))) == NULL)
               goto error;
            memcpy(temp, tempptr, sizeof(ptr_struct) * maxptr);
            free(tempptr);
            tempptr = temp;
            maxptr = maxptr * 2;
         }

         // Add path
         isoVarRead(&tempptr[num_entries].LengthOfDirectoryIdentifier, &buffer, sizeof(tempptr[num_entries].LengthOfDirectoryIdentifier));
         isoVarRead(&tempptr[num_entries].ExtendedAttributeRecordLength, &buffer, sizeof(tempptr[num_entries].ExtendedAttributeRecordLength));
         isoVarRead(&tempptr[num_entries].LocationOfExtent, &buffer, sizeof(tempptr[num_entries].LocationOfExtent));
         isoVarRead(&tempptr[num_entries].ParentDirectoryNumber, &buffer, sizeof(tempptr[num_entries].ParentDirectoryNumber));

         memset(&tempptr[num_entries].DirectoryIdentifier, 0, 32);
         isoVarRead(&tempptr[num_entries].DirectoryIdentifier, &buffer, tempptr[num_entries].LengthOfDirectoryIdentifier);
         buffer += (tempptr[num_entries].LengthOfDirectoryIdentifier % 2);
         j = (int)(buffer-sector);
         num_entries++;
      }
   }

   ptr[0] = tempptr;
   numptr[0] = num_entries;
   SetLastError(NO_ERROR);
   return TRUE;
error:
   if (tempptr)
      free(tempptr);
   return FALSE;
}

int ISOExtractClass::readDirRecords(unsigned long lba, unsigned long dirrecsize, dirrec_struct **dirrec, unsigned long *numdirrec, unsigned long parent)
{
   unsigned char *buffer;
   unsigned long counter=0;
   unsigned long num_entries;
   int readsize;
   unsigned char sector[2048];
   dirrec_struct *tempdirrec;
   unsigned long maxdirrec;
   unsigned long i;

   if (parent == 0xFFFFFFFF)
   {
      maxdirrec = 10;
      if ((tempdirrec = (dirrec_struct *)malloc(maxdirrec * sizeof(dirrec_struct))) == NULL)
         goto error;
      num_entries = 0;
   }
   else
   {
      maxdirrec = numdirrec[0];
      tempdirrec = dirrec[0];
      num_entries = maxdirrec;
   }

   for (i = 0; i < (dirrecsize / 2048); i++)
   {
      if (!readUserSector(lba+i, sector, &readsize))
         goto error;

      buffer = sector;

      for (;;)
      {
         // See if there's a path available
         if (buffer[0] == 0 ||
            (int)(buffer-sector) >= readsize)
            break;

         // Make sure we have enough memory, allocate more if need be
         if (num_entries >= maxdirrec)
         {
            dirrec_struct *temp;
            if ((temp = (dirrec_struct *)malloc((maxdirrec*2) * sizeof(dirrec_struct))) == NULL)
               goto error;
            memcpy(temp, tempdirrec, sizeof(dirrec_struct) * maxdirrec);
            free(tempdirrec);
            tempdirrec = temp;
            maxdirrec = maxdirrec * 2;
         }

         buffer += copyDirRecord(buffer, &tempdirrec[num_entries]);
         tempdirrec[num_entries].ParentRecord = parent;
         num_entries++;
      }
   }

   dirrec[0] = tempdirrec;
   numdirrec[0] = num_entries;
   SetLastError(NO_ERROR);
   return TRUE;
error:
   if (tempdirrec)
      free(tempdirrec);
   return FALSE;
}

int ISOExtractClass::loadCompleteRecordSet(pvd_struct *pvd, dirrec_struct **dirrec, unsigned long *numdirrec)
{
   unsigned long i;

   // Load in the root directory 
   if (!readDirRecords(pvd->DirectoryRecordForRootDirectory.LocationOfExtentL, 
                         pvd->DirectoryRecordForRootDirectory.DataLengthL,
                         dirrec, numdirrec, 0xFFFFFFFF))
   {
      return FALSE;
   }

   // Now then, let's go through and handle the sub directories
   for (i = 0; i < numdirrec[0]; i++)
   {
      if (dirrec[0][i].FileFlags & ISOATTR_DIRECTORY &&
          dirrec[0][i].FileIdentifier[0] != '\0' &&
          dirrec[0][i].FileIdentifier[0] != 0x01)
      {         
         if (!readDirRecords(dirrec[0][i].LocationOfExtentL, 
                               dirrec[0][i].DataLengthL,
                               dirrec, numdirrec, i))
            goto error;
      }
   }

   SetLastError(NO_ERROR);
   return TRUE;
error:
   return FALSE;
}

void ISOExtractClass::setPathSaveTime(HANDLE hPath, dirrec_struct *dirrec)
{
	SYSTEMTIME stSaveTime;
	FILETIME ftSaveTime;
	TIME_ZONE_INFORMATION tzi;
	SYSTEMTIME stUTCSaveTime;

	// Set the file time
	stSaveTime.wYear = (WORD)(1900 + dirrec->RecordingDateAndTime.Year);
	stSaveTime.wMonth = (WORD)dirrec->RecordingDateAndTime.Month;
	stSaveTime.wDayOfWeek = 0;
	stSaveTime.wDay = (WORD)dirrec->RecordingDateAndTime.Day;
	stSaveTime.wHour = (WORD)dirrec->RecordingDateAndTime.Hour; // need to compensate for gmt
	stSaveTime.wMinute = (WORD)dirrec->RecordingDateAndTime.Minute;
	stSaveTime.wSecond = (WORD)dirrec->RecordingDateAndTime.Second;
	stSaveTime.wMilliseconds = 0;

	GetTimeZoneInformation(&tzi);
	tzi.Bias = 240;
	TzSpecificLocalTimeToSystemTime(&tzi, &stSaveTime, &stUTCSaveTime);
	SystemTimeToFileTime(&stUTCSaveTime, &ftSaveTime);
	SetFileTime(hPath, &ftSaveTime, &ftSaveTime, &ftSaveTime);
}

int ISOExtractClass::extractFiles(cdinfo_struct *cdinfo, dirrec_struct *dirrec, unsigned long numdirrec, const char *dir)
{
   DWORD i;
   HANDLE hOutput=INVALID_HANDLE_VALUE;
   char filename[MAX_PATH+2], filename2[MAX_PATH], filename3[MAX_PATH];
   unsigned char sector[2352];
   unsigned long bytes_written=0;

   for (i = 0; i < numdirrec; i++)
   {
      if (!(dirrec[i].FileFlags & ISOATTR_DIRECTORY))
      {
         char *p;
         int readsize=1;
         int trackindex=0;

         if (dirrec[i].XAAttributes.attributes == 0x4111)
            // CDDA track
            continue;

         if (dirrec[i].ParentRecord != 0xFFFFFFFF)
         {
            dirrec_struct *parent=&dirrec[dirrec[i].ParentRecord];
            strcpy(filename3, (char *)dirrec[i].FileIdentifier);

            for(;;)
            {
               sprintf(filename2, "%s\\%s", parent->FileIdentifier, filename3);
               if (parent->ParentRecord == 0xFFFFFFFF)
                  break;
               strcpy(filename3, filename2);
               parent = &dirrec[parent->ParentRecord];
            }
            sprintf(filename, "%s\\Files\\%s", dir, filename2);
         }
         else
            sprintf(filename, "%s\\Files\\%s", dir, dirrec[i].FileIdentifier);

         // remove the version number
         p = strrchr((char *)filename, ';');
         if (p)
            p[0] = '\0';

         // Treat as a regular mode 1 file
         if ((hOutput = CreateFile(filename, GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
         {
            SetLastError(ERROR_OPEN_FAILED);
            goto error;
         }

			//LARGE_INTEGER time1, time2, freq;
			//QueryPerformanceFrequency(&freq);
			//QueryPerformanceCounter(&time1);
         for (unsigned long i2 = 0; i2 < dirrec[i].DataLengthL; i2+=2048)
         {
				printf("\r%s:(%ld/%ld)", dirrec[i].FileIdentifier, i2/2048, dirrec[i].DataLengthL/2048);
            if (!readUserSector(dirrec[i].LocationOfExtentL-cdinfo->trackinfo[trackindex].fileoffset + i2 / 2048, sector, &readsize))
               goto error;

            if ((dirrec[i].DataLengthL-i2) < (DWORD)readsize)
            {
               if (WriteFile(hOutput, sector, (dirrec[i].DataLengthL-i2), &bytes_written, NULL) == FALSE ||
                  bytes_written != (dirrec[i].DataLengthL-i2))
                  goto error;
            }
            else
            {
               if (WriteFile(hOutput, sector, readsize, &bytes_written, NULL) == FALSE ||
                  bytes_written != readsize)
                  goto error;
            }
         }

			printf("\r%s:(%ld/%ld)...done.\n", dirrec[i].FileIdentifier, dirrec[i].DataLengthL/2048, dirrec[i].DataLengthL/2048);
			//QueryPerformanceCounter(&time2);
			//float time=(float)(time2.QuadPart-time1.QuadPart)/(float)freq.QuadPart;
			//printf("time = %f sec(%f sectors/s)", dirrec[i].FileIdentifier, dirrec[i].DataLengthL/2048, dirrec[i].DataLengthL/2048, time, dirrec[i].DataLengthL/2048 / time);
			printf("\n");

			setPathSaveTime(hOutput, &dirrec[i]);
         CloseHandle(hOutput);
         hOutput = INVALID_HANDLE_VALUE;
      }
      else
      {
         // it's a directory, ignore
      }
   }

   SetLastError(NO_ERROR);
   return TRUE;
error:
   if (hOutput == INVALID_HANDLE_VALUE)
      CloseHandle(hOutput);
   return FALSE;
}

int ISOExtractClass::createPaths(const char *dir, ptr_struct *ptr, int numptr, dirrec_struct *dirrec, unsigned long numdirrec)
{
   char path[MAX_PATH];
   char path2[MAX_PATH];
   char path3[MAX_PATH];
   int i;

   // Create Files and CDDA
   sprintf(path, "%s\\Files", dir);
   if (!CreateDirectory(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
      goto error;

#if 0
	if (oldTime)
	{
		HANDLE hDir = CreateFile(path, FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		setPathSaveTime(hDir, &dirrec[0]);
	}
#endif

   sprintf(path, "%s\\CDDA", dir);
   if (!CreateDirectory(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
      goto error;

   // Read Paths and Create
   for (i = 1; i < numptr; i++)
   {
      if (ptr[i].ParentDirectoryNumber == 1)
         sprintf(path, "%s\\Files\\%s", dir, ptr[i].DirectoryIdentifier);
      else
      {
         ptr_struct *parent=&ptr[ptr[i].ParentDirectoryNumber-1];
         strcpy(path3, (char *)ptr[i].DirectoryIdentifier);

         for(;;)
         {
            sprintf(path2, "%s\\%s", parent->DirectoryIdentifier, path3);
            if (parent->ParentDirectoryNumber == 1)
               break;
            strcpy(path3, path2);
            parent = &ptr[parent->ParentDirectoryNumber-1];
         }
         sprintf(path, "%s\\Files\\%s", dir, path2);
      }
      if (!CreateDirectory(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
         goto error;
   }

   return TRUE;
error:
   return FALSE;
}

int ISOExtractClass::extractCDDA(cdinfo_struct *cdinfo, dirrec_struct *dirrec, unsigned long numdirrec, const char *dir)
{
   for (int i = 0; i < cdinfo->numtracks; i++)
   {
      if (cdinfo->trackinfo[i].type  == TT_CDDA)
      {
         char filename[MAX_PATH];
         unsigned char sector[2352];

         sprintf(filename, "%s\\CDDA\\track%02d.bin", dir, i+1);
         FILE *outfp = fopen(filename, "wb");

         if (outfp)
         {            
            unsigned int num_sectors=((unsigned int)cdinfo->trackinfo[i+1].fadstart-(unsigned int)cdinfo->trackinfo[i].fadstart);
            for (unsigned int  j = 0; j < num_sectors; j++)
            {
               int readsize=0;
					printf("\rTrack %d:(%ld/%ld)", i+1, j, num_sectors);
               if (readUserSector(cdinfo->trackinfo[i].fadstart + j - 150, sector, &readsize))
                  fwrite(sector, 1, 2352, outfp);
            }
				printf("\rTrack %d:(%ld/%ld)...done.\n", i+1, num_sectors, num_sectors);
            fclose(outfp);
         }
      }
   }
	printf("\n");
   return TRUE;
}

int ISOExtractClass::createDB(cdinfo_struct *cdinfo, pvd_struct *pvd, dirrec_struct *dirrec, unsigned long numdirrec, DBClass *db)
{
   unsigned long i, j, k=0;
   unsigned long counter=0;
   unsigned long last_lba=0, next_lba=0;
   unsigned long next_index;

   // Setup basic stuff

   db->setIPFilename(".\\IP.BIN");

//   memcpy(db->cdinfo, cdinfo, sizeof(cdinfo_struct));
   db->setPVD(pvd);

	db->addFile(dirrec, 0, this);

	if (sortType == SORT_BY_LBA)
	{
		for (i = 1; i < numdirrec; i++)
		{
			next_lba = 0xFFFFFFFF;
			next_index = 0xFFFFFFFF;
			for (j = 1; j < numdirrec; j++)
			{
				if (dirrec[j].LocationOfExtentL > last_lba &&
					dirrec[j].LocationOfExtentL < next_lba )
				{
					next_index = j;
					next_lba = dirrec[j].LocationOfExtentL;
				}
			}


			if (next_index == 0xFFFFFFFF || 
				dirrec[next_index].FileIdentifier[0] == 0 ||
				dirrec[next_index].FileIdentifier[0] == 1)
			{
			}
			else
			{
				db->addFile(dirrec, next_index, this);
				k++;
			}

			last_lba = next_lba;
		}
	}
	else if (sortType == SORT_BY_DIRREC)
	{
		for (i = 1; i < numdirrec; i++)
		{
			next_index = i;
			if (next_index == 0xFFFFFFFF || 
				dirrec[next_index].FileIdentifier[0] == 0 ||
				dirrec[next_index].FileIdentifier[0] == 1)
			{
			}
			else
			{
				db->addFile(dirrec, next_index, this);
				k++;
			}
		}
	}
	db->setFileNumber(k);

   for (int i = 0; i < cdinfo->numtracks; i++)
      db->addTrack(&cdinfo->trackinfo[i], i);

   return TRUE;
}

int ISOExtractClass::parseCueFile(const char *filename, cdinfo_struct *cdinfo)
{
   FILE *fp;
   char text[MAX_PATH*2];
   int tracknum=0, indexnum, min, sec, frame, pregap=0;

   if ((fp = fopen(filename, "rt")) == NULL)
      goto error;

   if (fscanf(fp, "FILE \"%*[^\"]\" %*s\r\n") == EOF)
      goto error;

   cdinfo->numtracks = 1;
   cdinfo->trackinfo = (trackinfo_struct *)malloc(100*sizeof(trackinfo_struct));
   if (!cdinfo->trackinfo)
      goto error;

   for (;;)
   {
      // Retrieve a line in cue
      if (fscanf(fp, "%s", text) == EOF)
         break;

      // Figure out what it is
      if (strncmp(text, "TRACK", 5) == 0)
      {
         // Handle accordingly
         if (fscanf(fp, "%d %[^\r\n]\r\n", &tracknum, text) == EOF)
            break;

         if (tracknum > cdinfo->numtracks)
         {
            // Reallocate buffer
            trackinfo_struct *trackinfo = (trackinfo_struct *)malloc((tracknum+1)*sizeof(trackinfo_struct));
            if (trackinfo == NULL)
               goto error;
            memcpy(trackinfo, cdinfo->trackinfo, sizeof(trackinfo_struct)*cdinfo->numtracks);
            free(cdinfo->trackinfo);
            cdinfo->trackinfo = trackinfo;
            cdinfo->numtracks = tracknum;
         }
         if (strncmp(text, "MODE1", 5) == 0 ||
             strncmp(text, "MODE2", 5) == 0)
         {
            // Figure out the track sector size
#if 0
            bytesPerSector = atoi(text + 6);
#endif

            cdinfo->trackinfo[tracknum-1].type = (tracktype)(text[4]-'0');
         }
         else if (strncmp(text, "AUDIO", 5) == 0)
            cdinfo->trackinfo[tracknum-1].type = TT_CDDA;
      }
      else if (strncmp(text, "INDEX", 5) == 0)
      {
         // Handle accordingly

         if (fscanf(fp, "%d %d:%d:%d\r\n", &indexnum, &min, &sec, &frame) == EOF)
            break;

         if (indexnum == 1)
         {
            // Update toc entry
            cdinfo->trackinfo[tracknum-1].fadstart = MSF_TO_FAD(min, sec, frame) + pregap;
            cdinfo->trackinfo[tracknum-1].fileoffset = pregap;
         }
      }
      else if (strncmp(text, "PREGAP", 5) == 0)
      {
         if (fscanf(fp, "%d:%d:%d\r\n", &min, &sec, &frame) == EOF)
            break;

         pregap += MSF_TO_FAD(min, sec, frame);
      }
      else if (strncmp(text, "POSTGAP", 5) == 0)
      {
         if (fscanf(fp, "%d:%d:%d\r\n", &min, &sec, &frame) == EOF)
            break;
      }
   }

   // Go back, retrieve image filename
   fseek(fp, 0, SEEK_SET);
   fscanf(fp, "FILE \"%[^\"]\" %*s\r\n", cdinfo->trackinfo[0].filename);   
   fclose(fp);

   struct _stat st;
   // Figure out file size from image file size
   if (_stat(cdinfo->trackinfo[0].filename, &st) != 0)
   {
      char newFilename[MAX_PATH];

      MakeCuePathFilename(cdinfo->trackinfo[0].filename, filename, newFilename);
      if (_stat(newFilename, &st) != 0)
      {
         cdinfo->trackinfo[tracknum].fileoffset = 0xFFFFFFFF;
         cdinfo->trackinfo[tracknum].fadstart = 0xFFFFFFFF;
         return TRUE;
      }
   }

   cdinfo->trackinfo[tracknum].fileoffset = cdinfo->trackinfo[tracknum-1].fileoffset;
   cdinfo->trackinfo[tracknum].fadstart = (st.st_size/2352) + cdinfo->trackinfo[tracknum].fileoffset;

   return TRUE;

error:
   if (cdinfo->trackinfo)
      free(cdinfo->trackinfo);
   if (fp)
      fclose(fp);
   return FALSE;
}

void ISOExtractClass::MakeCuePathFilename(const char *filename, const char *cueFilename, char *outFilename)
{
   // try stripping off path and use the same path as cue file
   char file[MAX_PATH];
   char *p = strrchr ((char *)filename, '/');
   char *p2 = strrchr ((char *)filename, '\\');

   if (p)
      strcpy(file, p);
   else if (p2)
      strcpy(file, p2);
   else
      strcpy(file, filename);      

   strcpy(outFilename, cueFilename);
   p = strrchr (outFilename, '/');
   p2 = strrchr (outFilename, '\\');

   if (p)
      strcpy(p+1, file);
   else if (p2)
      strcpy(p2+1, file);
   else
      strcpy(outFilename, file);
}

int ISOExtractClass::importDisc(const char *filename, const char *dir, DBClass *db)
{
   cdinfo_struct cdinfo;
   char *p;
   pvd_struct pvd;
   ptr_struct *ptr=NULL;
   int numptr;
   dirrec_struct *dirrec=NULL;
   unsigned long dirrecsize;
   char dlffilename[MAX_PATH];
   char dlfdir[MAX_PATH];

   memset(&cdinfo, 0, sizeof(cdinfo));
	imageFp = NULL;

   // Read cue file and figure out where bin file is
   if ((p = strrchr((char *)filename, '.')) == NULL)
      goto error;

   if (_stricmp(p, ".cue") == 0)
   {
      imageType = IT_BINCUE;

      if (!parseCueFile(filename, &cdinfo))
         goto error;
   }
   else if (_stricmp(p, ".iso") == 0)
   {
      // Unsupported for now
      goto error;
   }
   else
      goto error;

   if ((imageFp = fopen(cdinfo.trackinfo[0].filename, "rb")) == NULL)
   {
      // try stripping off path and use the same path as cue file
      char newFilename[MAX_PATH];

      MakeCuePathFilename(cdinfo.trackinfo[0].filename, filename, newFilename);

      if ((imageFp = fopen(newFilename, "rb")) == NULL)
         goto error;
   }

   char command[MAX_PATH*2];
   char path[MAX_PATH];
   sprintf(command, "mkdir %s", _fullpath(path, dir, sizeof(path)));
   system(command); 

   if (!extractIP(dir))
      goto error;

   // Read PVD
   if (!readPVD(&pvd))
      goto error;

   // Read path table and create directories
   if (!readPathTable(&pvd, &ptr, &numptr))
      goto error;

	// Read file table and figure out what goes where
	if (!loadCompleteRecordSet(&pvd, &dirrec, &dirrecsize))
		goto error;

   if (!createPaths(dir, ptr, numptr, dirrec, dirrecsize))
      goto error;

   // Read files into "Files" subdirectory, remember to double check that each file is properly linked to the correct start sector, etc.
	printf("Extracting files:\n");
   if (!extractFiles(&cdinfo, dirrec, dirrecsize, dir))
      goto error;

   // Read CDDA tracks into "CDDA" subdirectory
	printf("Extracting CD Tracks:\n");
   if (!extractCDDA(&cdinfo, dirrec, dirrecsize, dir))
      goto error;

   // Create database here and save it to the root of the output directory
   if (!createDB(&cdinfo, &pvd, dirrec, dirrecsize, db))
      goto error;

	printf("Generating script file");
   sprintf(dlffilename, "%s\\%s", dir, "disc.scr");
   if (!db->saveSCR(dlffilename, oldTime))
      goto error;
	printf("..done\n");

   GetCurrentDirectory(sizeof(dlfdir), dlfdir);
   db->setDLFDirectory(dlfdir);

   free(cdinfo.trackinfo);
   free(ptr);
   free(dirrec);
   fclose(imageFp);
   return TRUE;
error:
   if (cdinfo.trackinfo)
      free(cdinfo.trackinfo);
   if (ptr)
      free(ptr);
   if (dirrec)
      free(dirrec);
   if (imageFp)
     fclose(imageFp);
   return FALSE;
}

void ISOExtractClass::setSortType(ISOExtractClass::SORTTYPE sortType)
{
	this->sortType = sortType;
}
