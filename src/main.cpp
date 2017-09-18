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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include "ISOExtract.h"

#pragma comment(linker, "/SUBSYSTEM:CONSOLE")

void usage()
{
   printf("VCDEXTRACT v%s by Cyber Warrior X\n", VERSION);
   printf("usage: vcdextract [--oldtime] [--detailedstatus] [--extract=<output directory>] <filename/cdrom path>");
}

int main( int argc, char** argv )
{
   char outPath[PATH_MAX];
   char discPath[PATH_MAX];
   bool isDisc=false;
   bool extractFiles=false;
	bool oldTime=false;
	bool detailedStatus = false;
	ISOExtractClass::SORTTYPE sortType = ISOExtractClass::SORT_BY_DIRREC;
   enum errorcode err;

   if (argc < 2)
   {
      usage();
      exit(1);
   }

   getcwd(outPath, sizeof(outPath)/sizeof(outPath[0]));

   for (int i = 1; i < argc; i++)
   {
      if (strncmp(argv[i], "--extract=", strlen("--extract=")) == 0)
      {
         extractFiles = true;
         strcpy(outPath, argv[i]+strlen("--extract="));
      }
		else if (strncmp(argv[i], "--oldtime", strlen("--oldtime")) == 0)
			oldTime = true;
		else if (strncmp(argv[i], "--detailedstatus", strlen("--detailedstatus")) == 0)
			detailedStatus = true;
		else if (strncmp(argv[i], "--sortbylba", strlen("--sortbylba")) == 0)
			sortType = ISOExtractClass::SORT_BY_LBA;
		else
		{
         struct stat info;
         if (stat(argv[i], &info) == 0)
         {
            isDisc = !(info.st_mode & S_IFDIR) ? true : false;
            strcpy(discPath, argv[i]);
         }           
		}
   }

   DBClass curdb;
   ISOExtractClass iec;

   printf("Extracting disc %s to %s...\n", discPath, outPath);
	iec.setMaintainOldTime(oldTime);
	iec.setSortType(sortType);
	iec.setDetailedStatus(detailedStatus);

   if ((err = iec.importDisc(discPath, outPath, &curdb)) != ERR_NONE)
   {
      printf("Couldn't load disc. Error code = %d\n", err);
      exit(1);
   }

   printf("Extraction finished.\n");

   exit(0);
	return 0;
}
