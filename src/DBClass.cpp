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

#ifdef HAVE_FFMPEG
#include "MPEGDemux.h"
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#if !defined(_WIN32) || defined(__MINGW32__)
#include <stdlib.h>
#endif
#include "DBClass.h"
#include "ISOExtract.h"

DBClass::DBClass()
{
   sessionType = ST_CDROM;
}

DBClass::~DBClass(void)
{
}

bool DBClass::save(const char *filename)
{
   FILE *fp;

   if ((fp = fopen(filename, "wb")) == NULL)
      return false;

   id[0] = 'I';
   id[1] = 'T';
   id[2] = 'D';
   id[3] = 'B';

   version = 1;

   fwrite((void *)id, sizeof(id), 1, fp);
   fwrite((void *)&version, sizeof(version), 1, fp);
   fwrite((void *)ipfilename, sizeof(ipfilename), 1, fp);
   fwrite((void *)&pvd, sizeof(pvd), 1, fp);
   fwrite((void *)&filelistnum, sizeof(filelistnum), 1, fp);
   for (unsigned i = 0; i < filelist.size(); i++)
      filelist[i].write(fp);
   fclose(fp);

   return true;
}

bool DBClass::load(const char *filename)
{
   FILE *fp;

   if ((fp = fopen(filename, "rb")) == NULL)
      return false;

   fread((void *)id, sizeof(id), 1, fp);
   fread((void *)&version, sizeof(version), 1, fp);
   fread((void *)ipfilename, sizeof(ipfilename), 1, fp);
   fread((void *)&pvd, sizeof(pvd), 1, fp);
   fread((void *)&filelistnum, sizeof(filelistnum), 1, fp);
   filelist.resize(filelistnum);
   for (unsigned int i = 0; i < filelist.size(); i++)
      filelist[i].read(fp);

   fclose(fp);
   getcwd(dlfdir, sizeof(dlfdir));
   return true;
}

char *DBClass::stripEndWhiteSpace(unsigned char *string, int length)
{
   static char temp[1024];

   strncpy(temp, (char *)string, length);
   for (int i = 0; i < length; i++)
   {
      if (!isspace(temp[length-1-i]))
      {
         temp[length-i] = '\0';
         return temp;
      }
   }

   temp[0] = '\0';
   return temp;
}

void DBClass::doDirectoryMode1(FILE *fp, int dirIndex, int level)
{
	uint32_t i;
	char space[1024];

	memset(space, ' ', level * 4);
	space[level*4] = '\0';

	for (i=1; i < filelistnum; i++)
	{
		if (filelist[i].getFlags() & FF_CDDA)
			continue;

		if (filelist[i].getFlags() & FF_MODE2)
		{
			doMode2 = true;
			continue;
		}

		if (filelist[i].getParent() == dirIndex)
		{
			if (filelist[i].getFlags() & ISOATTR_DIRECTORY)
			{
				fprintf(fp, "%sDirectory %s\n", space, filelist[i].getFilename());
				if (oldTime)
				{
					volumedatetime_struct vdt = filelist[i].getDateTime();
					fprintf(fp, "%s    RecordingDate %02d/%02d/%04d %02d:%02d:%02d:%02d:%02d\n", space, vdt.Day, vdt.Month, 1900+vdt.Year, vdt.Hour, vdt.Minute, vdt.Second, 0, vdt.Zone);
				}
				doDirectoryMode1(fp, i, level+1);
				fprintf(fp, "%sEndDirectory\n\n", space);
			}
			else
			{
				fprintf(fp, "%sFile %s\n", space, filelist[i].getFilename());
				if (oldTime)
				{
					volumedatetime_struct vdt = filelist[i].getDateTime();
					fprintf(fp, "%s    RecordingDate %02d/%02d/%04d %02d:%02d:%02d:%02d:%02d\n", space, vdt.Day, vdt.Month, 1900+vdt.Year, vdt.Hour, vdt.Minute, vdt.Second, 0, vdt.Zone);
				}
				fprintf(fp, "%s    FileSource \"%s\"\n", space, filelist[i].getRealFilename());
				fprintf(fp, "%s    EndFileSource\n", space);
				fprintf(fp, "%sEndFile\n\n", space);
			}
		}
	}
}

void DBClass::doDirectoryMode2(FILE *fp, int dirIndex, int level)
{
	uint32_t i;
	char space[1024];

	memset(space, ' ', level * 4);
	space[level*4] = '\0';

	for (i=0; i < filelistnum; i++)
	{
		if (filelist[i].getFlags() & FF_CDDA ||
		    !(filelist[i].getFlags() & FF_MODE2))
			continue;

		if (filelist[i].getParent() == dirIndex)
		{
			int flags=filelist[i].getFlags();
			if (filelist[i].getFlags() & ISOATTR_DIRECTORY)
			{
				fprintf(fp, "%sDirectory %s\n", space, filelist[i].getFilename());
				if (oldTime)
				{
					volumedatetime_struct vdt = filelist[i].getDateTime();
					fprintf(fp, "%s    RecordingDate %02d/%02d/%04d %02d:%02d:%02d:%02d:%02d\n", space, vdt.Day, vdt.Month, 1900+vdt.Year, vdt.Hour, vdt.Minute, vdt.Second, 0, vdt.Zone);
				}
				doDirectoryMode2(fp, i, level+1);
				fprintf(fp, "%sEndDirectory\n\n", space);
			}
			else
			{
				fprintf(fp, "%sFile %s\n", space, filelist[i].getFilename());
				if (oldTime)
				{
					volumedatetime_struct vdt = filelist[i].getDateTime();
					fprintf(fp, "%s    RecordingDate %02d/%02d/%04d %02d:%02d:%02d:%02d:%02d\n", space, vdt.Day, vdt.Month, 1900+vdt.Year, vdt.Hour, vdt.Minute, vdt.Second, 0, vdt.Zone);
				}

#ifdef HAVE_FFMPEG
				if ((flags & (FF_MPEG|FF_VIDEO|FF_AUDIO)) == (FF_MPEG|FF_VIDEO|FF_AUDIO))
				{
					char basefile[PATH_MAX];
					strcpy(basefile, filelist[i].getRealFilename());
					char *p=strchr(basefile, '.');
					if (p != NULL) 
						p[0]='\0';;              
					fprintf(fp, "%s    SectorRate %d\n", space, 75);
					fprintf(fp, "%s    Pack\n", space);
					fprintf(fp, "%s    MpegMultiplex\n", space);
					fprintf(fp, "%s        MpegStream \"%s.M1V\" VIDEO\n", space, basefile);
					fprintf(fp, "%s            BitRate %d.0\n", space, filelist[i].getVideoBitrate());
					fprintf(fp, "%s        EndMpegStream\n", space);
					fprintf(fp, "%s        MpegStream \"%s.MP2\" AUDIO\n", space, basefile);
					fprintf(fp, "%s            BitRate %d.0\n", space, filelist[i].getAudioBitrate());
					fprintf(fp, "%s        EndMpegStream\n", space);
					fprintf(fp, "%s    EndMpegMultiplex\n", space);
				}
				else
#endif
				{
					fprintf(fp, "%s    FileSource \"%s\"\n", space, filelist[i].getRealFilename());
					if (filelist[i].getFlags() & FF_REALTIME)
						fprintf(fp, "%s       RealTime\n", space);
					if (filelist[i].getFlags() & FF_SOURCETYPE)
						fprintf(fp, "%s       SourceType %s\n", space, filelist[i].getSourceTypeString());
					if (filelist[i].getFlags() & FF_FORM2)
						fprintf(fp, "%s       DataType FORM2\n", space);
					if (filelist[i].getFlags() & FF_CODINGINFO)
						fprintf(fp, "%s       CodingInformation %02d\n", space, filelist[i].getCodingInformation());
					fprintf(fp, "%s    EndFileSource\n", space);
				}

				fprintf(fp, "%sEndFile\n\n", space);
			}
		}
	}
}

unsigned int DBClass::dateStrToValue(unsigned char *value, int size)
{
	char tmp[5];
	memcpy(tmp, value, size);
	tmp[size] = '\0';
	return (unsigned int)strtol(tmp, NULL, 10);
}

void DBClass::writeDate(FILE *fp, char *string, unsigned char *value, size_t value_size)
{
	unsigned int year,month,day,hour,min,second,ms,gmt;	
	year = dateStrToValue(value, 4);
	month = dateStrToValue(value+4, 2);
	day = dateStrToValue(value+6, 2);
	hour = dateStrToValue(value+8, 2);
	min = dateStrToValue(value+10, 2);
	second = dateStrToValue(value+12, 2);
	ms = dateStrToValue(value+14, 2);
	gmt=value[16];

	if (year && month && day)
	   fprintf(fp, "                    %-27s %02d/%02d/%04d %02d:%02d:%02d:%02d:%02d\n", string, day, month, year, hour, min, second, ms, gmt);
}

enum errorcode DBClass::saveSCR(const char *filename, bool oldTime)
{
   FILE *fp;
   enum errorcode err = ERR_NONE;
   bool hasCdda = false;

	doMode2=false;
	this->oldTime=oldTime;

   if ((fp = fopen(filename, "wt")) == NULL)
      return ERR_OPENWRITE;

   fprintf(fp, "Disc \"%s.DSK\"\n", stripEndWhiteSpace(pvd.VolumeIdentifier, sizeof(pvd.VolumeIdentifier)));
   switch (sessionType)
   {
      case ST_CDROM:
         fprintf(fp, "    Session CDROM\n");
         break;
      case ST_CDI:
         fprintf(fp, "    Session CDI\n");
         break;
      case ST_ROMXA:
         fprintf(fp, "    Session ROMXA\n");
         break;
      case ST_SEMIXA:
         fprintf(fp, "    Session SEMIXA\n");
         break;
      default: break;
   }
   fprintf(fp, "        LeadIn MODE1\n        EndLeadIn\n");
   fprintf(fp, "        SystemArea \"IP.BIN\"\n");
   fprintf(fp, "        Track MODE1\n");
   fprintf(fp, "            Volume ISO9660 \"%s.PVD\"\n", stripEndWhiteSpace(pvd.VolumeIdentifier, sizeof(pvd.VolumeIdentifier)));
   fprintf(fp, "                PrimaryVolume 0:2:16\n");
   fprintf(fp, "                    SystemIdentifier            \"%s\"\n", stripEndWhiteSpace(pvd.SystemIdentifier, sizeof(pvd.SystemIdentifier)));
   fprintf(fp, "                    VolumeIdentifier            \"%s\"\n", stripEndWhiteSpace(pvd.VolumeIdentifier, sizeof(pvd.VolumeIdentifier)));
   fprintf(fp, "                    VolumeSetIdentifier         \"%s\"\n", stripEndWhiteSpace(pvd.VolumeSetIdentifier, sizeof(pvd.VolumeSetIdentifier)));
   fprintf(fp, "                    PublisherIdentifier         \"%s\"\n", stripEndWhiteSpace(pvd.PublisherIdentifier, sizeof(pvd.PublisherIdentifier)));
   fprintf(fp, "                    DataPreparerIdentifier      \"%s\"\n", stripEndWhiteSpace(pvd.DataPreparerIdentifier, sizeof(pvd.DataPreparerIdentifier)));
   fprintf(fp, "                    CopyrightFileIdentifier     \"%s\"\n", stripEndWhiteSpace(pvd.CopyrightFileIdentifier, sizeof(pvd.CopyrightFileIdentifier)));
   fprintf(fp, "                    AbstractFileIdentifier      \"%s\"\n", stripEndWhiteSpace(pvd.AbstractFileIdentifier, sizeof(pvd.AbstractFileIdentifier)));
   fprintf(fp, "                    BibliographicFileIdentifier \"%s\"\n", stripEndWhiteSpace(pvd.BibliographicFileIdentifier, sizeof(pvd.BibliographicFileIdentifier)));

	if (oldTime)
	{
		writeDate(fp, "VolumeCreationDate", pvd.VolumeCreationDateAndTime, sizeof(pvd.VolumeCreationDateAndTime));
		writeDate(fp, "VolumeModificationDate", pvd.VolumeModificationDateAndTime, sizeof(pvd.VolumeModificationDateAndTime));
		writeDate(fp, "VolumeExpirationDate", pvd.VolumeExpirationDateAndTime, sizeof(pvd.VolumeExpirationDateAndTime));
		writeDate(fp, "VolumeEffectiveDate", pvd.VolumeEffectiveDateAndTime, sizeof(pvd.VolumeEffectiveDateAndTime));
	}

   fprintf(fp, "                EndPrimaryVolume\n");
   fprintf(fp, "            EndVolume\n\n");

	if (oldTime)
	{
		volumedatetime_struct vdt = filelist[0].getDateTime();
		//fprintf(fp, "            RecordingDate %02d/%02d/%04d %02d:%02d:%02d:%02d:%02d\n", vdt.Day, vdt.Month, 1900+vdt.Year, vdt.Hour, vdt.Minute, vdt.Second, 0, vdt.Zone);
	}

	// Mode 1 Track
	doDirectoryMode1(fp, 0xFFFFFFFF, 3);

	// If there's any Mode 2 files, do a Mode 2 track
	if (doMode2)
	{
		fprintf(fp, "            PostGap 75\n");
		fprintf(fp, "        EndTrack\n");

		fprintf(fp, "        Track MODE2\n");
		fprintf(fp, "            PreGap 150\n");
		doDirectoryMode2(fp, 0xFFFFFFFF, 3);
	}

   fprintf(fp, "            PostGap 150\n");
   fprintf(fp, "        EndTrack\n");

   // Add CDDA tracks
   for (uint32_t i = 0; i < tracklist.size(); i++)
   {
      if (tracklist[i].getFlags() != TT_CDDA)
         continue;
      hasCdda = true;
      fprintf(fp, "        Track CDDA\n");
      
      // Find file in table here
      if (strcmp(tracklist[i].getFilename(), "") != 0)
      {
         // Use table filename
         fprintf(fp, "            File %s\n", tracklist[i].getFilename());
			if (oldTime)
			{
				volumedatetime_struct vdt = tracklist[i].getDateTime();
				fprintf(fp, "                RecordingDate %02d/%02d/%04d %02d:%02d:%02d:%02d:%02d\n", vdt.Day, vdt.Month, 1900+vdt.Year, vdt.Hour, vdt.Minute, vdt.Second, 0, vdt.Zone);
			}
         fprintf(fp, "                FileSource \"%s\"\n", tracklist[i].getRealFilename());
         fprintf(fp, "                EndFileSource\n");
         fprintf(fp, "            EndFile\n");
      }
      else
      {
         // None exists, just create the track
         fprintf(fp, "            FileSource \"%s\"\n", tracklist[i].getRealFilename());
         fprintf(fp, "            EndFileSource\n");
      }

      fprintf(fp, "        EndTrack\n");
   }

   if (hasCdda) {
      fprintf(fp, "        LeadOut CDDA\n");
   } else if (doMode2) {
      fprintf(fp, "        LeadOut MODE2\n");
   } else {
      fprintf(fp, "        LeadOut MODE1\n");
   }
   fprintf(fp, "            Empty 600\n");
   fprintf(fp, "        EndLeadOut\n");
   fprintf(fp, "    EndSession\n");
   fprintf(fp, "EndDisc\n");

   fclose(fp);

   return err;
}

bool DBClass::changeFileFlags()
{
   return true;
}

bool DBClass::saveDiscLayout(const char *filename)
{
   return save(filename);
}

char *DBClass::getDLFDirectory()
{
   return dlfdir;
}

void DBClass::setDLFDirectory( const char *dlfdir )
{
   strcpy(this->dlfdir, dlfdir);
}

char *DBClass::getIPFilename()
{
   return ipfilename;
}

void DBClass::setIPFilename( const char *filename )
{
   strcpy(ipfilename, filename);
}

void DBClass::setPVD( pvd_struct * pvd )
{
   memcpy(&this->pvd, pvd, sizeof(pvd_struct));
}

void DBClass::addFile( dirrec_struct * dirrec, int i, ISOExtractClass *iec )
{
   filelist.push_back(FileListClass());
   uint32_t cur=(uint32_t)filelist.size()-1;
   
   filelist[cur].setFilename((const char *)dirrec[i].FileIdentifier);
   filelist[cur].setLBA(dirrec[i].LocationOfExtentL);
   filelist[cur].setSize(dirrec[i].DataLengthL);
   filelist[cur].setFlags(dirrec[i].FileFlags);
	filelist[cur].setDateTime(dirrec[i].RecordingDateAndTime);

   // Calculate new parent
   if (dirrec[i].ParentRecord == -1)
      filelist[cur].setParent(-1);
   else
   {
      //for (uint32_t k = 0; k <= dirrec[i].ParentRecord; k++)
		for (uint32_t k = 0; k < filelist.size(); k++)
      {
         if (strcmp(filelist[k].getFilename(), (char *)dirrec[dirrec[i].ParentRecord].FileIdentifier) == 0 &&
            filelist[k].getLBA() == dirrec[dirrec[i].ParentRecord].LocationOfExtentL &&
            filelist[k].getSize() == dirrec[dirrec[i].ParentRecord].DataLengthL &&
            (filelist[k].getFlags() & 0xFF) == dirrec[dirrec[i].ParentRecord].FileFlags)
            filelist[cur].setParent(k);
      }
   }

   char realFilename[PATH_MAX], path[PATH_MAX], temp[PATH_MAX];
   strcpy(realFilename, "Files\\");

   // Go through parents and create path
   strcpy(path, "");
   uint32_t parent=cur;
   for (int j = 0; j < 8; j++)
   {
      parent=filelist[parent].getParent();
      if (parent == 0xFFFFFFFF)
         break;

      strcpy(temp, path);
      strcpy(path, filelist[parent].getFilename());
      strcat(path, "\\");
      strcat(path, temp);
   }

   strcat(realFilename, path);
   strcat(realFilename, (const char *)dirrec[i].FileIdentifier);
   if (char *p = strchr(realFilename, ';'))
      p[0] = '\0';
   filelist[cur].setRealFilename(realFilename);

   if (dirrec[i].XAAttributes.attributes != 0)
   {
      if (dirrec[i].XAAttributes.attributes == 0x4111)
         filelist[cur].setFlags(dirrec[i].FileFlags | FF_CDDA);
		else if (dirrec[i].XAAttributes.attributes & XAATTR_DIR)
			filelist[cur].setFlags(dirrec[i].FileFlags | FF_MODE2);
		else if (dirrec[i].XAAttributes.attributes & XAATTR_FORM2)
		{
			int readsize=0;
			xa_subheader_struct subheader[2];
			iec->readSectorSubheader(dirrec[i].LocationOfExtentL+150, subheader);
			iec->readSectorSubheader(dirrec[i].LocationOfExtentL+151, subheader+1);

			filelist[cur].setFlags(dirrec[i].FileFlags | FF_MODE2);

			if (subheader[0].sm & XAFLAG_REALTIME)
				filelist[cur].setFlags(filelist[cur].getFlags() | FF_REALTIME);

			if (subheader[0].sm & XAFLAG_AUDIO && subheader[0].ci == 0x7F)
			{
				// MPEG Audio
				filelist[cur].setFlags(filelist[cur].getFlags() | FF_MPEG | FF_AUDIO);
				if (subheader[1].sm & XAFLAG_VIDEO && subheader[1].ci == 0x0F)
					// MPEG Video -> ISO11172
					filelist[cur].setFlags(filelist[cur].getFlags() | FF_VIDEO);
			}
			else if (subheader[0].sm & XAFLAG_VIDEO && subheader[0].ci == 0x0F)
			{
			   filelist[cur].setFlags(filelist[cur].getFlags() | FF_VIDEO | FF_MPEG);

#ifdef HAVE_FFMPEG
            MPEGDemuxClass demux;
            demux.openStream(iec, dirrec[i].LocationOfExtentL, dirrec[i].DataLengthL / 2048);
            filelist[cur].setVideoBitrate(demux.getVideoBitrate());
            filelist[cur].setAudioBitrate(demux.getAudioBitrate());
            demux.closeStream();
#else
            filelist[cur].setSourceType(FileListClass::ST_ISO11172);
#endif
				if (subheader[1].sm & XAFLAG_AUDIO && subheader[1].ci == 0x7F)
					// MPEG Video -> ISO11172
					filelist[cur].setFlags(filelist[cur].getFlags() | FF_AUDIO);
			}
			else if (subheader[0].sm & XAFLAG_VIDEO)
				filelist[cur].setFlags(filelist[cur].getFlags() | FF_VIDEO);
			else if (subheader[0].sm & XAFLAG_AUDIO)
			{
				// ADPCM audio
				filelist[cur].setFlags(filelist[cur].getFlags() | FF_AUDIO);

				if ((subheader[0].ci & 0xAA) == 0)
				{
					bool stereo=subheader[0].ci & 0x1;
					bool modeA=(subheader[0].ci >> 4) & 0x1;
					bool s18k=(subheader[0].ci >> 2) & 0x1;

					if (stereo)
					{
						if (modeA)
						   filelist[cur].setSourceType(FileListClass::ST_STEREO_A);
						else
							filelist[cur].setSourceType(s18k ? FileListClass::ST_STEREO_C : FileListClass::ST_STEREO_B);
					}
					else
					{
						if (modeA)
							filelist[cur].setSourceType(FileListClass::ST_MONO_A);
						else
							filelist[cur].setSourceType(s18k ? FileListClass::ST_MONO_C : FileListClass::ST_MONO_B);
					}

					// since we can identify the source, we don't need to keep the ci
					//subheader[0].ci = 0;
				}
			}
			if (subheader[0].sm & XAFLAG_FORM2)
				filelist[cur].setFlags(filelist[cur].getFlags() | FF_FORM2);
			if (subheader[0].fn > 0)
				filelist[cur].setFlags(filelist[cur].getFlags() | FF_INTERLEAVE);

			filelist[cur].setCodingInformation(subheader[0].ci);
			if (subheader[0].ci)
				filelist[cur].setFlags(filelist[cur].getFlags() | FF_CODINGINFO);
		}
      sessionType = ST_SEMIXA;
   }
}

void DBClass::setFileNumber( uint32_t num )
{
   filelistnum = num;
}

void DBClass::clearFiles()
{
   filelist.clear();
}

void DBClass::addTrack( trackinfo_struct *trackinfo, int i)
{
   tracklist.push_back(FileListClass());
   uint32_t cur=(uint32_t)tracklist.size()-1;
   char realFilename[PATH_MAX];

   sprintf(realFilename, "CDDA\\track%02d.bin", i+1);
   tracklist[cur].setRealFilename(realFilename);
   tracklist[cur].setFilename("");
   for (uint32_t j = 0; j < filelist.size(); j++)
   {
      if (filelist[j].getLBA()+150 == trackinfo->fadstart)
		{
         tracklist[cur].setFilename(filelist[j].getFilename());
			if (oldTime)
				tracklist[cur].setDateTime(filelist[j].getDateTime());
		}
   }

   tracklist[cur].setLBA(trackinfo->fadstart);
   tracklist[cur].setFlags(trackinfo->type);
}

void DBClass::clearTracks()
{
   tracklist.clear();
}
