/* Copyright (C)2002-2011 Jean-Marc Valin
   Copyright (C)2007-2012 Xiph.Org Foundation
   Copyright (C)2008-2012 Gregory Maxwell
   File: opusenc.c

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "OpusOgg.h"
#include "boost/format.hpp"

#include "config.h"

#ifdef XP_MACOSX
#include <Carbon/Carbon.h>
#else
#ifdef XP_UNIX
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#endif

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#if defined(XP_WIN)
#include <ctime>
#include <process.h>
#include "unicode_support.h"
/* We need the following two to set stdout to binary */
#include <io.h>
#include <fcntl.h>
#define I64FORMAT "I64d"
#else
# define I64FORMAT "lld"
# define fopen_utf8(_x,_y) fopen((_x),(_y))
# define argc_utf8 argc
# define argv_utf8 argv
#endif

#include <opus_multistream.h>
#include <ogg/ogg.h>

#include "opus_header.h"
#include "opusenc.h"
#include "diag_range.h"

#ifdef VALGRIND
#include <valgrind/memcheck.h>
#define VG_UNDEF(x,y) VALGRIND_MAKE_MEM_UNDEFINED((x),(y))
#define VG_CHECK(x,y) VALGRIND_CHECK_MEM_IS_DEFINED((x),(y))
#else
#define VG_UNDEF(x,y)
#define VG_CHECK(x,y)
#endif

#ifdef XP_MACOSX
#undef OPUS_SET_LSB_DEPTH
#endif

static void comment_init(char **comments, int* length, const char *vendor_string);
static void comment_add(char **comments, int* length, char *tag, char *val);

/*Write an Ogg page to a vector*/
static inline int oe_write_page(ogg_page *page, std::vector<unsigned char>& output)
{
   int written;
   unsigned char * ptr = page->header;
   int write;
   for(write=0; write < page->header_len; write++)
   {
    output.push_back(*ptr);
    ptr++;
   }
   written=page->header_len;

   ptr = page->body;
   for(write=0; write < page->body_len; write++)
   {
    output.push_back(*ptr);
    ptr++;
   }
   written += page->body_len;
   return written;
}

#define MAX_FRAME_BYTES 61295
#define IMIN(a,b) ((a) < (b) ? (a) : (b))   /**< Minimum int value.   */
#define IMAX(a,b) ((a) > (b) ? (a) : (b))   /**< Maximum int value.   */

void opustoolsversion(const char *opusversion)
{
  printf("opusenc %s %s (using %s)\n",PACKAGE,VERSION,opusversion);
  printf("Copyright (C) 2008-2012 Xiph.Org Foundation\n");
}

void opustoolsversion_short(const char *opusversion)
{
  printf("opusenc %s %s (using %s)\n",PACKAGE,VERSION,opusversion);
  printf("Copyright (C) 2008-2012 Xiph.Org Foundation\n");
}

bool OpusOgg::encode(opus_int32 sampling_rate, int channels, opus_int32 bitrate_bps, std::vector<unsigned char>& inputVec, std::vector<unsigned char>& outputVec, std::string& errMessage)
{
  char rawstr[] = "raw";
  char rawfilereaderstr[] = "RAW file reader";
  // PETE: changed to fix compiler - warning: deprecated conversion from string constant to ‘char*’
  // static const input_format raw_format = { NULL, 0, raw_open, wav_close, rawstr, N_("RAW file reader") };
  static const input_format raw_format = { NULL, 0, raw_open, wav_close, rawstr, rawfilereaderstr };
  int i, ret;
  OpusMSEncoder      *st;
  const char         *opus_version;
  unsigned char      *packet;
  float              *input;
  /*I/O*/
  oe_enc_opt         inopt;
  const input_format *in_format;
  ogg_stream_state   os;
  ogg_page           og;
  ogg_packet         op;
  ogg_int64_t        last_granulepos=0;
  ogg_int64_t        enc_granulepos=0;
  ogg_int64_t        original_samples=0;
  ogg_int32_t        id=-1;
  int                last_segments=0;
  int                eos=0;
  OpusHeader         header;
  int                comments_length = 0;
  char               ENCODER_string[64];
  char               *comments = NULL;
  /*Counters*/
  opus_int64         nb_encoded=0;
  opus_int64         bytes_written=0;
  opus_int64         pages_out=0;
  opus_int64         total_bytes=0;
  opus_int64         total_samples=0;
  opus_int32         nbBytes;
  opus_int32         nb_samples;
  opus_int32         peak_bytes=0;
  opus_int32         min_bytes;
  time_t             start_time;
  time_t             stop_time;
  /*Settings*/
  int                max_frame_bytes;
  opus_int32         rate=48000;
  opus_int32         coding_rate=48000;
  opus_int32         frame_size=960;
  int                with_hard_cbr=0;
  int                with_cvbr=0;
  int                expect_loss=0;
  int                complexity=10;
  int                downmix=0;
  int                uncoupled=0;
  int                max_ogg_delay=48000; /*48kHz samples*/
  opus_int32         lookahead=0;
  unsigned char      mapping[256];
  int                force_narrow=0;
#ifdef WIN_UNICODE
   int argc_utf8;
   char **argv_utf8;
#endif

  in_format=NULL;
  inopt.rate=coding_rate=rate;
  inopt.samplesize=16;
  inopt.endianness=0;
  inopt.rawmode=1;
  inopt.ignorelength=0;
  inopt.rate=sampling_rate;
  inopt.channels=channels;

  for(i=0;i<256;i++)mapping[i]=i;

  opus_version=opus_get_version_string();
  /*Vendor string should just be the encoder library,
    the ENCODER comment specifies the tool used.*/
  comment_init(&comments, &comments_length, opus_version);
  snprintf(ENCODER_string, sizeof(ENCODER_string), "opusenc from %s %s",PACKAGE,VERSION);
  char encoderstr [] = "ENCODER="; 
  // PETE: changed to fix compiler - warning: deprecated conversion from string constant to ‘char*’
  // comment_add(&comments, &comments_length, "ENCODER=", ENCODER_string);
  comment_add(&comments, &comments_length, encoderstr, ENCODER_string);

  frame_size=960;

  in_format = &raw_format;
  in_format->open_func(inputVec, &inopt, NULL, 0);

  if (downmix==0&&inopt.channels>2&&bitrate_bps>0&&bitrate_bps<(32000*inopt.channels))
  {
    downmix=inopt.channels>8?1:2;
  }

  if (downmix>0&&downmix<inopt.channels)
  {
    downmix=setup_downmix(&inopt,downmix);
  }
  else
  {
    downmix=0;
  }

  rate=inopt.rate;
  //chan=inopt.channels;
  inopt.skip=0;

  /*In order to code the complete length we'll need to do a little padding*/
  setup_padder(&inopt,&original_samples);

  if (rate>24000)
    coding_rate=48000;
  else if (rate>16000)
    coding_rate=24000;
  else if (rate>12000)
    coding_rate=16000;
  else if (rate>8000)
    coding_rate=12000;
  else 
    coding_rate=8000;

  frame_size=frame_size/(48000/coding_rate);

  /*Scale the resampler complexity, but only for 48000 output because
    the near-cutoff behavior matters a lot more at lower rates.*/
  if (rate!=coding_rate)setup_resample(&inopt,coding_rate==48000?(complexity+1)/2:5,coding_rate);

  /*OggOpus headers*/ /*FIXME: broke forcemono*/
  header.channels=channels;
  header.nb_streams=header.channels;
  header.nb_coupled=0;
  if (header.channels<=8&&!uncoupled)
  {
    static const unsigned char opusenc_streams[8][10]={
      /*Coupled, NB_bitmap, mapping...*/
      /*1*/ {0,   0, 0},
      /*2*/ {1,   0, 0,1},
      /*3*/ {1,   0, 0,2,1},
      /*4*/ {2,   0, 0,1,2,3},
      /*5*/ {2,   0, 0,4,1,2,3},
      /*6*/ {2,1<<3, 0,4,1,2,3,5},
      /*7*/ {2,1<<4, 0,4,1,2,3,5,6},
      /*6*/ {3,1<<4, 0,6,1,2,3,4,5,7}
    };
    for(i=0;i<header.channels;i++)mapping[i]=opusenc_streams[header.channels-1][i+2];
    force_narrow=opusenc_streams[header.channels-1][1];
    header.nb_coupled=opusenc_streams[header.channels-1][0];
    header.nb_streams=header.channels-header.nb_coupled;
  }
  header.channel_mapping=header.channels>8?255:header.nb_streams>1;
  if (header.channel_mapping>0)
    for(i=0;i<header.channels;i++)
      header.stream_map[i]=mapping[i];
  /* 0 dB gain is the recommended unless you know what you're doing */
  header.gain=0;
  header.input_sample_rate=rate;

  min_bytes=max_frame_bytes=(1275*3+7)*header.nb_streams;
  packet=(unsigned char*)malloc(sizeof(unsigned char)*max_frame_bytes);
  if (packet==NULL)
  {
  errMessage.assign("Error allocating packet buffer.");
  return(false);
  }

  /*Initialize OPUS encoder*/
  /*Framesizes <10ms can only use the MDCT modes, so we switch on RESTRICTED_LOWDELAY
    to save the extra 2.5ms of codec lookahead when we'll be using only small frames.*/
  st=opus_multistream_encoder_create(coding_rate, channels, header.nb_streams, header.nb_coupled,
     mapping, frame_size<480/(48000/coding_rate)?OPUS_APPLICATION_RESTRICTED_LOWDELAY:OPUS_APPLICATION_AUDIO, &ret);
  if (ret!=OPUS_OK)
  {
    errMessage.assign("Error cannot create encoder: ");
  errMessage.append(opus_strerror(ret));
  return(false);
  }

  if (force_narrow!=0)
  {
    for(i=0;i<header.nb_streams;i++)
    {
      if (force_narrow&(1<<i))
      {
        OpusEncoder *oe;
        opus_multistream_encoder_ctl(st,OPUS_MULTISTREAM_GET_ENCODER_STATE(i,&oe));
        ret=opus_encoder_ctl(oe, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_NARROWBAND));
        if (ret!=OPUS_OK)
        {
      errMessage.assign("Error OPUS_SET_MAX_BANDWIDTH returned:");
      errMessage.append(opus_strerror(ret));
          return(false);
        }
      }
    }
  }

  if (bitrate_bps<0)
  {
    /*Lower default rate for sampling rates [8000-44100) by a factor of (rate+16k)/(64k)*/
    bitrate_bps=((64000*header.nb_streams+32000*header.nb_coupled)*
             (IMIN(48,IMAX(8,((rate<44100?rate:48000)+1000)/1000))+16)+32)>>6;
  }

  if (bitrate_bps>(1024000*channels)||bitrate_bps<500)
  {
    errMessage.assign("Error: Invalid Bitrate");
    return(false);
  }
  bitrate_bps=IMIN(channels*256000,bitrate_bps);

  ret=opus_multistream_encoder_ctl(st, OPUS_SET_BITRATE(bitrate_bps));
  if (ret!=OPUS_OK)
  {
    errMessage.assign("Error OPUS_SET_BITRATE returned: ");
  errMessage.append(opus_strerror(ret));
    return(false);
  }

  ret=opus_multistream_encoder_ctl(st, OPUS_SET_VBR(!with_hard_cbr));
  if (ret!=OPUS_OK)
  {
    errMessage.assign("Error OPUS_SET_VBR returned: ");
  errMessage.append(opus_strerror(ret));
    return(false);
  }

  if (!with_hard_cbr)
  {
    ret=opus_multistream_encoder_ctl(st, OPUS_SET_VBR_CONSTRAINT(with_cvbr));
    if (ret!=OPUS_OK)
    {
      errMessage.assign("Error OPUS_SET_VBR_CONSTRAINT returned: ");
    errMessage.append(opus_strerror(ret));
    return(false);
    }
  }

  ret=opus_multistream_encoder_ctl(st, OPUS_SET_COMPLEXITY(complexity));
  if (ret!=OPUS_OK)
  {
    errMessage.assign("Error OPUS_SET_COMPLEXITY returned: ");
  errMessage.append(opus_strerror(ret));
    return(false);
  }

  ret=opus_multistream_encoder_ctl(st, OPUS_SET_PACKET_LOSS_PERC(expect_loss));
  if (ret!=OPUS_OK)
  {
    errMessage.assign("Error OPUS_SET_PACKET_LOSS_PERC returned: ");
  errMessage.append(opus_strerror(ret));
    return(false);
  }

#ifdef OPUS_SET_LSB_DEPTH
  ret=opus_multistream_encoder_ctl(st, OPUS_SET_LSB_DEPTH(IMAX(8,IMIN(24,inopt.samplesize))));
  if (ret!=OPUS_OK)
  {
    errMessage.assign("Warning OPUS_SET_LSB_DEPTH returned: ");
  errMessage.append(opus_strerror(ret));
    return(false);
  }
#endif

  /*We do the lookahead check late so user CTLs can change it*/
  ret=opus_multistream_encoder_ctl(st, OPUS_GET_LOOKAHEAD(&lookahead));
  if (ret!=OPUS_OK)
  {
    errMessage.assign("Error OPUS_GET_LOOKAHEAD returned: ");
  errMessage.append(opus_strerror(ret));
  return(false);
  }
  inopt.skip+=lookahead;
  /*Regardless of the rate we're coding at the ogg timestamping/skip is
    always timed at 48000.*/
  header.preskip=inopt.skip*(48000./coding_rate);
  /* Extra samples that need to be read to compensate for the pre-skip */
  inopt.extraout=(int)header.preskip*(rate/48000.);

  /*Initialize Ogg stream struct*/
  start_time = time(NULL);
#ifdef XP_MACOSX
  ProcessSerialNumber curPSN;
  pid_t curPID = 0;
  GetProcessPID(&curPSN, &curPID);
  srand(((curPID&65535)<<15)^start_time);
#else
  srand(((getpid()&65535)<<15)^start_time);
#endif
  if (ogg_stream_init(&os, rand())==-1)
  {
    errMessage.assign("Error: stream init failed\n");
    return(false);
  }

  /*Write header*/
  {
  unsigned char header_data[100];
  int packet_size=opus_header_to_packet(&header, header_data, 100);
  op.packet=header_data;
  op.bytes=packet_size;
  op.b_o_s=1;
  op.e_o_s=0;
  op.granulepos=0;
  op.packetno=0;
  ogg_stream_packetin(&os, &op);

  while((ret=ogg_stream_flush(&os, &og)))
  {
    if (!ret)break;
    ret=oe_write_page(&og, outputVec);
    if (ret!=og.header_len+og.body_len)
    {
      errMessage.assign("Error: failed writing header to output stream");
      return(false);
    }
    bytes_written+=ret;
    pages_out++;
  }

  op.packet=(unsigned char *)comments;
  op.bytes=comments_length;
  op.b_o_s=0;
  op.e_o_s=0;
  op.granulepos=0;
  op.packetno=1;
  ogg_stream_packetin(&os, &op);
  }

  /* writing the rest of the opus header packets */
  while((ret=ogg_stream_flush(&os, &og)))
  {
    if (!ret)break;
    ret=oe_write_page(&og, outputVec);
    if (ret!=og.header_len + og.body_len)
    {
      errMessage.assign("Error: failed writing header to output stream");
      return(false);
    }
    bytes_written+=ret;
    pages_out++;
  }

  free(comments);

  input=(float*)malloc(sizeof(float)*frame_size*channels);
  if (input==NULL)
  {
    errMessage.assign("Error: couldn't allocate sample buffer.");
    return(false);
  }

  /*Main encoding loop (one frame per iteration)*/
  eos=0;
  nb_samples=-1;
  while(!op.e_o_s)
  {
    int size_segments,cur_frame_size;
    id++;

    if (nb_samples<0)
    {
      nb_samples = inopt.read_samples(inopt.readdata,input,frame_size);
      total_samples+=nb_samples;
      if (nb_samples<frame_size)op.e_o_s=1;
      else op.e_o_s=0;
    }
    op.e_o_s|=eos;

    if (start_time==0)
    {
      start_time = time(NULL);
    }

    cur_frame_size=frame_size;

    /*No fancy end padding, just fill with zeros for now.*/
    if (nb_samples<cur_frame_size)for(i=nb_samples*channels;i<cur_frame_size*channels;i++)input[i]=0;

    /*Encode current frame*/
    VG_UNDEF(packet,max_frame_bytes);
    VG_CHECK(input,sizeof(float)*channels*cur_frame_size);
    nbBytes=opus_multistream_encode_float(st, input, cur_frame_size, packet, max_frame_bytes);
    if (nbBytes<0)
    {
      errMessage.assign( "Encoding failed.  Error: ");
    errMessage.append(opus_strerror(nbBytes));
      break;
    }
    VG_CHECK(packet,nbBytes);
    VG_UNDEF(input,sizeof(float)*channels*cur_frame_size);
    nb_encoded+=cur_frame_size;
    enc_granulepos+=cur_frame_size*48000/coding_rate;
    total_bytes+=nbBytes;
    size_segments=(nbBytes+255)/255;
    peak_bytes=IMAX(nbBytes,peak_bytes);
    min_bytes=IMIN(nbBytes,min_bytes);

    /*Flush early if adding this packet would make us end up with a
      continued page which we wouldn't have otherwise.*/
    while((((size_segments<=255)&&(last_segments+size_segments>255))||
           (enc_granulepos-last_granulepos>max_ogg_delay))&&
#ifdef OLD_LIBOGG
           ogg_stream_flush(&os, &og))
           {
#else
           ogg_stream_flush_fill(&os, &og,255*255))
           {
#endif
      if (ogg_page_packets(&og)!=0)last_granulepos=ogg_page_granulepos(&og);
      last_segments-=og.header[26];
      ret=oe_write_page(&og, outputVec);
      if (ret!=og.header_len+og.body_len)
      {
         errMessage.assign("Error: failed writing data to output stream\n");
         return false;
      }
      bytes_written+=ret;
      pages_out++;
    }

    /*The downside of early reading is if the input is an exact
      multiple of the frame_size you'll get an extra frame that needs
      to get cropped off. The downside of late reading is added delay.
      If your ogg_delay is 120ms or less we'll assume you want the
      low delay behavior.*/
    if ((!op.e_o_s)&&max_ogg_delay>5760)
    {
      nb_samples = inopt.read_samples(inopt.readdata,input,frame_size);
      total_samples+=nb_samples;
      if (nb_samples<frame_size)eos=1;
      if (nb_samples==0)op.e_o_s=1;
    } else nb_samples=-1;

    op.packet=(unsigned char *)packet;
    op.bytes=nbBytes;
    op.b_o_s=0;
    op.granulepos=enc_granulepos;
    if (op.e_o_s)
    {
      /*We compute the final GP as ceil(len*48k/input_rate). When a resampling
        decoder does the matching floor(len*input/48k) conversion the length will
        be exactly the same as the input.*/
      op.granulepos=((original_samples*48000+rate-1)/rate)+header.preskip;
    }
    op.packetno=2+id;
    ogg_stream_packetin(&os, &op);
    last_segments+=size_segments;

    /*If the stream is over or we're sure that the delayed flush will fire,
      go ahead and flush now to avoid adding delay.*/
    while((op.e_o_s||(enc_granulepos+(frame_size*48000/coding_rate)-last_granulepos>max_ogg_delay)||
           (last_segments>=255))?
#ifdef OLD_LIBOGG
    /*Libogg > 1.2.2 allows us to achieve lower overhead by
      producing larger pages. For 20ms frames this is only relevant
      above ~32kbit/sec.*/
           ogg_stream_flush(&os, &og):
           ogg_stream_pageout(&os, &og))
    {
#else
           ogg_stream_flush_fill(&os, &og,255*255):
           ogg_stream_pageout_fill(&os, &og,255*255))
    {
#endif
      if (ogg_page_packets(&og)!=0)last_granulepos=ogg_page_granulepos(&og);
      last_segments-=og.header[26];
      ret=oe_write_page(&og, outputVec);
      if (ret!=og.header_len+og.body_len)
      {
         errMessage.assign("Error: failed writing data to output stream");
     return(false);
      }
      bytes_written+=ret;
      pages_out++;
    }
  }
  stop_time = time(NULL);

  opus_multistream_encoder_destroy(st);
  ogg_stream_clear(&os);
  free(packet);
  free(input);

  if (rate!=coding_rate)clear_resample(&inopt);
  clear_padder(&inopt);
  if (downmix)clear_downmix(&inopt);
  in_format->close_func(inopt.readdata);

  return true;
}

#define readint(buf, base) (((buf[base+3]<<24)&0xff000000)| \
                           ((buf[base+2]<<16)&0xff0000)| \
                           ((buf[base+1]<<8)&0xff00)| \
                           (buf[base]&0xff))
#define writeint(buf, base, val) do{ buf[base+3]=((val)>>24)&0xff; \
                                     buf[base+2]=((val)>>16)&0xff; \
                                     buf[base+1]=((val)>>8)&0xff; \
                                     buf[base]=(val)&0xff; \
                                 }while(0)

static void comment_init(char **comments, int* length, const char *vendor_string)
{
  /*The 'vendor' field should be the actual encoding library used.*/
  int vendor_length=strlen(vendor_string);
  int user_comment_list_length=0;
  int len=8+4+vendor_length+4;
  char *p=(char*)malloc(len);
  if (p==NULL)
  {
    //fprintf(stderr, "malloc failed in comment_init()\n");
    return;
  }
  memcpy(p, "OpusTags", 8);
  writeint(p, 8, vendor_length);
  memcpy(p+12, vendor_string, vendor_length);
  writeint(p, 12+vendor_length, user_comment_list_length);
  *length=len;
  *comments=p;
}

static void comment_add(char **comments, int* length, char *tag, char *val)
{
  char* p=*comments;
  int vendor_length=readint(p, 8);
  int user_comment_list_length=readint(p, 8+4+vendor_length);
  int tag_len=(tag?strlen(tag):0);
  int val_len=strlen(val);
  int len=(*length)+4+tag_len+val_len;

  p=(char*)realloc(p, len);
  if (p==NULL)
  {
    //fprintf(stderr, "realloc failed in comment_add()\n");
    return;
  }

  writeint(p, *length, tag_len+val_len);      /* length of comment */
  if (tag) memcpy(p+*length+4, tag, tag_len);  /* comment */
  memcpy(p+*length+4+tag_len, val, val_len);  /* comment */
  writeint(p, 8+4+vendor_length, user_comment_list_length+1);
  *comments=p;
  *length=len;
}
#undef readint
#undef writeint
