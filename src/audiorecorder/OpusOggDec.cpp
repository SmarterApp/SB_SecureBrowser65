#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "speex_resampler.h"

#include <stdio.h>
#if !defined WIN32 && !defined _WIN32
#include <unistd.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
/*#ifndef HAVE_GETOPT_LONG
#include "getopt_win.h"
#endif*/
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <ctype.h> /*tolower()*/

#include "OpusOgg.h"
#include "boost/format.hpp"

#include <opus.h>
#include <opus_multistream.h>
#include <ogg/ogg.h>

#if defined WIN32 || defined _WIN32 || defined WIN64 || defined _WIN64
# include "unicode_support.h"
/* We need the following two to set stdout to binary */
# include <io.h>
# include <fcntl.h>
# define I64FORMAT "I64d"
#else
# define I64FORMAT "lld"
# define fopen_utf8(_x,_y) fopen((_x),(_y))
# define argc_utf8 argc
# define argv_utf8 argv
#endif

#include <math.h>

#ifdef HAVE_LRINTF
# define float2int(x) lrintf(x)
#else
# define float2int(flt) ((int)(floor(.5+flt)))
#endif

#if defined HAVE_LIBSNDIO
# include <sndio.h>
#elif defined HAVE_SYS_SOUNDCARD_H || defined HAVE_MACHINE_SOUNDCARD_H || HAVE_SOUNDCARD_H
# ifdef HAVE_SYS_SOUNDCARD_H
#  include <sys/soundcard.h>
# elif HAVE_MACHINE_SOUNDCARD_H
#  include <machine/soundcard.h>
# else
#  include <soundcard.h>
# endif
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <sys/ioctl.h>
#elif defined HAVE_SYS_AUDIOIO_H
# include <sys/types.h>
# include <fcntl.h>
# include <sys/ioctl.h>
# include <sys/audioio.h>
# ifndef AUDIO_ENCODING_SLINEAR
#  define AUDIO_ENCODING_SLINEAR AUDIO_ENCODING_LINEAR /* Solaris */
# endif
#endif

#include <stdio.h>
#include <opus_types.h>

#if !defined(__LITTLE_ENDIAN__) && ( defined(WORDS_BIGENDIAN) || defined(__BIG_ENDIAN__) )
#define le_short(s) ((short) ((unsigned short) (s) << 8) | ((unsigned short) (s) >> 8))
#define be_short(s) ((short) (s))
#else
#define le_short(s) ((short) (s))
#define be_short(s) ((short) ((unsigned short) (s) << 8) | ((unsigned short) (s) >> 8))
#endif 

#ifdef XP_MACOSX
#undef OPUS_SET_LSB_DEPTH
#endif

/** Convert little endian */
static inline opus_int32 le_int(opus_int32 i)
{
#if !defined(__LITTLE_ENDIAN__) && ( defined(WORDS_BIGENDIAN) || defined(__BIG_ENDIAN__) )
   opus_uint32 ui, ret;
   ui = i;
   ret =  ui>>24;
   ret |= (ui>>8)&0x0000ff00;
   ret |= (ui<<8)&0x00ff0000;
   ret |= (ui<<24);
   return ret;
#else
   return i;
#endif
}

#include <string.h>
#include "opus_header.h"
#include "diag_range.h"
#include "stack_alloc.h"

#define MINI(_a,_b)      ((_a)<(_b)?(_a):(_b))
#define MAXI(_a,_b)      ((_a)>(_b)?(_a):(_b))
#define CLAMPI(_a,_b,_c) (MAXI(_a,MINI(_b,_c)))

/* 120ms at 48000 */
#define MAX_FRAME_SIZE (960*6)

#define readint(buf, base) (((buf[base+3]<<24)&0xff000000)| \
                           ((buf[base+2]<<16)&0xff0000)| \
                           ((buf[base+1]<<8)&0xff00)| \
                           (buf[base]&0xff))

#ifdef HAVE_LIBSNDIO
struct sio_hdl *hdl;
#endif

typedef struct shapestate shapestate;
struct shapestate {
  float * b_buf;
  float * a_buf;
  int fs;
  int mute;
};

static unsigned int rngseed = 22222;
static inline unsigned int fast_rand(void) {
  rngseed = (rngseed * 96314165) + 907633515;
  return rngseed;
}

#ifndef HAVE_FMINF
# define fminf(_x,_y) ((_x)<(_y)?(_x):(_y))
#endif

#ifndef HAVE_FMAXF
# define fmaxf(_x,_y) ((_x)>(_y)?(_x):(_y))
#endif

/******** PETE: Compiler says function not used
static void quit(int _x) {
#ifdef WIN_UNICODE
  uninit_console_utf8();
#endif
  exit(_x);
}
********/

/* This implements a 16 bit quantization with full triangular dither
   and IIR noise shaping. The noise shaping filters were designed by
   Sebastian Gesemann based on the LAME ATH curves with flattening
   to limit their peak gain to 20 dB.
   (Everyone elses' noise shaping filters are mildly crazy)
   The 48kHz version of this filter is just a warped version of the
   44.1kHz filter and probably could be improved by shifting the
   HF shelf up in frequency a little bit since 48k has a bit more
   room and being more conservative against bat-ears is probably
   more important than more noise suppression.
   This process can increase the peak level of the signal (in theory
   by the peak error of 1.5 +20 dB though this much is unobservable rare)
   so to avoid clipping the signal is attenuated by a couple thousandths
   of a dB. Initially the approach taken here was to only attenuate by
   the 99.9th percentile, making clipping rare but not impossible (like
   SoX) but the limited gain of the filter means that the worst case was
   only two thousandths of a dB more, so this just uses the worst case.
   The attenuation is probably also helpful to prevent clipping in the DAC
   reconstruction filters or downstream resampling in any case.*/
static inline void shape_dither_toshort(shapestate *_ss, short *_o, float *_i, int _n, int _CC)
{
  const float gains[3]={32768.f-15.f,32768.f-15.f,32768.f-3.f};
  const float fcoef[3][8] =
  {
    {2.2374f, -.7339f, -.1251f, -.6033f, 0.9030f, .0116f, -.5853f, -.2571f}, /* 48.0kHz noise shaping filter sd=2.34*/
    {2.2061f, -.4706f, -.2534f, -.6214f, 1.0587f, .0676f, -.6054f, -.2738f}, /* 44.1kHz noise shaping filter sd=2.51*/
    {1.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f,0.0000f, 0.0000f, 0.0000f}, /* lowpass noise shaping filter sd=0.65*/
  };
  int i;
  int rate=_ss->fs==44100?1:(_ss->fs==48000?0:2);
  float gain=gains[rate];
  float *b_buf;
  float *a_buf;
  int mute=_ss->mute;
  b_buf=_ss->b_buf;
  a_buf=_ss->a_buf;
  /*In order to avoid replacing digital silence with quiet dither noise
    we mute if the output has been silent for a while*/
  if (mute>64)
    memset(a_buf,0,sizeof(float)*_CC*4);
  for(i=0;i<_n;i++)
  {
    int c;
    int pos = i*_CC;
    int silent=1;
    for(c=0;c<_CC;c++)
    {
      int j, si;
      float r,s,err=0;
      silent&=_i[pos+c]==0;
      s=_i[pos+c]*gain;
      for(j=0;j<4;j++)
        err += fcoef[rate][j]*b_buf[c*4+j] - fcoef[rate][j+4]*a_buf[c*4+j];
      memmove(&a_buf[c*4+1],&a_buf[c*4],sizeof(float)*3);
      memmove(&b_buf[c*4+1],&b_buf[c*4],sizeof(float)*3);
      a_buf[c*4]=err;
      s = s - err;
      r=(float)fast_rand()*(1/(float)UINT_MAX) - (float)fast_rand()*(1/(float)UINT_MAX);
      if (mute>16)r=0;
      /*Clamp in float out of paranoia that the input will be >96 dBFS and wrap if the
        integer is clamped.*/
      _o[pos+c] = si = float2int(fmaxf(-32768,fminf(s + r,32767)));
      /*Including clipping in the noise shaping is generally disastrous:
        the futile effort to restore the clipped energy results in more clipping.
        However, small amounts-- at the level which could normally be created by
        dither and rounding-- are harmless and can even reduce clipping somewhat
        due to the clipping sometimes reducing the dither+rounding error.*/
      b_buf[c*4] = (mute>16)?0:fmaxf(-1.5f,fminf(si - s,1.5f));
    }
    mute++;
    if (!silent)mute=0;
  }
  _ss->mute=MINI(mute,960);
}

void version(void)
{
   //printf("opusdec %s %s (using %s)\n",PACKAGE,VERSION,opus_get_version_string());
   //printf("Copyright (C) 2008-2012 Xiph.Org Foundation\n");
}

void version_short(void)
{
   //printf("opusdec %s %s (using %s)\n",PACKAGE,VERSION,opus_get_version_string());
   //printf("Copyright (C) 2008-2012 Xiph.Org Foundation\n");
}

/*Process an Opus header and setup the opus decoder based on it.
  It takes several pointers for header values which are needed
  elsewhere in the code.*/
static OpusMSDecoder *process_header(ogg_packet *op, opus_int32 *rate,
       int *mapping_family, int *channels, int *preskip, float *gain,
       float manual_gain, int *streams, int wav_format, int quiet)
{
   int err;
   OpusMSDecoder *st;
   OpusHeader header;

   if (opus_header_parse(op->packet, op->bytes, &header)==0)
   {
      //fprintf(stderr, "Cannot parse header\n");
      return NULL;
   }

   *mapping_family = header.channel_mapping;
   *channels = header.channels;

   if (!*rate)*rate=header.input_sample_rate;
   /*If the rate is unspecified we decode to 48000*/
   if (*rate==0)*rate=48000;
   if (*rate<8000||*rate>192000){
     //fprintf(stderr,"Warning: Crazy input_rate %d, decoding to 48000 instead.\n",*rate);
     *rate=48000;
   }

   *preskip = header.preskip;
   st = opus_multistream_decoder_create(48000, header.channels, header.nb_streams, header.nb_coupled, header.stream_map, &err);
   if (err != OPUS_OK){
     //fprintf(stderr, "Cannot create encoder: %s\n", opus_strerror(err));
     return NULL;
   }
   if (!st)
   {
      //fprintf (stderr, "Decoder initialization failed: %s\n", opus_strerror(err));
      return NULL;
   }

   *streams=header.nb_streams;

   if (header.gain!=0 || manual_gain!=0)
   {
      /*Gain API added in a newer libopus version, if we don't have it
        we apply the gain ourselves. We also add in a user provided
        manual gain at the same time.*/
      int gainadj = (int)(manual_gain*256.)+header.gain;
#ifdef OPUS_SET_GAIN
      err=opus_multistream_decoder_ctl(st,OPUS_SET_GAIN(gainadj));
      if (err==OPUS_UNIMPLEMENTED)
      {
#endif
         *gain = pow(10., gainadj/5120.);
#ifdef OPUS_SET_GAIN
      } else if (err!=OPUS_OK)
      {
         //fprintf (stderr, "Error setting gain: %s\n", opus_strerror(err));
         return NULL;
      }
#endif
   }

   return st;
}

opus_int64 audio_write(float *pcm, int channels, int frame_size, std::vector<unsigned char>& outputVec, SpeexResamplerState *resampler,
                       int *skip, shapestate *shapemem, int file, opus_int64 maxout)
{
   opus_int64 sampout=0;
   int i, tmp_skip;
   unsigned out_len;
   short *out;
   float *buf;
   float *output;
   out=(short*)alloca(sizeof(short)*MAX_FRAME_SIZE*channels);
   buf=(float*)alloca(sizeof(float)*MAX_FRAME_SIZE*channels);
   maxout=maxout<0?0:maxout;
   do {
     if (skip){
       tmp_skip = (*skip>frame_size) ? (int)frame_size : *skip;
       *skip -= tmp_skip;
     } else {
       tmp_skip = 0;
     }
     if (resampler){
       unsigned in_len;
       output=buf;
       in_len = frame_size-tmp_skip;
       out_len = 1024<maxout?1024:maxout;
       speex_resampler_process_interleaved_float(resampler, pcm+channels*tmp_skip, &in_len, buf, &out_len);
       pcm += channels*(in_len+tmp_skip);
       frame_size -= in_len+tmp_skip;
     } else {
       output=pcm+channels*tmp_skip;
       out_len=frame_size-tmp_skip;
       frame_size=0;
     }

     /*Convert to short and save to output file*/
     if (shapemem){
       shape_dither_toshort(shapemem,out,output,out_len,channels);
     }else{
       for (i=0;i<(int)out_len*channels;i++)
         out[i]=(short)float2int(fmaxf(-32768,fminf(output[i]*32768.f,32767)));
     }
     if ((le_short(1)!=1)&&file){
       for (i=0;i<(int)out_len*channels;i++)
         out[i]=le_short(out[i]);
     }

     if (maxout>0)
     {
       //ret=fwrite(out, 2*channels, out_len<maxout?out_len:maxout, fout);
     long numWrite = out_len < maxout ? out_len:maxout;
     long cntr;
     unsigned char *ptr = (unsigned char *)out;
     for (cntr=0; cntr < numWrite; cntr++)
     {
       outputVec.push_back(*ptr);
       ptr++;
       outputVec.push_back(*ptr);
       ptr++;
     }
       sampout+=numWrite;
       maxout-=numWrite;
     }
   } while (frame_size>0 && maxout>0);
   return sampout;
}


bool OpusOgg::decode(std::vector<unsigned char>& inputVec, std::vector<unsigned char>& outputVec, std::string&  errMessage, int &sampleRate, int &channels)
{
   float *output;
   int frame_size=0;
   OpusMSDecoder *st=NULL;
   opus_int64 packet_count=0;
   int total_links=0;
   int stream_init = 0;
   int quiet = 0;
   ogg_int64_t page_granule=0;
   ogg_int64_t link_out=0;
   ogg_sync_state oy;
   ogg_page       og;
   ogg_packet     op;
   ogg_stream_state os;
   int eos=0;
   ogg_int64_t audio_size=0;
   float loss_percent=-1;
   float manual_gain=0;
   channels=-1;
   int mapping_family;
   sampleRate=0;
   int wav_format=0;
   int preskip=0;
   int gran_offset=0;
   int has_opus_stream=0;
   ogg_int32_t opus_serialno=0;
   int dither=1;
   shapestate shapemem;
   SpeexResamplerState *resampler=NULL;
   float gain=1;
   int streams=0;

   output=0;
   shapemem.a_buf=0;
   shapemem.b_buf=0;
   shapemem.mute=960;
   shapemem.fs=0;

   //rate=sampling_rate;
   wav_format=0;



   /* .opus files use the Ogg container to provide framing and timekeeping.
    * http://tools.ietf.org/html/draft-terriberry-oggopus
    * The easiest way to decode the Ogg container is to use libogg, so
    *  thats what we do here.
    * Using libogg is fairly straight forward-- you take your stream of bytes
    *  and feed them to ogg_sync_ and it periodically returns Ogg pages, you
    *  check if the pages belong to the stream you're decoding then you give
    *  them to libogg and it gives you packets. You decode the packets. The
    *  pages also provide timing information.*/
   ogg_sync_init(&oy);

   long inPos = 0;
   /*Main decoding loop*/
   while (1)
   {
      char *data;
      int i;
      /*Get the ogg buffer for writing*/
      data = ogg_sync_buffer(&oy, 200);
      /*Read bitstream from input*/
    // int numRead;
    int numRead;
    for (numRead=0; numRead < 200 && inPos < (int)inputVec.size(); numRead++)
    {
      *data = inputVec[inPos];
      data++;
      inPos++;
    }
      //nb_read = fread(data, sizeof(char), 200, fin);
      ogg_sync_wrote(&oy, numRead);

      /*Loop for all complete pages we got (most likely only one)*/
      while (ogg_sync_pageout(&oy, &og)==1)
      {
         if (stream_init == 0) {
            ogg_stream_init(&os, ogg_page_serialno(&og));
            stream_init = 1;
         }
         if (ogg_page_serialno(&og) != os.serialno) {
            /* so all streams are read. */
            ogg_stream_reset_serialno(&os, ogg_page_serialno(&og));
         }
         /*Add page to the bitstream*/
         ogg_stream_pagein(&os, &og);
         page_granule = ogg_page_granulepos(&og);
         /*Extract all available packets*/
         while (ogg_stream_packetout(&os, &op) == 1)
         {
            /*OggOpus streams are identified by a magic string in the initial
              stream header.*/
            if (op.b_o_s && op.bytes>=8 && !memcmp(op.packet, "OpusHead", 8)) 
            {
               if (!has_opus_stream)
               {
                 opus_serialno = os.serialno;
                 has_opus_stream = 1;
                 link_out = 0;
                 packet_count = 0;
                 eos = 0;
                 total_links++;
               } 
            }
            if (!has_opus_stream || os.serialno != opus_serialno)
               break;
            /*If first packet in a logical stream, process the Opus header*/
            if (packet_count==0)
            {
               st = process_header(&op, &sampleRate, &mapping_family, &channels, &preskip, &gain, manual_gain, &streams, wav_format, quiet);
               if (!st) return(false);

               if (ogg_stream_packetout(&os, &op)!=0 || og.header[og.header_len-1]==255)
               {
                  /*The format specifies that the initial header and tags packets are on their
                    own pages. To aid implementors in discovering that their files are wrong
                    we reject them explicitly here. In some player designs files like this would
                    fail even without an explicit test.*/
                   errMessage.assign("Extra packets on initial header page. Invalid stream.");
                  return false;
               }

               /*Remember how many samples at the front we were told to skip
                 so that we can adjust the timestamp counting.*/
               gran_offset=preskip;

               /*Setup the memory for the dithered output*/
               if (!shapemem.a_buf)
               {
                  shapemem.a_buf=(float*)calloc(channels,sizeof(float)*4);
                  shapemem.b_buf=(float*)calloc(channels,sizeof(float)*4);
                  shapemem.fs=sampleRate;
               }

               if (!output)output=(float*)malloc(sizeof(float)*MAX_FRAME_SIZE*channels);

               /*Normal players should just play at 48000 or their maximum rate,
                 as described in the OggOpus spec.  But for commandline tools
                 like opusdec it can be desirable to exactly preserve the original
                 sampling rate and duration, so we have a resampler here.*/
               if (sampleRate != 48000)
               {
                  int err;
                  resampler = speex_resampler_init(channels, 48000, sampleRate, 5, &err);
                  if (err!=0)
                  {
                    errMessage.assign("resampler error: ");
                    errMessage.append(speex_resampler_strerror(err));
                  }
                  speex_resampler_skip_zeros(resampler);
               }
             } 
               else if (packet_count==1)
             {
               if (ogg_stream_packetout(&os, &op)!=0 || og.header[og.header_len-1]==255)
               {
                 errMessage.assign("Extra packets on initial tags page. Invalid stream.");
                 return false;
               }
            } 
              else 
            {
               int ret;
               opus_int64 maxout;
               opus_int64 outsamp;
               int lost=0;
               if (loss_percent>0 && 100*((float)rand())/RAND_MAX<loss_percent)
                  lost=1;

               /*End of stream condition*/
               if (op.e_o_s && os.serialno == opus_serialno)eos=1; /* don't care for anything except opus eos */

               /*Are we simulating loss for this packet?*/
               if (!lost)
               {
                  /*Decode Opus packet*/
                  ret = opus_multistream_decode_float(st, (unsigned char*)op.packet, op.bytes, output, MAX_FRAME_SIZE, 0);
               } else {
                  /*Extract the original duration.
                    Normally you wouldn't have it for a lost packet, but normally the
                    transports used on lossy channels will effectively tell you.
                    This avoids opusdec squaking when the decoded samples and
                    granpos mismatches.*/
                  opus_int32 lost_size;
                  lost_size = MAX_FRAME_SIZE;
                  if (op.bytes>0)
                  {
                    opus_int32 spp;
                    spp=opus_packet_get_nb_frames(op.packet,op.bytes);
                    if (spp>0)
                    {
                      spp*=opus_packet_get_samples_per_frame(op.packet,48000/*decoding_rate*/);
                      if (spp>0)lost_size=spp;
                    }
                  }
                  /*Invoke packet loss concealment.*/
                  ret = opus_multistream_decode_float(st, NULL, 0, output, lost_size, 0);
               }

               /*If the decoder returned less than zero, we have an error.*/
               if (ret<0)
               {
                 errMessage.assign("Decoding error: ");
                 errMessage.append(opus_strerror(ret));
                 break;
               }
               frame_size = ret;

               /*Apply header gain, if we're not using an opus library new
                 enough to do this internally.*/
               if (gain!=0)
               {
                 for (i=0;i<frame_size*channels;i++)
                    output[i] *= gain;
               }

               /*This handles making sure that our output duration respects
                 the final end-trim by not letting the output sample count
                 get ahead of the granpos indicated value.*/
               maxout=((page_granule-gran_offset)*sampleRate/48000)-link_out;
               outsamp=audio_write(output, channels, frame_size, outputVec, resampler, &preskip, dither?&shapemem:0, 1, 0>maxout?0:maxout);
               link_out+=outsamp;
               audio_size+=sizeof(short)*outsamp*channels;
            }
            packet_count++;
         }
         /*We're done, drain the resampler if we were using it.*/
         if (eos && resampler)
         {
            float *zeros;
            int drain;

            zeros=(float *)calloc(100*channels,sizeof(float));
            drain = speex_resampler_get_input_latency(resampler);
            do {
               opus_int64 outsamp;
               int tmp = drain;
               if (tmp > 100)
                  tmp = 100;
               outsamp=audio_write(zeros, channels, tmp, outputVec, resampler, NULL, &shapemem, 1, ((page_granule-gran_offset)*sampleRate/48000)-link_out);
               link_out+=outsamp;
               audio_size+=sizeof(short)*outsamp*channels;
               drain -= tmp;
            } while (drain>0);
            free(zeros);
            speex_resampler_destroy(resampler);
            resampler=NULL;
         }
         if (eos)
         {
            has_opus_stream=0;
            if (st)opus_multistream_decoder_destroy(st);
            st=NULL;
         }
      }

      if (inPos >= (int)inputVec.size())
      {
         break;
      }
   }

   /*Did we make it to the end without recovering ANY opus logical streams?*/
   // if (!total_links)
      errMessage.assign("This doesn't look like a Opus file");

   if (stream_init) ogg_stream_clear(&os);

   ogg_sync_clear(&oy);

   if (shapemem.a_buf)free(shapemem.a_buf);
   if (shapemem.b_buf)free(shapemem.b_buf);

   if (output)free(output);

   return true;
}
