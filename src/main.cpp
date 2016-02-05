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

#include <stdio.h>
#include "ISOExtract.h"

#pragma comment(linker, "/SUBSYSTEM:CONSOLE")

void usage()
{
   printf("vcdextract v%s [--oldtime] [--extract=<output directory>] <filename/cdrom path>", VERSION);
}

int main( int argc, char** argv )
{
   char outPath[MAX_PATH];
   char discPath[MAX_PATH];
   bool isDisc=false;
   bool extractFiles=false;
	bool oldTime=false;
	ISOExtractClass::SORTTYPE sortType = ISOExtractClass::SORT_BY_DIRREC;

   if (argc < 2)
   {
      usage();
      exit(1);
   }

   for (int i = 1; i < argc; i++)
   {
      if (strncmp(argv[i], "--extract=", strlen("--extract=")) == 0)
      {
         extractFiles = true;
         strcpy(outPath, argv[i]+strlen("--extract="));
      }
		else if (strncmp(argv[i], "--oldtime", strlen("--oldtime")) == 0)
			oldTime = true;
		else if (strncmp(argv[i], "--sortbylba", strlen("--sortbylba")) == 0)
			sortType = ISOExtractClass::SORT_BY_LBA;
		else
		{
			DWORD ret = GetFileAttributesA(argv[i]);

			isDisc = (ret != INVALID_FILE_ATTRIBUTES && (ret & FILE_ATTRIBUTE_DIRECTORY));
			strcpy(discPath, argv[i]);
		}
   }

   DBClass curdb;
   ISOExtractClass iec;

   printf("Extracting disc %s to %s...\n", discPath, outPath);
	iec.setMaintainOldTime(oldTime);

	iec.setSortType(sortType);

   if (!iec.importDisc(discPath, outPath, &curdb))
   {
      printf("Couldn't load disc.\n");
      exit(1);
   }

   printf("done.\n");

   exit(0);
	return 0;
}
