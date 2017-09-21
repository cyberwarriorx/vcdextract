/*  Copyright 2017 Theo Berkau

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

extern "C" {
#include <stdio.h>
#define __STDC_CONSTANT_MACROS
#define __STDC_FORMAT_MACROS
#define snprintf
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
};

#include "ISOExtract.h"

class MPEGDemuxClass
{
private:
   ISOExtractClass *ISOExtract;
   AVFormatContext *ifmtCtx;
   AVIOContext* ioCtx;
   sectorinfo_struct sectorInfo;
   int lba;
   int lbaStart;
   int lbaEnd;

public:
   MPEGDemuxClass();
   ~MPEGDemuxClass();
   int openStream(ISOExtractClass *ISOExtract, int lba, int size);
   void closeStream();
   void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag);
   int demux(char *videoFilename, char *audioFilename);
   ISOExtractClass *getISOExtract();
   void setLBA(int lba);
   int getLBA();
   int getLBAStart();
   int getLBAEnd();
   int getSectorSize();
   int getDataSize();
   void setPosition(int pos);
   int  getPosition();
   uint32_t getVideoBitrate();
   uint32_t getAudioBitrate();
};

