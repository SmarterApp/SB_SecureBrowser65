/* Copyright 2000-2002, Michael Smith <msmith@xiph.org>
             2010, Monty <monty@xiph.org>
   AIFF/AIFC support from OggSquish, (c) 1994-1996 Monty <xiphmont@xiph.org>
   (From GPL code in oggenc relicensed by permission from Monty and Msmith)
   File: audio-in.c

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if !defined(_LARGEFILE_SOURCE)
# define _LARGEFILE_SOURCE
#endif
#if !defined(_LARGEFILE64_SOURCE)
# define _LARGEFILE64_SOURCE
#endif
#if !defined(_FILE_OFFSET_BITS)
# define _FILE_OFFSET_BITS 64
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <math.h>
#include <vector>

#include "stack_alloc.h"

#ifdef WIN32
#include <windows.h> /*GetFileType()*/
#include <io.h>      /*_get_osfhandle()*/
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(X) gettext(X)
#else
#define _(X) (X)
#define textdomain(X)
#define bindtextdomain(X, Y)
#endif
#ifdef gettext_noop
#define N_(X) gettext_noop(X)
#else
#define N_(X) (X)
#endif

#include <ogg/ogg.h>
#include "opusenc.h"
#include "speex_resampler.h"
#include "lpc.h"
#include "opus_header.h"

#ifdef HAVE_LIBFLAC
#include "flac.h"
#endif


/* Macros to read header data */
#define READ_U32_LE(buf) \
    (((buf)[3]<<24)|((buf)[2]<<16)|((buf)[1]<<8)|((buf)[0]&0xff))

#define READ_U16_LE(buf) \
    (((buf)[1]<<8)|((buf)[0]&0xff))

#define READ_U32_BE(buf) \
    (((buf)[0]<<24)|((buf)[1]<<16)|((buf)[2]<<8)|((buf)[3]&0xff))

#define READ_U16_BE(buf) \
    (((buf)[0]<<8)|((buf)[1]&0xff))


int wav_id(unsigned char *buf, int len)
{
    if(len<12) return 0; /* Something screwed up */

    if(memcmp(buf, "RIFF", 4))
        return 0; /* Not wave */

    /*flen = READ_U32_LE(buf+4);*/ /* We don't use this */

    if(memcmp(buf+8, "WAVE",4))
        return 0; /* RIFF, but not wave */

    return 1;
}


long wav_read(void *in, float *buffer, int samples)
{
    wavfile *f = (wavfile *)in;
    int sampbyte = f->samplesize / 8;
    signed char *buf = (signed char *)alloca(samples*sampbyte*f->channels);
    //long bytes_read = fread(buf, 1, samples*sampbyte*f->channels, f->f);
	long bytes_read;
	signed char *ptr;
	ptr = buf;
	for (bytes_read=0; bytes_read < samples*sampbyte*f->channels && f->inputpos < f->input.size(); bytes_read++)
	{
		*ptr = f->input[f->inputpos];
		ptr++;
		f->inputpos++;
	}

    int i,j;
    opus_int64 realsamples;
    int *ch_permute = f->channel_permute;

    if(f->totalsamples && f->samplesread +
            bytes_read/(sampbyte*f->channels) > f->totalsamples) {
        bytes_read = sampbyte*f->channels*(f->totalsamples - f->samplesread);
    }

    realsamples = bytes_read/(sampbyte*f->channels);
    f->samplesread += realsamples;

    if(f->samplesize==8)
    {
        unsigned char *bufu = (unsigned char *)buf;
        for(i = 0; i < realsamples; i++)
        {
            for(j=0; j < f->channels; j++)
            {
                buffer[i*f->channels+j]=((int)(bufu[i*f->channels + ch_permute[j]])-128)/128.0f;
            }
        }
    }
    else if(f->samplesize==16)
    {
        if(!f->bigendian)
        {
            for(i = 0; i < realsamples; i++)
            {
                for(j=0; j < f->channels; j++)
                {
                    buffer[i*f->channels+j] = ((buf[i*2*f->channels + 2*ch_permute[j] + 1]<<8) |
                                    (buf[i*2*f->channels + 2*ch_permute[j]] & 0xff))/32768.0f;
                }
            }
        }
        else
        {
            for(i = 0; i < realsamples; i++)
            {
                for(j=0; j < f->channels; j++)
                {
                    buffer[i*f->channels+j]=((buf[i*2*f->channels + 2*ch_permute[j]]<<8) |
                                  (buf[i*2*f->channels + 2*ch_permute[j] + 1] & 0xff))/32768.0f;
                }
            }
        }
    }
    else if(f->samplesize==24)
    {
        if(!f->bigendian) {
            for(i = 0; i < realsamples; i++)
            {
                for(j=0; j < f->channels; j++)
                {
                    buffer[i*f->channels+j] = ((buf[i*3*f->channels + 3*ch_permute[j] + 2] << 16) |
                      (((unsigned char *)buf)[i*3*f->channels + 3*ch_permute[j] + 1] << 8) |
                      (((unsigned char *)buf)[i*3*f->channels + 3*ch_permute[j]] & 0xff))
                        / 8388608.0f;

                }
            }
        }
        else {
            //fprintf(stderr, _("Big endian 24 bit PCM data is not currently "
            //                  "supported, aborting.\n"));
            return 0;
        }
    }
    else {
        //fprintf(stderr, _("Internal error: attempt to read unsupported "
        //                  "bitdepth %d\n"), f->samplesize);
        return 0;
    }

    return realsamples;
}

void wav_close(void *info)
{
    wavfile *f = (wavfile *)info;
    free(f->channel_permute);

    free(f);
}

int raw_open(std::vector<unsigned char>& inp, oe_enc_opt *opt, unsigned char *buf, int buflen)
{
    wav_fmt format; /* fake wave header ;) */
	int sz = sizeof(wavfile);
    wavfile *wav = new wavfile();// (wavfile *)malloc(sz);
    int i;
    (void)buf;/*unused*/
    (void)buflen;/*unused*/

    /* construct fake wav header ;) */
    format.format =      2;
    format.channels =    opt->channels;
    format.samplerate =  opt->rate;
    format.samplesize =  opt->samplesize;
    format.bytespersec = opt->channels * opt->rate * opt->samplesize / 8;
    format.align =       format.bytespersec;
    wav->input =         inp;
	wav->inputpos =	     0;
    wav->samplesread =   0;
    wav->bigendian =     opt->endianness;
    wav->channels =      format.channels;
    wav->samplesize =    opt->samplesize;
    wav->totalsamples =  0;
    wav->channel_permute = (int*)malloc(wav->channels * sizeof(int));
    for (i=0; i < wav->channels; i++)
      wav->channel_permute[i] = i;

    opt->read_samples = wav_read;
    opt->readdata = (void *)wav;
    opt->total_samples_per_channel = 0; /* raw mode, don't bother */
    return 1;
}

typedef struct {
    audio_read_func real_reader;
    void *real_readdata;
    int channels;
    float scale_factor;
} scaler;

typedef struct {
    audio_read_func real_reader;
    void *real_readdata;
    ogg_int64_t *original_samples;
    int channels;
    int lpc_ptr;
    int *extra_samples;
    float *lpc_out;
} padder;

/* Read audio data, appending padding to make up any gap
 * between the available and requested number of samples
 * with LPC-predicted data to minimize the pertubation of
 * the valid data that falls in the same frame.
 */
static long read_padder(void *data, float *buffer, int samples) {
    padder *d = (padder*)data;
    long in_samples = d->real_reader(d->real_readdata, buffer, samples);
    int i, extra=0;
    const int lpc_order=32;

    if(d->original_samples)*d->original_samples+=in_samples;

    if(in_samples<samples){
      if(d->lpc_ptr<0){
        d->lpc_out=(float*)calloc(d->channels * *d->extra_samples, sizeof(*d->lpc_out));
        if(in_samples>lpc_order*2){
          float *lpc=(float*)alloca(lpc_order*sizeof(*lpc));
          for(i=0;i<d->channels;i++){
            vorbis_lpc_from_data(buffer+i,lpc,in_samples,lpc_order,d->channels);
            vorbis_lpc_predict(lpc,buffer+i+(in_samples-lpc_order)*d->channels,
                               lpc_order,d->lpc_out+i,*d->extra_samples,d->channels);
          }
        }
        d->lpc_ptr=0;
      }
      extra=samples-in_samples;
      if(extra>*d->extra_samples)extra=*d->extra_samples;
      *d->extra_samples-=extra;
    }
    memcpy(buffer+in_samples*d->channels,d->lpc_out+d->lpc_ptr*d->channels,extra*d->channels*sizeof(*buffer));
    d->lpc_ptr+=extra;
    return in_samples+extra;
}

void setup_padder(oe_enc_opt *opt,ogg_int64_t *original_samples) {
    padder *d = (padder*)calloc(1, sizeof(padder));

    d->real_reader = opt->read_samples;
    d->real_readdata = opt->readdata;

    opt->read_samples = read_padder;
    opt->readdata = d;
    d->channels = opt->channels;
    d->extra_samples = &opt->extraout;
    d->original_samples=original_samples;
    d->lpc_ptr = -1;
    d->lpc_out = NULL;
}

void clear_padder(oe_enc_opt *opt) {
    padder *d = (padder*)opt->readdata;

    opt->read_samples = d->real_reader;
    opt->readdata = d->real_readdata;

    if(d->lpc_out)free(d->lpc_out);
    free(d);
}

typedef struct {
    SpeexResamplerState *resampler;
    audio_read_func real_reader;
    void *real_readdata;
    float *bufs;
    int channels;
    int bufpos;
    int bufsize;
    int done;
} resampler;

static long read_resampled(void *d, float *buffer, int samples)
{
    resampler *rs = (resampler*)d;
    int out_samples=0;
    float *pcmbuf;
    int *inbuf;
    pcmbuf=rs->bufs;
    inbuf=&rs->bufpos;
    while(out_samples<samples){
      int i;
      int reading, ret;
      unsigned in_len, out_len;
      out_len=samples-out_samples;
      reading=rs->bufsize-*inbuf;
      if(reading>1024)reading=1024;
      ret=rs->real_reader(rs->real_readdata, pcmbuf+*inbuf*rs->channels, reading);
      *inbuf+=ret;
      in_len=*inbuf;
      speex_resampler_process_interleaved_float(rs->resampler, pcmbuf, &in_len, buffer+out_samples*rs->channels, &out_len);
      out_samples+=out_len;
      if(ret==0&&in_len==0){
        for(i=out_samples*rs->channels;i<samples*rs->channels;i++)buffer[i]=0;
        return out_samples;
      }
      for(i=0;i<rs->channels*(*inbuf-(long int)in_len);i++)pcmbuf[i]=pcmbuf[i+rs->channels*in_len];
      *inbuf-=in_len;
    }
    return out_samples;
}

int setup_resample(oe_enc_opt *opt, int complexity, long outfreq) {
    resampler *rs = (resampler*)calloc(1, sizeof(resampler));
    int err;

    rs->bufsize = 5760*2; /* Have at least two output frames worth, just in case of ugly ratios */
    rs->bufpos = 0;

    rs->real_reader = opt->read_samples;
    rs->real_readdata = opt->readdata;
    rs->channels = opt->channels;
    rs->done = 0;
    rs->resampler = speex_resampler_init(rs->channels, opt->rate, outfreq, complexity, &err);
    if(err!=0)fprintf(stderr, _("resampler error: %s\n"), speex_resampler_strerror(err));

    opt->skip+=speex_resampler_get_output_latency(rs->resampler);

    rs->bufs = (float*)malloc(sizeof(float) * rs->bufsize * opt->channels);

    opt->read_samples = read_resampled;
    opt->readdata = rs;
    if(opt->total_samples_per_channel)
        opt->total_samples_per_channel = (int)((float)opt->total_samples_per_channel *
            ((float)outfreq/(float)opt->rate));
    opt->rate = outfreq;

    return 0;
}

void clear_resample(oe_enc_opt *opt) {
    resampler *rs = (resampler*)opt->readdata;

    opt->read_samples = rs->real_reader;
    opt->readdata = rs->real_readdata;
    speex_resampler_destroy(rs->resampler);

    free(rs->bufs);

    free(rs);
}

typedef struct {
    audio_read_func real_reader;
    void *real_readdata;
    float *bufs;
    float *matrix;
    int in_channels;
    int out_channels;
} downmix;

static long read_downmix(void *data, float *buffer, int samples)
{
    downmix *d = (downmix*)data;
    long in_samples = d->real_reader(d->real_readdata, d->bufs, samples);
    int i,j,k,in_ch,out_ch;

    in_ch=d->in_channels;
    out_ch=d->out_channels;

    for(i=0;i<in_samples;i++){
      for(j=0;j<out_ch;j++){
        float *samp;
        samp=&buffer[i*out_ch+j];
        *samp=0;
        for(k=0;k<in_ch;k++){
          *samp+=d->bufs[i*in_ch+k]*d->matrix[in_ch*j+k];
        }
      }
    }
    return in_samples;
}

int setup_downmix(oe_enc_opt *opt, int out_channels) {
    static const float stupid_matrix[7][8][2]={
      /*2*/  {{1,0},{0,1}},
      /*3*/  {{1,0},{0.7071f,0.7071f},{0,1}},
      /*4*/  {{1,0},{0,1},{0.866f,0.5f},{0.5f,0.866f}},
      /*5*/  {{1,0},{0.7071f,0.7071f},{0,1},{0.866f,0.5f},{0.5f,0.866f}},
      /*6*/  {{1,0},{0.7071f,0.7071f},{0,1},{0.866f,0.5f},{0.5f,0.866f},{0.7071f,0.7071f}},
      /*7*/  {{1,0},{0.7071f,0.7071f},{0,1},{0.866f,0.5f},{0.5f,0.866f},{0.6123f,0.6123f},{0.7071f,0.7071f}},
      /*8*/  {{1,0},{0.7071f,0.7071f},{0,1},{0.866f,0.5f},{0.5f,0.866f},{0.866f,0.5f},{0.5f,0.866f},{0.7071f,0.7071f}},
    };
    float sum;
    downmix *d;
    int i,j;

    if(opt->channels<=out_channels || out_channels>2 || (out_channels==2&&opt->channels>8) || opt->channels<=0 || out_channels<=0) {
        fprintf(stderr, "Downmix must actually downmix and only knows mono/stereo out.\n");
        if(opt->channels>8)fprintf(stderr, "Downmix also only knows how to mix >8ch to mono.\n");
        return 0;
    }

    d = (downmix*)calloc(1, sizeof(downmix));
    d->bufs = (float*)malloc(sizeof(float)*opt->channels*4096);
    d->matrix = (float*)malloc(sizeof(float)*opt->channels*out_channels);
    d->real_reader = opt->read_samples;
    d->real_readdata = opt->readdata;
    d->in_channels=opt->channels;
    d->out_channels=out_channels;

    if(out_channels==1&&d->in_channels>8){
      for(i=0;i<d->in_channels;i++)d->matrix[i]=1.0f/d->in_channels;
    }else if(out_channels==2){
      for(j=0;j<d->out_channels;j++)
        for(i=0;i<d->in_channels;i++)d->matrix[d->in_channels*j+i]=
          stupid_matrix[opt->channels-2][i][j];
    }else{
      for(i=0;i<d->in_channels;i++)d->matrix[i]=
        (stupid_matrix[opt->channels-2][i][0])+
        (stupid_matrix[opt->channels-2][i][1]);
    }
    sum=0;
    for(i=0;i<d->in_channels*d->out_channels;i++)sum+=d->matrix[i];
    sum=(float)out_channels/sum;
    for(i=0;i<d->in_channels*d->out_channels;i++)d->matrix[i]*=sum;
    opt->read_samples = read_downmix;
    opt->readdata = d;

    opt->channels = out_channels;
    return out_channels;
}

void clear_downmix(oe_enc_opt *opt) {
    downmix *d = (downmix*)opt->readdata;

    opt->read_samples = d->real_reader;
    opt->readdata = d->real_readdata;
    opt->channels = d->in_channels; /* other things in cleanup rely on this */

    free(d->bufs);
    free(d->matrix);
    free(d);
}
