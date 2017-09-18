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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <utime.h>
#include <time.h>
#include <errno.h>
#include "ISOExtract.h"
#include "iso.h"

#ifndef _MSC_VER
# include <unistd.h>
# define mkdir(file) mkdir(file, 0755)
# define FILE_SEPARATOR '/'
#else
# include <direct.h>
# define getcwd _getcwd
# define stat _stat
# define realpath(file_name, resolved_name) _fullpath(resolved_name, file_name, sizeof(resolved_name))
# define FILE_SEPARATOR '\\'
# define WINDOWS_BUILD 1
#endif

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

void ISOExtractClass::setDetailedStatus(bool detailedStatus)
{
	this->detailedStatus = detailedStatus;
}

int ISOExtractClass::readRawSector(unsigned int FAD, unsigned char *buffer, int *readsize, trackinfo_struct *track)
{
	if (track == NULL)
		track = FADToTrack(FAD);

	if (imageType != IT_ISO)
	{
		fseek(track->fp, track->fileoffset + (FAD-track->fadstart) * track->sectorsize, SEEK_SET); 
		if ((readsize[0] = (int)fread(buffer, sizeof(unsigned char), 2352, track->fp)) != 2352)
			return false;
		return true;
	}

	return false;
}

int ISOExtractClass::readSectorSubheader(unsigned int FAD, xa_subheader_struct *subheader, trackinfo_struct *track)
{
	if (track == NULL)
		track = FADToTrack(FAD);

	if (imageType != IT_ISO)
	{
		fseek(track->fp, (track->fileoffset + (FAD-track->fadstart) * track->sectorsize) + 16, SEEK_SET); 
		if ((fread(subheader, sizeof(unsigned char), sizeof(xa_subheader_struct), track->fp)) != sizeof(xa_subheader_struct))
			return false;
		return true;
	}   

	return false;
}

trackinfo_struct *ISOExtractClass::FADToTrack(unsigned int FAD)
{
	trackinfo_struct *track = NULL;

	for (int i = 0; i < cdinfo.numtracks; i++)
	{
		if (FAD >= cdinfo.trackinfo[i].fadstart &&
			FAD <= cdinfo.trackinfo[i].fadend)
		{
			track = &cdinfo.trackinfo[i];
			break;
		}
	}

	return track;
}

int ISOExtractClass::readUserSector(int offset, unsigned char *buffer, int *readsize, trackinfo_struct *track, sectorinfo_struct *sectorinfo)
{
   int size;
   int mode;
	unsigned int FAD=offset+150;

	if (track == NULL)
		track = FADToTrack(FAD);

   if (track == NULL)
      return false;

	if (sectorinfo)
		sectorinfo->type = track->type;

	if (imageType != IT_ISO)
   {
      unsigned char sync[12];
      static unsigned char truesync[12] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };

		fseek(track->fp, track->fileoffset + (FAD-track->fadstart) * track->sectorsize, SEEK_SET); 
      if (fread((void *)sync, sizeof(unsigned char), 12, track->fp) != 12)
         return false;

      if (memcmp(sync, truesync, 12) == 0)
      {
         // Data sector
			unsigned char buf[4];
			fread(buf, 1, 4, track->fp);
         mode = buf[3];
         if (mode == 1)
            size = 2048;
         else if (mode == 2)
         {
				// Figure it out based on subheader

				if ((fread(&sectorinfo->subheader, sizeof(unsigned char), sizeof(xa_subheader_struct), track->fp)) != sizeof(xa_subheader_struct))
					return false;

            if (sectorinfo->subheader.sm & XAFLAG_FORM2)
				{
					if (sectorinfo->subheader.sm & XAFLAG_AUDIO)
						size = 2304; // We don't need the last 20 bytes
					else
                  size = 2324;
				}
            else
               size = 2048;

            fseek(track->fp, 4, SEEK_CUR);
         }
         else
            return false;
      }
      else
      {
         // Audio sector
         fseek(track->fp, track->fileoffset + (FAD-track->fadstart) * track->sectorsize, SEEK_SET); 
         size = track->sectorsize;
      }
   }
   else
   {
      fseek(track->fp, track->fileoffset + (FAD-track->fadstart) * track->sectorsize, SEEK_SET); 
      size = track->sectorsize;
   }

   if ((readsize[0] = (int)fread(buffer, sizeof(unsigned char), size, track->fp)) != size)
      return false;
   return true;
}

int ISOExtractClass::extractIP(const char *dir)
{
   unsigned char sector[2048];
   char outfilename[PATH_MAX];
   FILE *outfp;
   int i;

   sprintf(outfilename, "%s%cIP.BIN", dir, FILE_SEPARATOR);
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
   return true;
error:
   if (outfp)
      fclose(outfp);
   return false;
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

enum errorcode ISOExtractClass::readPVD(pvd_struct *pvd)
{
   int readsize;
   unsigned char sector[2048];
   unsigned char *buffer;

   if (!readUserSector(16, sector, &readsize))
      return ERR_READ;

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

   return ERR_NONE;
}

enum errorcode ISOExtractClass::readPathTable(pvd_struct *pvd, ptr_struct **ptr, int *numptr)
{
   int i,j=0;
   int num_entries=0;
   unsigned char sector[2352];
   int ptrsize;
   ptr_struct *tempptr;
   int maxptr = 10;
   unsigned char *buffer;
   enum errorcode err=ERR_NONE;

   if ((tempptr = (ptr_struct *)malloc(sizeof(ptr_struct) * maxptr)) == NULL)
   {
      err = ERR_ALLOC;
      goto error;
   }

   // Read in Path Table
   ptrsize = (pvd->PathTableSizeL / 2048)+(pvd->PathTableSizeL % 2048 ? 1 : 0);

   j = 0;

   for (i = 0; i < ptrsize; i++)
   {
      int readsize;

      if (!readUserSector(pvd->LocationOfTypeLPathTable+i, sector, &readsize))
      {
         err = ERR_READ;
         goto error;
      }

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
   return ERR_NONE;
error:
   if (tempptr)
      free(tempptr);
   return err;
}

enum errorcode ISOExtractClass::readDirRecords(unsigned long lba, unsigned long dirrecsize, dirrec_struct **dirrec, unsigned long *numdirrec, unsigned long parent)
{
   unsigned char *buffer;
   unsigned long counter=0;
   unsigned long num_entries;
   int readsize;
   unsigned char sector[2048];
   dirrec_struct *tempdirrec;
   unsigned long maxdirrec;
   unsigned long i;
   enum errorcode err=ERR_NONE;

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
      {
         err = ERR_READ;
         goto error;
      }

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
            {
               err = ERR_ALLOC;
               goto error;
            }
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
   return err;
error:
   if (tempdirrec)
      free(tempdirrec);
   return err;
}

enum errorcode ISOExtractClass::loadCompleteRecordSet(pvd_struct *pvd, dirrec_struct **dirrec, unsigned long *numdirrec)
{
   unsigned long i;
   enum errorcode err=ERR_NONE;

   // Load in the root directory 
   if ((err = readDirRecords(pvd->DirectoryRecordForRootDirectory.LocationOfExtentL, 
                         pvd->DirectoryRecordForRootDirectory.DataLengthL,
                         dirrec, numdirrec, 0xFFFFFFFF)) != ERR_NONE)
   {
      return err;
   }

   // Now then, let's go through and handle the sub directories
   for (i = 0; i < numdirrec[0]; i++)
   {
      if (dirrec[0][i].FileFlags & ISOATTR_DIRECTORY &&
          dirrec[0][i].FileIdentifier[0] != '\0' &&
          dirrec[0][i].FileIdentifier[0] != 0x01)
      {         
         if ((err = readDirRecords(dirrec[0][i].LocationOfExtentL, 
                               dirrec[0][i].DataLengthL,
                               dirrec, numdirrec, i)) != ERR_NONE)
            return err;
      }
   }

   return err;
}

void ISOExtractClass::setPathSaveTime(char *path, dirrec_struct *dirrec)
{
	// Set the file time
   struct tm time;
   time.tm_sec = dirrec->RecordingDateAndTime.Second;
   time.tm_min = dirrec->RecordingDateAndTime.Minute;
   time.tm_hour = dirrec->RecordingDateAndTime.Hour; // need to compensate for gmt
   time.tm_mday = dirrec->RecordingDateAndTime.Day;
   time.tm_mon = dirrec->RecordingDateAndTime.Month;
   time.tm_year = dirrec->RecordingDateAndTime.Year;
   time.tm_wday = 0;
   time.tm_yday = 0;
   time.tm_isdst = 0;

   struct utimbuf tb;
   tb.modtime = mktime(&time);
   tb.actime = tb.modtime;
   utime(path, &tb);
}

enum errorcode ISOExtractClass::extractFiles(dirrec_struct *dirrec, unsigned long numdirrec, const char *dir)
{
   unsigned long i;
   FILE *fp, *fp2;
   char filename[PATH_MAX+2], filename2[PATH_MAX], filename3[PATH_MAX];
   unsigned char sector[2352];
   unsigned long bytes_written=0;
	sectorinfo_struct sectorinfo, sectorinfo2;
   enum errorcode err = ERR_NONE;

   for (i = 0; i < numdirrec; i++)
   {
      bool mpegMultiplexDemux=false;
      if (!(dirrec[i].FileFlags & ISOATTR_DIRECTORY))
      {
         char *p;
         int readsize=1;
         int trackindex=0;

			// Check first sector to make sure it isn't a CDDA track
			trackinfo_struct *track = FADToTrack((dirrec[i].LocationOfExtentL-cdinfo.trackinfo[trackindex].fileoffset / 2048)+150);
			if (track->type == TT_CDDA)
			{
				dirrec[i].XAAttributes.attributes = 0x4111;
				continue;
			}
			else if (track->type == TT_MODE2 &&
				      strstr((char *)dirrec[i].FileIdentifier, ".MPG") &&
						readUserSector(dirrec[i].LocationOfExtentL-cdinfo.trackinfo[trackindex].fileoffset+0, sector, &readsize, track, &sectorinfo) &&
						readUserSector(dirrec[i].LocationOfExtentL-cdinfo.trackinfo[trackindex].fileoffset+1, sector, &readsize, track, &sectorinfo2))
			{
				// Is it a muxed mpeg file?
				if ((sectorinfo.subheader.sm & XAFLAG_VIDEO) && sectorinfo.subheader.ci == 0x0F &&
					 (sectorinfo2.subheader.sm & XAFLAG_AUDIO) && sectorinfo2.subheader.ci == 0x7F)
					mpegMultiplexDemux = true;			
			}

         if (dirrec[i].ParentRecord != 0xFFFFFFFF)
         {
            dirrec_struct *parent=&dirrec[dirrec[i].ParentRecord];
            strcpy(filename3, (char *)dirrec[i].FileIdentifier);

            for(;;)
            {
               sprintf(filename2, "%s%c%s", parent->FileIdentifier, FILE_SEPARATOR, filename3);
               if (parent->ParentRecord == 0xFFFFFFFF)
                  break;
               strcpy(filename3, filename2);
               parent = &dirrec[parent->ParentRecord];
            }
            sprintf(filename, "%s%cFiles%c%s", dir, FILE_SEPARATOR, FILE_SEPARATOR, filename2);
         }
         else
            sprintf(filename, "%s%cFiles%c%s", dir, FILE_SEPARATOR, FILE_SEPARATOR, dirrec[i].FileIdentifier);

         // remove the version number
         p = strrchr((char *)filename, ';');
         if (p)
            p[0] = '\0';

			if (mpegMultiplexDemux)
			{
				p = strrchr(filename, '.');
				p[0] = '\0';

				strcpy(filename2, filename);
				strcat(filename, ".M1V");
				strcat(filename2, ".MP2");

				if ((fp2 = fopen(filename2, "wb")) == NULL)
				{
               err = ERR_OPENWRITE;
					goto error;
				}
			}

         // Treat as a regular mode 1 file
         if ((fp = fopen(filename, "wb")) == NULL)
         {
            err = ERR_OPENWRITE;
            goto error;
         }

		if (!detailedStatus)
				printf("%s...", dirrec[i].FileIdentifier);

         for (unsigned long i2 = 0; i2 < dirrec[i].DataLengthL; i2+=2048)
         {
			if (detailedStatus)
				printf("\r%s:(%ld/%ld)", dirrec[i].FileIdentifier, i2 / 2048, dirrec[i].DataLengthL / 2048);
			if (!readUserSector(dirrec[i].LocationOfExtentL-cdinfo.trackinfo[trackindex].fileoffset + i2 / 2048, sector, &readsize, track, &sectorinfo))
         {
            err = ERR_READ;
				goto error;
         }

				FILE *curOutput=NULL;

				if (!mpegMultiplexDemux)
					curOutput = fp;
				else
				{
					if ((sectorinfo.subheader.sm & XAFLAG_VIDEO) && sectorinfo.subheader.ci == 0x0F)
						curOutput = fp;
					else if ((sectorinfo.subheader.sm & XAFLAG_AUDIO) && sectorinfo.subheader.ci == 0x7F)
						curOutput = fp2;
				}

            if ((dirrec[i].DataLengthL-i2) < (unsigned long)2048)
            {
					if (curOutput != NULL)
					{
                  fwrite(sector, 1, (dirrec[i].DataLengthL-i2), curOutput);
                  if (ferror(curOutput))
                  {
                     err = ERR_WRITE;
							goto error;
                  }
					}
            }
            else
            {
					if (curOutput != NULL)
					{
                  fwrite(sector, 1, readsize, curOutput);
                  if (ferror(curOutput))
                  {
                     err = ERR_WRITE;
							goto error;
                  }
					}
            }
         }

         if (detailedStatus)
            printf("\r%s:(%ld/%ld)...done.\n", dirrec[i].FileIdentifier, dirrec[i].DataLengthL / 2048, dirrec[i].DataLengthL / 2048);
         else
            printf("done.\n");

         fclose(fp);
         setPathSaveTime(filename, &dirrec[i]);
         fp = NULL;
         if (mpegMultiplexDemux)
         {
            fclose(fp2);
            setPathSaveTime(filename2, &dirrec[i]);
            fp2 = NULL;
         }
      }
      else
      {
         // it's a directory, ignore
      }
   }

   return err;
error:
   if (fp != NULL)
      fclose(fp);
	if (fp2 != NULL)
      fclose(fp2);
   return err;
}

enum errorcode ISOExtractClass::createPaths(const char *dir, ptr_struct *ptr, int numptr, dirrec_struct *dirrec, unsigned long numdirrec)
{
   char path[PATH_MAX];
   char path2[PATH_MAX];
   char path3[PATH_MAX];
   int i;

   // Create Files and CDDA
   sprintf(path, "%s%cFiles", dir, FILE_SEPARATOR);
   
   if (mkdir(path) != 0 && errno != EEXIST)
   {
	   printf("Error creating directory %s\n", path);
      return ERR_CREATEDIR;
   }

#if 0
	if (oldTime)
	{
		?HANDLE hDir = CreateFile(path, FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		setPathSaveTime(hDir, &dirrec[0]);
	}
#endif

   sprintf(path, "%s%cCDDA", dir, FILE_SEPARATOR);

   if (mkdir(path) != 0 && errno != EEXIST)
   {
	   printf("Error creating directory %s\n", path);
      return ERR_CREATEDIR;
   }

   // Read Paths and Create
   for (i = 1; i < numptr; i++)
   {
      if (ptr[i].ParentDirectoryNumber == 1)
         sprintf(path, "%s%cFiles%c%s", dir, FILE_SEPARATOR, FILE_SEPARATOR, ptr[i].DirectoryIdentifier);
      else
      {
         ptr_struct *parent=&ptr[ptr[i].ParentDirectoryNumber-1];
         strcpy(path3, (char *)ptr[i].DirectoryIdentifier);

         for(;;)
         {
            sprintf(path2, "%s%c%s", parent->DirectoryIdentifier, FILE_SEPARATOR, path3);
            if (parent->ParentDirectoryNumber == 1)
               break;
            strcpy(path3, path2);
            parent = &ptr[parent->ParentDirectoryNumber-1];
         }
         sprintf(path, "%s%cFiles%c%s", dir, FILE_SEPARATOR, FILE_SEPARATOR, path2);
      }
      
      if (mkdir(path) != 0 && errno != EEXIST)
      {
         printf("Error creating directory %s\n", path);
         return ERR_CREATEDIR;
      }
   }

   return ERR_NONE;
}

enum errorcode ISOExtractClass::extractCDDA(dirrec_struct *dirrec, unsigned long numdirrec, const char *dir)
{
   for (int i = 0; i < cdinfo.numtracks; i++)
   {
      if (cdinfo.trackinfo[i].type  == TT_CDDA)
      {
         char filename[PATH_MAX];
         unsigned char sector[2352];

         sprintf(filename, "%s%cCDDA%ctrack%02d.bin", dir, FILE_SEPARATOR, FILE_SEPARATOR, i+1);
         FILE *outfp = fopen(filename, "wb");

         if (outfp)
         {            
            unsigned int num_sectors=((unsigned int)cdinfo.trackinfo[i].fadend-(unsigned int)cdinfo.trackinfo[i].fadstart+1);
            if (!detailedStatus)
               printf("Track %d...", i + 1);
            for (unsigned int  j = 0; j < num_sectors; j++)
            {
               int readsize=0;
               if (detailedStatus)
                  printf("\rTrack %d:(%ld/%ld)", i + 1, j, num_sectors);
               if (readUserSector(cdinfo.trackinfo[i].fadstart + j - 150, sector, &readsize))
                  fwrite(sector, 1, 2352, outfp);
            }
            if (detailedStatus)
               printf("\rTrack %d:(%ld/%ld)...done.\n", i + 1, num_sectors, num_sectors);
            else
               printf("done.\n");
            fclose(outfp);
         }
         else
            return ERR_OPENWRITE;
      }
   }
   printf("\n");
   return ERR_NONE;
}

void ISOExtractClass::createDB(pvd_struct *pvd, dirrec_struct *dirrec, unsigned long numdirrec, DBClass *db)
{
   unsigned long i, j, k=0;
   unsigned long counter=0;
   unsigned long last_lba=0, next_lba=0;
   unsigned long next_index;

   // Setup basic stuff

   char ipfilename[9];
   sprintf(ipfilename, ".%cIP.BIN", FILE_SEPARATOR);
   db->setIPFilename(ipfilename);

//   memcpy(db->cdinfo, cdinfo, sizeof(cdinfo_struct));
   db->setPVD(pvd);

	db->addFile(dirrec, 0, this);
	k++;

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

   for (int i = 0; i < cdinfo.numtracks; i++)
      db->addTrack(&cdinfo.trackinfo[i], i);
}

int ISOExtractClass::parseCueFile(const char *filename, FILE *fp)
{
   char text[PATH_MAX*2];
	char fn[PATH_MAX];
   int tracknum=0, indexnum, min, sec, frame, pregap=0;

	fseek(fp, 0, SEEK_SET);

   cdinfo.numtracks = 1;
   cdinfo.trackinfo = (trackinfo_struct *)calloc(100, sizeof(trackinfo_struct));
   if (!cdinfo.trackinfo)
      goto error;

   for (;;)
   {
      // Retrieve a line in cue
      if (fscanf(fp, "%[^\n]\n", text) == EOF)
         break;

      // Figure out what it is
		if (strncmp(text, "CATALOG", 7) == 0)
		{
			sscanf(text, "CATALOG %s", cdinfo.catalogNo);
		}
		else if (strncmp(text, "CDTEXTFILE", 10) == 0)
			printf("Warning: Unsupported cue tag 'CDTEXTFILE'\n");
		else if (strncmp(text, "FLAGS", 5) == 0)
			printf("Warning: Unsupported cue tag 'FLAGS'\n");
		else if (strncmp(text, "FILE", 4) == 0)
		{
			// Go back, retrieve image filename
			char type[512];
			sscanf(text, "FILE \"%[^\"]\" %s\r\n", fn, type);   

			if (strcasecmp(type, "BINARY") != 0)
			{
				printf("Error: Unsupported cue 'FILE' type '%s'\n", type);
				goto error;
			}
		}
		else if (strncmp(text, "INDEX", 5) == 0)
		{
			// Handle accordingly

			if (sscanf(text, "INDEX %d %d:%d:%d\r\n", &indexnum, &min, &sec, &frame) != 4)
				break;

			if (indexnum == 1)
			{
				// Update toc entry
				cdinfo.trackinfo[tracknum-1].fadstart = (MSF_TO_FAD(min, sec, frame) + pregap + 150);
				cdinfo.trackinfo[tracknum-1].fileoffset = MSF_TO_FAD(min, sec, frame) * cdinfo.trackinfo[tracknum-1].sectorsize;
			}
		}
		else if (strncmp(text, "ISRC", 4) == 0)
			printf("Warning: Unsupported cue tag 'ISRC'\n");
		else if (strncmp(text, "PERFORMER", 9) == 0)
		{
			// Ignore line
		}
      else if (strncmp(text, "PREGAP", 5) == 0)
      {
         if (sscanf(text, "PREGAP %d:%d:%d\r\n", &min, &sec, &frame) != 3)
            break;

         pregap += MSF_TO_FAD(min, sec, frame);
      }
      else if (strncmp(text, "POSTGAP", 5) == 0)
      {
         if (sscanf(text, "POSTGAP %d:%d:%d\r\n", &min, &sec, &frame) != 3)
            break;
      }
		else if (strncmp(text, "REM", 3) == 0)
		{
			// Comment, just ignore line
			if (fscanf(fp, "%*[^\n]\n") == EOF)
				break;
		}
		else if (strncmp(text, "SONGWRITER", 10) == 0)
		{
			// Ignore line
		}
		else if (strncmp(text, "TITLE", 5) == 0)
		{
			// Ignore line
		}
		else if (strncmp(text, "TRACK", 5) == 0)
		{
			char type[512];
			// Handle accordingly
			if (sscanf(text, "TRACK %d %[^\r\n]\r\n", &tracknum, type) != 2)
				break;

			if (tracknum > cdinfo.numtracks)
			{
				// Reallocate buffer
				trackinfo_struct *trackinfo = (trackinfo_struct *)malloc((tracknum+1)*sizeof(trackinfo_struct));
				if (trackinfo == NULL)
					goto error;
				memcpy(trackinfo, cdinfo.trackinfo, sizeof(trackinfo_struct)*cdinfo.numtracks);
				free(cdinfo.trackinfo);
				cdinfo.trackinfo = trackinfo;
				cdinfo.numtracks = tracknum;
			}

			strcpy(cdinfo.trackinfo[tracknum-1].filename, fn);

			if (strncmp(type, "MODE1", 5) == 0 ||
				strncmp(type, "MODE2", 5) == 0)
			{
				// Figure out the track sector size
				cdinfo.trackinfo[tracknum-1].sectorsize = atoi(type + 6);
				cdinfo.trackinfo[tracknum-1].type = (tracktype)(type[4]-'0');
			}
			else if (strncmp(type, "AUDIO", 5) == 0)
			{
				cdinfo.trackinfo[tracknum-1].sectorsize = 2352;
				cdinfo.trackinfo[tracknum-1].type = TT_CDDA;
			}
		}
   }

	cdinfo.trackinfo[tracknum].fileoffset = 0;
	cdinfo.trackinfo[tracknum].fadstart = 0xFFFFFFFF;

	FILE *imageFp;
	// Now go and open up the image file, figure out its size, etc.
	if ((imageFp = fopen(cdinfo.trackinfo[0].filename, "rb")) == NULL)
	{
		// try stripping off path and use the same path as cue file
		char newFilename[PATH_MAX];

		MakeCuePathFilename(cdinfo.trackinfo[0].filename, filename, newFilename);

		if ((imageFp = fopen(newFilename, "rb")) == NULL)
		{
         // try using the same base filename and path as cue file
         strcpy(newFilename, filename);
         char *p = strrchr(newFilename, '.');
         strcpy(p, ".BIN");

         if ((imageFp = fopen(newFilename, "rb")) == NULL)
         {
            cdinfo.trackinfo[tracknum].fileoffset = 0xFFFFFFFF;
            cdinfo.trackinfo[tracknum].fadstart = 0xFFFFFFFF;
            printf("Error opening binary file specified in cue file: %s\n", cdinfo.trackinfo[0].filename);
            goto error;
         }
		}
	}

	struct stat st;
	stat(cdinfo.trackinfo[0].filename, &st);

	for (int i = 0; i < tracknum; i++)
	{
		cdinfo.trackinfo[i].fp = imageFp;
		cdinfo.trackinfo[i].fadend = cdinfo.trackinfo[i+1].fadstart-1;
	}

	cdinfo.trackinfo[tracknum-1].fadend = cdinfo.trackinfo[tracknum-1].fadstart+
		                                    (st.st_size-cdinfo.trackinfo[tracknum-1].fileoffset)/cdinfo.trackinfo[tracknum-1].sectorsize;
	fclose(fp);
   return true;

error:
   if (cdinfo.trackinfo)
   {
	   free(cdinfo.trackinfo);
	   cdinfo.trackinfo = NULL;
   }
   if (fp)
      fclose(fp);
   return false;
}

enum tracktype ISOExtractClass::getTrackType(uint64_t offset, FILE *fp)
{
	unsigned char header[16];
	static unsigned char sync[12] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };

	fseek(fp, (long)offset, SEEK_SET);
	fread((void *)header, 1, sizeof(header), fp);
	if (memcmp(header, sync, 12) == 0)
	{
		if (header[15] == 0x01)
			return TT_MODE1;
		else if (header[15] == 0x02)
			return TT_MODE2;
		else
			return TT_MODE0;
	}
	else
		return TT_CDDA;
}

int ISOExtractClass::loadMDSTracks(const char *mdsFilename, FILE *isoFile, mds_session_struct *mdsSession)
{
	int i;
	int track_num=0;
	uint32_t fadend;

	cdinfo.trackinfo = (trackinfo_struct *)malloc(sizeof(trackinfo_struct) * mdsSession->lastTrack);
	if (cdinfo.trackinfo == NULL)
		return -1;

	memset(cdinfo.trackinfo, 0, sizeof(trackinfo_struct) * mdsSession->lastTrack);

	for (i = 0; i < mdsSession->totalBlocks; i++)
	{
		mds_track_struct track;
		FILE *fp=NULL;
		int file_size = 0;

		fseek(isoFile, mdsSession->trackBlocksOffset + i * sizeof(mds_track_struct), SEEK_SET);
		if (fread(&track, 1, sizeof(mds_track_struct), isoFile) != sizeof(mds_track_struct))
		{
			free(cdinfo.trackinfo);
			return -1;
		}

		if (track.track_num == 0xA2)
			fadend = MSF_TO_FAD(track.m, track.s, track.f);
		if (!track.extra_offset)
			continue;

		if (track.footer_offset)
		{
			mds_footer_struct footer;
			int found_dupe=0;
			int j;

			// Make sure we haven't already opened file already
			for (j = 0; j < track_num; j++)
			{
				if (track.footer_offset == cdinfo.trackinfo[j].fileid)
				{
					found_dupe = 1;
					break;
				}
			}

			if (found_dupe)
			{
				fp = cdinfo.trackinfo[j].fp;
				file_size = cdinfo.trackinfo[j].filesize;
			}
			else
			{
				fseek(isoFile, track.footer_offset, SEEK_SET);
				if (fread(&footer, 1, sizeof(mds_footer_struct), isoFile) != sizeof(mds_footer_struct))
				{
					free(cdinfo.trackinfo);
					return -1;
				}

				fseek(isoFile, footer.filename_offset, SEEK_SET);

            // Filenames should only be widechars on Windows;
            // on other platforms, there isn't a wide fopen().
            #ifdef WINDOWS_BUILD
				if (footer.is_widechar)
				{
					wchar_t filename[512];
					wchar_t img_filename[512];
					memset(img_filename, 0, 512 * sizeof(wchar_t));

					if (fwscanf(isoFile, L"%512c", img_filename) != 1)
					{
						free(cdinfo.trackinfo);
						return -1;
					}

					if (wcsncmp(img_filename, L"*.", 2) == 0)
					{
						wchar_t *ext;
						swprintf(filename, sizeof(filename)/sizeof(wchar_t), L"%S", mdsFilename);
						ext = wcsrchr(filename, '.');
						wcscpy(ext, img_filename+1);
					}
					else
						wcscpy(filename, img_filename);

					fp = _wfopen(filename, L"rb");
				}
				else
				{
            #endif
					char filename[512];
					char img_filename[512];
					memset(img_filename, 0, 512);

					if (fscanf(isoFile, "%512c", img_filename) != 1)
					{
						free(cdinfo.trackinfo);
						return -1;
					}

					if (strncmp(img_filename, "*.", 2) == 0)
					{
						char *ext;
						strcpy(filename, mdsFilename);
						ext = strrchr(filename, '.');
						strcpy(ext, img_filename+1);
					}
					else
						strcpy(filename, img_filename);

					fp = fopen(filename, "rb");
            #ifdef WINDOWS_BUILD
				}
            #endif

				if (fp == NULL)
				{
					free(cdinfo.trackinfo);
					return -1;
				}

				fseek(fp, 0, SEEK_END);
				file_size = ftell(fp);
				fseek(fp, 0, SEEK_SET);
			}
		}

		cdinfo.trackinfo[track_num].type = getTrackType(track.start_offset, fp);
		cdinfo.trackinfo[track_num].fadstart = track.start_sector+150;
		if (track_num > 0)
			cdinfo.trackinfo[track_num-1].fadend = cdinfo.trackinfo[track_num].fadstart;
		cdinfo.trackinfo[track_num].fileoffset = (unsigned int)track.start_offset;
		cdinfo.trackinfo[track_num].sectorsize = track.sector_size;
		cdinfo.trackinfo[track_num].fp = fp;
		cdinfo.trackinfo[track_num].filesize = file_size;
		cdinfo.trackinfo[track_num].fileid = track.footer_offset;
		cdinfo.trackinfo[track_num].interleavedsub = track.subchannel_mode != 0 ? 1 : 0;

		track_num++;
	}

	cdinfo.trackinfo[track_num-1].fadend = fadend;
	cdinfo.numtracks = track_num;
	return 0;
}

int ISOExtractClass::parseMDS(const char *mds_filename, FILE *iso_file)
{
	int32_t i;
	mds_header_struct header;

	fseek(iso_file, 0, SEEK_SET);

	if (fread((void *)&header, 1, sizeof(mds_header_struct), iso_file) != sizeof(mds_header_struct))
		return -1;
	else if (memcmp(&header.signature,  "MEDIA DESCRIPTOR", sizeof(header.signature)))
		return -1;
	else if (header.version[0] > 1)
		return -1;

	if (header.medium_type & 0x10)
	{
		// DVD's aren't supported, nor will they ever be
		return -1;
	}

	for (i = 0; i < 1; i++)
	{
		mds_session_struct session;

		fseek(iso_file, header.sessions_blocks_offset + i * sizeof(mds_session_struct), SEEK_SET);
		if (fread(&session, 1, sizeof(mds_session_struct), iso_file) != sizeof(mds_session_struct))
		{
			return -1;
		}

		if (loadMDSTracks(mds_filename, iso_file, &session) != 0)
			return -1;
	}

	fclose(iso_file);

	return 0;
}

char* StripPreSuffixWhitespace(char* string)
{
	char* p;
	for (;;)
	{
		if (string[0] == 0 || !isspace(string[0]))
			break;
		string++;
	}

	if (strlen(string) == 0)
		return string;

	p = string+strlen(string)-1;
	for (;;)
	{
		if (p <= string || !isspace(p[0]))
		{
			p[1] = '\0';
			break;
		}
		p--;
	}

	return string;
}

int ISOExtractClass::loadParseCCD(FILE *ccd_fp, ccd_struct *ccd)
{
	char text[60], section[CCD_MAX_SECTION], old_name[CCD_MAX_NAME] = "";
	char * start, *end, *name, *value;
	int lineno = 0, error = 0, max_size = 100;

	ccd->dict = (ccd_dict_struct *)malloc(sizeof(ccd_dict_struct)*max_size);
	if (ccd->dict == NULL) 
		return -1;

	ccd->num_dict = 0;

	// Read CCD file
	while (fgets(text, sizeof(text), ccd_fp) != NULL) 
	{
		lineno++;

		start = StripPreSuffixWhitespace(text);

		if (start[0] == '[') 
		{
			// Section
			end = strchr(start+1, ']');
			if (end == NULL) 
			{
				// ] missing from section
				error = lineno;
			}
			else
			{
				end[0] = '\0';
				memset(section, 0, sizeof(section));
				strncpy(section, start + 1, sizeof(section));
				old_name[0] = '\0';
			}
		}
		else if (start[0]) 
		{
			// Name/Value pair
			end = strchr(start, '=');
			if (end) 
			{
				end[0] = '\0';
				name = StripPreSuffixWhitespace(start);
				value = StripPreSuffixWhitespace(end + 1);

				memset(old_name, 0, sizeof(old_name));
				strncpy(old_name, name, sizeof(old_name));
				if (ccd->num_dict+1 > max_size)
				{
					max_size *= 2;
					ccd->dict = (ccd_dict_struct *)realloc(ccd->dict, sizeof(ccd_dict_struct)*max_size);
					if (ccd->dict == NULL)
					{
						free(ccd->dict);
						return -2;
					}
				}
				strcpy(ccd->dict[ccd->num_dict].section, section);
				strcpy(ccd->dict[ccd->num_dict].name, name);
				strcpy(ccd->dict[ccd->num_dict].value, value);
				ccd->num_dict++;
			}
			else
				error = lineno;
		}

		if (error)
			break;
	}

	if (error)
	{
		free(ccd->dict);
		ccd->num_dict = 0;
	}

	return error;
}

int ISOExtractClass::GetIntCCD(ccd_struct *ccd, char *section, char *name)
{
	int i;
	for (i = 0; i < ccd->num_dict; i++)
	{
		if (strcasecmp(ccd->dict[i].section, section) == 0 &&
			strcasecmp(ccd->dict[i].name, name) == 0)
			return strtol(ccd->dict[i].value, NULL, 0);
	}

	return -1;
}

int ISOExtractClass::parseCCD(const char *ccd_filename, FILE *iso_file)
{
	int i;
	ccd_struct ccd;
	int num_toc;
	char img_filename[512];
	char *ext;
	FILE *fp;

	strcpy(img_filename, ccd_filename);
	ext = strrchr(img_filename, '.');
	strcpy(ext, ".img");
	fp = fopen(img_filename, "rb");

	if (fp == NULL)
		return -1;

	fseek(iso_file, 0, SEEK_SET);

	// Load CCD file as dictionary
	if (loadParseCCD(iso_file, &ccd))
	{
		fclose(fp);
		return -1;
	}

	num_toc = GetIntCCD(&ccd, "DISC", "TocEntries");

	if (GetIntCCD(&ccd, "DISC", "DataTracksScrambled"))
	{
		fclose(fp);
		free(ccd.dict);
		return -1;
	}

	// Find track number and allocate
	for (i = 0; i < num_toc; i++)
	{
		char sect_name[64];
		int point;

		sprintf(sect_name, "Entry %d", i);
		point = GetIntCCD(&ccd, sect_name, "Point");

		if (point == 0xA1)
		{
			int ses = GetIntCCD(&ccd, sect_name, "Session");

			if (ses != 1)
				continue;

			cdinfo.numtracks=GetIntCCD(&ccd, sect_name, "PMin");;
			cdinfo.trackinfo = (trackinfo_struct *)malloc(cdinfo.numtracks * sizeof(trackinfo_struct));
			if (cdinfo.trackinfo == NULL)
			{
				fclose(fp);
				free(ccd.dict);
				return -1;
			}
			memset(cdinfo.trackinfo, 0, cdinfo.numtracks * sizeof(trackinfo_struct));
		}
	}

	// Load TOC
	for (i = 0; i < num_toc; i++)
	{
		char sect_name[64];
		int ses, point, adr, control, trackno, amin, asec, aframe;
		int alba, zero, pmin, psec, pframe, plba;

		sprintf(sect_name, "Entry %d", i);

		ses = GetIntCCD(&ccd, sect_name, "Session");
		point = GetIntCCD(&ccd, sect_name, "Point");
		adr = GetIntCCD(&ccd, sect_name, "ADR");
		control = GetIntCCD(&ccd, sect_name, "Control");
		trackno = GetIntCCD(&ccd, sect_name, "TrackNo");
		amin = GetIntCCD(&ccd, sect_name, "AMin");
		asec = GetIntCCD(&ccd, sect_name, "ASec");
		aframe = GetIntCCD(&ccd, sect_name, "AFrame");
		alba = GetIntCCD(&ccd, sect_name, "ALBA");
		zero = GetIntCCD(&ccd, sect_name, "Zero");
		pmin = GetIntCCD(&ccd, sect_name, "PMin");
		psec = GetIntCCD(&ccd, sect_name, "PSec");
		pframe = GetIntCCD(&ccd, sect_name, "PFrame");
		plba = GetIntCCD(&ccd, sect_name, "PLBA");

		if (ses != 1)
			// Multi-session not supported
			continue;

		if(point >= 1 && point <= 99)
		{
			trackinfo_struct *track=&cdinfo.trackinfo[point-1];
			track->type = getTrackType(plba*2352, fp);
			track->fadstart = MSF_TO_FAD(pmin, psec, pframe);
			if (point >= 2)
				cdinfo.trackinfo[point-2].fadend = track->fadstart-1;
			track->fileoffset = plba*2352;
			track->sectorsize = 2352;
			track->fp = fp;
			track->filesize = (track->fadend+1-track->fadstart)*2352;
			track->fileid = 0;
			track->interleavedsub = 0;
		}
		else if (point == 0xA2)
			cdinfo.trackinfo[cdinfo.numtracks-1].fadend = MSF_TO_FAD(pmin, psec, pframe);
	}

	fclose(iso_file);

	return 0;
}

void ISOExtractClass::MakeCuePathFilename(const char *filename, const char *cueFilename, char *outFilename)
{
   // try stripping off path and use the same path as cue file
   char file[PATH_MAX];
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

void ISOExtractClass::closeTrackHandles()
{
	if (cdinfo.trackinfo)
	{
		for (int i = 0; i < cdinfo.numtracks; i++)
		{
			if (cdinfo.trackinfo[i].fp)
			{
				fclose(cdinfo.trackinfo[i].fp);
				for (int j = i+1; j < cdinfo.numtracks; j++)
				{
					if (cdinfo.trackinfo[j].fp == cdinfo.trackinfo[i].fp)
						cdinfo.trackinfo[j].fp = NULL;
				}

				cdinfo.trackinfo[i].fp = NULL;
			}
		}
	}
}

enum errorcode ISOExtractClass::importDisc(const char *filename, const char *dir, DBClass *db)
{
   char *p;
   pvd_struct pvd;
   ptr_struct *ptr=NULL;
   int numptr;
   dirrec_struct *dirrec=NULL;
   unsigned long dirrecsize;
   char dlffilename[PATH_MAX];
   char dlfdir[PATH_MAX];
	FILE *fp;
   char header[6];
	size_t num_read;
   enum errorcode err=ERR_NONE;

   memset(&cdinfo, 0, sizeof(cdinfo));

	if (!(fp = fopen(filename, "rb")))
		goto error;

	num_read = fread((void *)header, 1, 6, fp);

   if ((p = strrchr((char *)filename, '.')) == NULL)
      goto error;

   if (strcasecmp(p, ".cue") == 0)
   {
		// Read cue file and figure out where bin file is
      imageType = IT_BINCUE;

	  if (!parseCueFile(filename, fp))
		  goto error;
   }
	else if (strcasecmp(p, ".MDS") == 0 && strncmp(header, "MEDIA ", sizeof(header)) == 0)
	{
		// It's a MDS
		imageType = IT_MDS;
		if (parseMDS(filename, fp) != 0)
			goto error;
	}
	else if (strcasecmp(p, ".CCD") == 0)
	{
		// It's a CCD
		imageType = IT_CCD;
		if (parseCCD(filename, fp) != 0)
		goto error;
	}
   else if (strcasecmp(p, ".iso") == 0)
   {
      // Unsupported
      goto error;
   }
   else
      goto error;

   char command[PATH_MAX*2];
   char path[PATH_MAX];
   realpath(dir, path);
   sprintf(command, "mkdir %s", path);
   system(command); 

   if (!extractIP(dir))
      goto error;

   // Read PVD
   if ((err = readPVD(&pvd)) != ERR_NONE)
      goto error;

   // Read path table and create directories
   if ((err = readPathTable(&pvd, &ptr, &numptr)) != ERR_NONE)
      goto error;

	// Read file table and figure out what goes where
	if ((err = loadCompleteRecordSet(&pvd, &dirrec, &dirrecsize)) != ERR_NONE)
		goto error;

	if ((err = createPaths(dir, ptr, numptr, dirrec, dirrecsize)) != ERR_NONE)
      goto error;

   // Read files into "Files" subdirectory, remember to double check that each file is properly linked to the correct start sector, etc.
	printf("Extracting files:\n");
   if ((err = extractFiles(dirrec, dirrecsize, dir)) != ERR_NONE)
      goto error;

   // Read CDDA tracks into "CDDA" subdirectory
	printf("Extracting CD Tracks:\n");
   if ((err = extractCDDA(dirrec, dirrecsize, dir)) != ERR_NONE)
      goto error;

	// Create database
   createDB(&pvd, dirrec, dirrecsize, db);

	printf("Generating script file");
   // Save database to the root of the output directory
   sprintf(dlffilename, "%s%c%s", dir, FILE_SEPARATOR, "disc.scr");
   if ((err = db->saveSCR(dlffilename, oldTime)) != ERR_NONE)
      goto error;
	printf("..done\n");

   getcwd(dlfdir, sizeof(dlfdir));
   db->setDLFDirectory(dlfdir);

	closeTrackHandles();
   free(cdinfo.trackinfo);
   free(ptr);
   free(dirrec);
   return err;
error:
	closeTrackHandles();
   if (cdinfo.trackinfo)
      free(cdinfo.trackinfo);
   if (ptr)
      free(ptr);
   if (dirrec)
      free(dirrec);
   return err;
}

void ISOExtractClass::setSortType(ISOExtractClass::SORTTYPE sortType)
{
	this->sortType = sortType;
}
