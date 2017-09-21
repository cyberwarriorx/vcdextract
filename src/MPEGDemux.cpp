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

#include "MPEGDemux.h"

MPEGDemuxClass::MPEGDemuxClass()
{
   ifmtCtx = NULL;
   ioCtx = NULL;

   // register all formats and codecs
   av_register_all();
}

MPEGDemuxClass::~MPEGDemuxClass()
{

}

int ReadFunc(void* ptr, uint8_t* buf, int buf_size)
{
   MPEGDemuxClass *demux = reinterpret_cast<MPEGDemuxClass*>(ptr);
   int readsize=0;
   sectorinfo_struct sectorInfo;

   if (!(demux->getISOExtract())->readUserSector(demux->getLBA(), buf, &readsize, NULL, &sectorInfo))
      return -1;
   demux->setLBA(demux->getLBA()+1);
   if(demux->getLBA() >= demux->getLBAEnd())
      return AVERROR_EOF;  // Let FFmpeg know that we have reached eof
   return readsize;
}

int64_t SeekFunc(void* ptr, int64_t pos, int whence)
{
   MPEGDemuxClass *demux = reinterpret_cast<MPEGDemuxClass*>(ptr);

   switch (whence)
   {
   case SEEK_SET:
      demux->setPosition((int)pos);
      break;
   case SEEK_CUR:      
      pos = demux->getPosition() + pos;
      demux->setPosition((int)pos);
      break;
   case SEEK_END:
      pos = demux->getDataSize()-pos;
      demux->setPosition((int)pos);
      break;
   case AVSEEK_SIZE:
      // File size      
      return demux->getDataSize();
   }

   // Return the new position:
   return demux->getPosition();
}

int MPEGDemuxClass::openStream(ISOExtractClass *ISOExtract, int lba, int size)
{
   int ret;

   this->ISOExtract = ISOExtract;
   this->lbaStart = this->lba = lba;
   this->lbaEnd = lba+size;

   uint8_t *avio_ctx_buffer = NULL;
   int avio_ctx_buffer_size=2352 * 2;
   avio_ctx_buffer = (uint8_t *)av_malloc(avio_ctx_buffer_size);
   if (!avio_ctx_buffer) 
      return ERR_ALLOC;

   // Open mpeg context
   if ((ioCtx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size, 0, this, ReadFunc, NULL, SeekFunc)) == NULL)
   {
      ret = ERR_ALLOC;
      goto error;
   }      

   // Allocate the AVFormatContext
   if ((ifmtCtx = avformat_alloc_context()) == NULL)
   {
      ret = ERR_ALLOC;
      goto error;
   }      

   // Set the IOContext
   ifmtCtx->pb = ioCtx;

   // Determining the input format
   int readsize;
   memset (avio_ctx_buffer, 0, avio_ctx_buffer_size);
   if (!ISOExtract->readUserSector(lba, avio_ctx_buffer, &readsize, NULL, &sectorInfo))
   {
      printf("Error reading mpeg data\n");
      ret = ERR_READ;
      goto error;
   }

   // Now we set the ProbeData-structure for av_probe_input_format
   AVProbeData probeData;
   probeData.buf = avio_ctx_buffer;
   probeData.buf_size = readsize;
   probeData.filename = "";
   probeData.mime_type = "";

   // Determine the input-format:
   ifmtCtx->iformat = av_probe_input_format(&probeData, 1);
   ifmtCtx->flags = AVFMT_FLAG_CUSTOM_IO;

   if((ret = avformat_open_input(&ifmtCtx, "", NULL, NULL)) != 0)
   {
      printf("Could not open mpeg data\n");
      ret = ERR_OPENREAD;
      goto error;
   }

   // Get stream info
   if (avformat_find_stream_info(ifmtCtx, NULL) < 0) 
   {
      printf("Could not find stream information\n");
      ret = ERR_STREAMINFO;
      goto error;
   }

   return ERR_NONE;
error:
   avformat_close_input(&ifmtCtx);
   av_free(ioCtx);   
   return ret;
}

void MPEGDemuxClass::closeStream()
{
   // Close input
   avformat_close_input(&ifmtCtx);
   if (ioCtx)
      av_free(ioCtx);   
}

int MPEGDemuxClass::demux(char *videoFilename, char *audioFilename)
{
   int ret = ERR_NONE;

   if (ifmtCtx == NULL)
   {
      printf("Data not loaded\n");
      return ERR_DATANOTLOADED;
   }

   AVFormatContext *vfmt_ctx = NULL;
   avformat_alloc_output_context2(&vfmt_ctx, NULL, NULL, videoFilename);
   if (!vfmt_ctx) 
   {
      printf("Could not create video output context\n");
      ret = ERR_ALLOC;
      goto end;
   }

   AVFormatContext *afmt_ctx = NULL;
   avformat_alloc_output_context2(&afmt_ctx, NULL, NULL, audioFilename);
   if (!afmt_ctx) 
   {
      printf("Could not create audio output context\n");
      ret = ERR_ALLOC;
      goto end;
   }

   int stream_mapping_size = ifmtCtx->nb_streams;
   int *stream_mapping = (int *)av_mallocz_array(stream_mapping_size, sizeof(*stream_mapping));
   if (!stream_mapping) 
   {
      ret = ERR_ALLOC;
      goto end;
   }

   AVOutputFormat *vfmt = vfmt_ctx->oformat;
   AVOutputFormat *afmt = afmt_ctx->oformat;

   int vstream_index = 0;
   int astream_index = 0;
   for (unsigned int i = 0; i < ifmtCtx->nb_streams; i++) 
   {
      AVStream *in_stream = ifmtCtx->streams[i];
      AVCodecParameters *in_codecpar = in_stream->codecpar;

      if (in_codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
      {
         stream_mapping[i] = (in_codecpar->codec_type << 8) | vstream_index++;

         AVStream *vid_stream = avformat_new_stream(vfmt_ctx, NULL);
         if (!vid_stream) 
         {
            printf("Failed allocating video output stream\n");
            ret = ERR_ALLOC;
            goto end;
         }

         ret = avcodec_parameters_copy(vid_stream->codecpar, in_codecpar);
         if (ret < 0) 
         {
            printf("Failed to copy codec parameters\n");
            ret = ERR_ALLOC;
            goto end;
         }
         vid_stream->codecpar->codec_tag = 0;
      }
      else if (in_codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
      {
         stream_mapping[i] = (in_codecpar->codec_type << 8) | astream_index++;

         AVStream *aud_stream = avformat_new_stream(afmt_ctx, NULL);
         if (!aud_stream) {
            printf("Failed allocating audio output stream\n");
            ret = ERR_ALLOC;
            goto end;
         }

         ret = avcodec_parameters_copy(aud_stream->codecpar, in_codecpar);
         if (ret < 0) 
         {
            printf("Failed to copy codec parameters\n");
            ret = ERR_COPY;
            goto end;
         }
         aud_stream->codecpar->codec_tag = 0;
      }
      else
      {
         stream_mapping[i] = -1;
         continue;
      }
   }

   if (!(vfmt->flags & AVFMT_NOFILE)) 
   {
      ret = avio_open(&vfmt_ctx->pb, videoFilename, AVIO_FLAG_WRITE);
      if (ret < 0) 
      {
         printf("Could not open output file %s", videoFilename);
         ret = ERR_OPENWRITE;
         goto end;
      }
   }

   if (!(afmt->flags & AVFMT_NOFILE)) 
   {
      ret = avio_open(&afmt_ctx->pb, audioFilename, AVIO_FLAG_WRITE);
      if (ret < 0) 
      {
         printf("Could not open output file '%s'", audioFilename);
         ret = ERR_OPENWRITE;
         goto end;
      }
   }

   ret = avformat_write_header(vfmt_ctx, NULL);
   if (ret < 0) 
   {
      printf("Error occurred when opening output video file\n");
      ret = ERR_WRITE;
      goto end;
   }

   ret = avformat_write_header(afmt_ctx, NULL);
   if (ret < 0) 
   {
      printf("Error occurred when opening output audio file\n");
      ret = ERR_WRITE;
      goto end;
   }

   AVPacket pkt;
   while (1) 
   {
      AVStream *in_stream, *out_stream;

      ret = av_read_frame(ifmtCtx, &pkt);
      if (ret < 0)
         break;

      in_stream  = ifmtCtx->streams[pkt.stream_index];
      if (pkt.stream_index >= stream_mapping_size ||
         stream_mapping[pkt.stream_index] < 0) 
      {
         av_packet_unref(&pkt);
         continue;
      }

      int stream_index = pkt.stream_index;
      pkt.stream_index = stream_mapping[stream_index] & 0xFF;
      int type = stream_mapping[stream_index] >> 8;
      if (type == AVMEDIA_TYPE_VIDEO)
         out_stream = vfmt_ctx->streams[pkt.stream_index];
      else if (type == AVMEDIA_TYPE_AUDIO)
         out_stream = afmt_ctx->streams[pkt.stream_index];

      // copy packet
      pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
      pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
      pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
      pkt.pos = -1;
      if (type == AVMEDIA_TYPE_VIDEO)
         ret = av_interleaved_write_frame(vfmt_ctx, &pkt);
      else if (type == AVMEDIA_TYPE_AUDIO)
         ret = av_interleaved_write_frame(afmt_ctx, &pkt);

      if (ret < 0) 
      {
         printf("Error muxing packet\n");
         break;
      }
      av_packet_unref(&pkt);
   }

   av_write_trailer(vfmt_ctx);
   av_write_trailer(afmt_ctx);
end:
   if (vfmt_ctx && !(vfmt->flags & AVFMT_NOFILE))
      avio_closep(&vfmt_ctx->pb);
   if (afmt_ctx && !(afmt->flags & AVFMT_NOFILE))
      avio_closep(&afmt_ctx->pb);
   avformat_free_context(vfmt_ctx);
   avformat_free_context(afmt_ctx);

   av_freep(&stream_mapping);
   return ret;
}

ISOExtractClass *MPEGDemuxClass::getISOExtract()
{
   return ISOExtract;
}

void MPEGDemuxClass::setLBA(int lba)
{
   this->lba = lba;
}

int MPEGDemuxClass::getLBA()
{
   return lba;
}

int MPEGDemuxClass::getLBAStart()
{
   return lbaStart;
}

int MPEGDemuxClass::getLBAEnd()
{
   return lbaEnd;
}

int MPEGDemuxClass::getSectorSize()
{
   return (sectorInfo.subheader.sm & XAFLAG_FORM2) ? 2324 : 2048;
}

int MPEGDemuxClass::getDataSize()
{
   return (lbaEnd-lbaStart)*getSectorSize();
}

void MPEGDemuxClass::setPosition(int pos)
{
   lba = lbaStart + (int)(pos / getSectorSize());
}

int MPEGDemuxClass::getPosition()
{
   return lba * getSectorSize();
}

uint32_t MPEGDemuxClass::getVideoBitrate()
{
   if (ifmtCtx)
   {
      for (unsigned int i = 0; i < ifmtCtx->nb_streams; i++) 
      {
         AVStream *in_stream = ifmtCtx->streams[i];
         AVCodecParameters *in_codecpar = in_stream->codecpar;

         if (in_codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            return (uint32_t)in_codecpar->bit_rate;
      }
   }

   return 0;
}

uint32_t MPEGDemuxClass::getAudioBitrate()
{
   if (ifmtCtx)
   {
      for (unsigned int i = 0; i < ifmtCtx->nb_streams; i++) 
      {
         AVStream *in_stream = ifmtCtx->streams[i];
         AVCodecParameters *in_codecpar = in_stream->codecpar;

         if (in_codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            return (uint32_t)in_codecpar->bit_rate;
      }
   }

   return 0;
}
