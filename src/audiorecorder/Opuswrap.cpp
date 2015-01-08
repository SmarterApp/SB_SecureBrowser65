/* Copyright (c) 2007-2008 CSIRO
   Copyright (c) 2007-2009 Xiph.Org Foundation
   Written by Jean-Marc Valin */
/*
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
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "Opuswrap.h"
#include "boost/format.hpp"

#define MAX_PACKET 1500

//void print_usage( char* argv[] )
//{
//    fprintf(stderr, "Usage: %s [-e] <application> <sampling rate (Hz)> <channels (1/2)> "
//        "<bits per second>  [options] <input> <output>\n", argv[0]);
//    fprintf(stderr, "       %s -d <sampling rate (Hz)> <channels (1/2)> "
//        "[options] <input> <output>\n\n", argv[0]);
//    fprintf(stderr, "mode: voip | audio | restricted-lowdelay\n" );
//    fprintf(stderr, "options:\n" );
//    fprintf(stderr, "-e                   : only runs the encoder (output the bit-stream)\n" );
//    fprintf(stderr, "-d                   : only runs the decoder (reads the bit-stream as input)\n" );
//    fprintf(stderr, "-cbr                 : enable constant bitrate; default: variable bitrate\n" );
//    fprintf(stderr, "-cvbr                : enable constrained variable bitrate; default: unconstrained\n" );
//    fprintf(stderr, "-bandwidth <NB|MB|WB|SWB|FB> : audio bandwidth (from narrowband to fullband); default: sampling rate\n" );
//    fprintf(stderr, "-framesize <2.5|5|10|20|40|60> : frame size in ms; default: 20 \n" );
//    fprintf(stderr, "-max_payload <bytes> : maximum payload size in bytes, default: 1024\n" );
//    fprintf(stderr, "-complexity <comp>   : complexity, 0 (lowest) ... 10 (highest); default: 10\n" );
//    fprintf(stderr, "-inbandfec           : enable SILK inband FEC\n" );
//    fprintf(stderr, "-forcemono           : force mono encoding, even for stereo input\n" );
//    fprintf(stderr, "-dtx                 : enable SILK DTX\n" );
//    fprintf(stderr, "-loss <perc>         : simulate packet loss, in percent (0-100); default: 0\n" );
//}

static void int_to_char(opus_uint32 i, unsigned char ch[4])
{
    ch[0] = i>>24;
    ch[1] = (i>>16)&0xFF;
    ch[2] = (i>>8)&0xFF;
    ch[3] = i&0xFF;
}

static opus_uint32 char_to_int(unsigned char ch[4])
{
    return ((opus_uint32)ch[0]<<24) | ((opus_uint32)ch[1]<<16)
         | ((opus_uint32)ch[2]<< 8) |  (opus_uint32)ch[3];
}

bool Opuswrap::code(bool encode, bool decode, opus_int32 sampling_rate, int channels, opus_int32 bitrate_bps, std::vector<unsigned char>& input, std::vector<unsigned char>& output, std::string& errMessage)
{
    int err;
    OpusEncoder *enc=NULL;
    OpusDecoder *dec=NULL;
    // int args;
    int len[2];
    int frame_size;
    unsigned char *data[2];
    unsigned char *fbytes;
    int use_vbr;
    int max_payload_bytes;
    int complexity;
    int use_inbandfec;
    int use_dtx;
    int forcechannels;
    int cvbr = 0;
    int packet_loss_perc;
    opus_int32 count=0, count_act=0;
    int k;
    opus_int32 skip=0;
    int stop=0;
    short *in, *out;
    int application=OPUS_APPLICATION_AUDIO;
    double bits=0.0, bits_max=0.0, bits_act=0.0, bits2=0.0, nrg;
    int bandwidth=-1;
    const char *bandwidth_string;
    int lost = 0, lost_prev = 1;
    int toggle = 0;
    opus_uint32 enc_final_range[2];
    opus_uint32 dec_final_range;
    int encode_only=0, decode_only=0;
    int max_frame_size = 960*6;
    int curr_read=0;
    int nb_modes_in_list=0;
    int curr_mode=0;
    int curr_mode_count=0;
    int mode_switch_time = 48000;

    //fprintf(stderr, "%s\n", opus_get_version_string());

	if ((encode && decode))
		return false;

	if (encode)
		encode_only = 1;
	else if (decode)
		decode_only = 1;
	else
		return false;

    if (!decode_only)
    {
       application = OPUS_APPLICATION_VOIP;
    }

    if (sampling_rate != 8000 && sampling_rate != 12000
     && sampling_rate != 16000 && sampling_rate != 24000
     && sampling_rate != 48000)
    {
		errMessage.assign("Opus Error: Unsupported sampling rate.  Supported sampling rates are 8000, 12000, 16000, 24000 and 48000.");
		return false;
    }

    /* defaults: */
    use_vbr = 1;
    bandwidth = OPUS_AUTO;
    max_payload_bytes = MAX_PACKET;
    complexity = 7;
    use_inbandfec = 0;
    forcechannels = OPUS_AUTO;
    use_dtx = 0;
    packet_loss_perc = 0;
    max_frame_size = 960*6;
    curr_read=0;

	//framesize 2.5 ms = sampling_rate/400
	//framesize 5 ms = sampling_rate/200
	//framesize 10 ms = sampling_rate/100
	//framesize 20 ms = sampling_rate/50
	//framesize 40 ms = sampling_rate/25
	//framesize 60 ms = sampling_rate/50
	frame_size = sampling_rate/25;

	if (!decode_only)
    {
       enc = opus_encoder_create(sampling_rate, channels, application, &err);
       if (err != OPUS_OK)
       {
			errMessage.assign("Opus Error: Cannot create encoder: ");
			errMessage.append(opus_strerror(err));
			return false;
       }
       opus_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate_bps));
       opus_encoder_ctl(enc, OPUS_SET_BANDWIDTH(bandwidth));
       opus_encoder_ctl(enc, OPUS_SET_VBR(use_vbr));
       opus_encoder_ctl(enc, OPUS_SET_VBR_CONSTRAINT(cvbr));
       opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(complexity));
       opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(use_inbandfec));
       opus_encoder_ctl(enc, OPUS_SET_FORCE_CHANNELS(forcechannels));
       opus_encoder_ctl(enc, OPUS_SET_DTX(use_dtx));
       opus_encoder_ctl(enc, OPUS_SET_PACKET_LOSS_PERC(packet_loss_perc));

       opus_encoder_ctl(enc, OPUS_GET_LOOKAHEAD(&skip));
       opus_encoder_ctl(enc, OPUS_SET_LSB_DEPTH(16));
    }
    if (!encode_only)
    {
       dec = opus_decoder_create(sampling_rate, channels, &err);
       if (err != OPUS_OK)
       {
			errMessage.assign("Opus Error: Cannot create decoder: ");
			errMessage.append(opus_strerror(err));
			return false;
       }
    }

	switch(bandwidth)
    {
    case OPUS_BANDWIDTH_NARROWBAND:
         bandwidth_string = "narrowband";
         break;
    case OPUS_BANDWIDTH_MEDIUMBAND:
         bandwidth_string = "mediumband";
         break;
    case OPUS_BANDWIDTH_WIDEBAND:
         bandwidth_string = "wideband";
         break;
    case OPUS_BANDWIDTH_SUPERWIDEBAND:
         bandwidth_string = "superwideband";
         break;
    case OPUS_BANDWIDTH_FULLBAND:
         bandwidth_string = "fullband";
         break;
    case OPUS_AUTO:
         bandwidth_string = "auto";
         break;
    default:
         bandwidth_string = "unknown";
         break;
    }

    //if (decode_only)
    //   fprintf(stderr, "Decoding with %ld Hz output (%d channels)\n",
    //                   (long)sampling_rate, channels);
    //else
    //   fprintf(stderr, "Encoding %ld Hz input at %.3f kb/s "
    //                   "in %s mode with %d-sample frames.\n",
    //                   (long)sampling_rate, bitrate_bps*0.001,
    //                   bandwidth_string, frame_size);

    in = (short*)malloc(max_frame_size*channels*sizeof(short));
    out = (short*)malloc(max_frame_size*channels*sizeof(short));
    fbytes = (unsigned char*)malloc(max_frame_size*channels*sizeof(short));
    data[0] = (unsigned char*)calloc(max_payload_bytes,sizeof(char));
    if ( use_inbandfec ) {
        data[1] = (unsigned char*)calloc(max_payload_bytes,sizeof(char));
    }

	int curpos = 0;
	int counter=0;

	while (!stop)
    {
        if (decode_only)
        {
            unsigned char ch[4];
			if (curpos + 4 >= (int)input.size())
				break;
			for (counter = 0; counter < 4; counter++)
			{
				ch[counter] = input[curpos++];
			}
            len[toggle] = char_to_int(ch);
            if (len[toggle]>max_payload_bytes || len[toggle]<0)
            {
				boost::format fmt("Opus Error: Invalid payload length: %1%");
				fmt % len[toggle];
				errMessage.assign(fmt.str());
                break;
            }
			for (counter = 0; counter < 4; counter++)
			{
				ch[counter] = input[curpos++];
			}
            enc_final_range[toggle] = char_to_int(ch);
			if (curpos + len[toggle] > (int)input.size())
            {
				boost::format fmt("Opus Error: Ran out of input, expecting %1% bytes got %2%");
				fmt % len[toggle] % input.size();
				errMessage.assign(fmt.str());
                break;
            }
			for (counter = 0; counter < len[toggle]; counter++)
			{
				data[toggle][counter] = input[curpos++];
			}
        } else {
            int i;
			for (counter = 0; (curpos < (int)input.size()) && (counter < (int)sizeof(short) * channels * frame_size); counter++)
			{
				fbytes[counter] = input[curpos++];
			}
			curr_read = counter;

            for(i=0;i<curr_read*channels;i++)
            {
                opus_int32 s;
                s=fbytes[2*i+1]<<8|fbytes[2*i];
                s=((s&0xFFFF)^0x8000)-0x8000;
                in[i]=s;
            }
            if (curr_read < frame_size*channels)
            {
                for (i=curr_read*channels;i<frame_size*channels;i++)
                   in[i] = 0;
                stop = 1;
            }
            len[toggle] = opus_encode(enc, in, frame_size, data[toggle], max_payload_bytes);
            opus_encoder_ctl(enc, OPUS_GET_FINAL_RANGE(&enc_final_range[toggle]));
            if (len[toggle] < 0)
            {
				boost::format fmt("Opus Error: encode returned %1%");
				fmt % len[toggle];
				errMessage.assign(fmt.str());
				return false;
            }
            curr_mode_count += frame_size;
            if (curr_mode_count > mode_switch_time && curr_mode < nb_modes_in_list-1)
            {
               curr_mode++;
               curr_mode_count = 0;
            }
        }

        if (encode_only)
        {
            unsigned char int_field[4];
            int_to_char(len[toggle], int_field);
			for (counter = 0; counter < 4; counter++)
			{
				output.push_back(int_field[counter]);
			}
            int_to_char(enc_final_range[toggle], int_field);
			for (counter = 0; counter < 4; counter++)
			{
				output.push_back(int_field[counter]);
			}
			for (counter = 0; counter < len[toggle]; counter++)
			{
				output.push_back(data[toggle][counter]);
			}
        } else {
            int output_samples;
            lost = len[toggle]==0 || (packet_loss_perc>0 && rand()%100 < packet_loss_perc);
            if (lost)
               opus_decoder_ctl(dec, OPUS_GET_LAST_PACKET_DURATION(&output_samples));
            else
               output_samples = max_frame_size;
            if ( count >= use_inbandfec ) {
                /* delay by one packet when using in-band FEC */
                if ( use_inbandfec  ) {
                    if ( lost_prev ) {
                        /* attempt to decode with in-band FEC from next packet */
                        output_samples = opus_decode(dec, lost ? NULL : data[toggle], len[toggle], out, output_samples, 1);
                    } else {
                        /* regular decode */
                        output_samples = opus_decode(dec, data[1-toggle], len[1-toggle], out, output_samples, 0);
                    }
                } else {
                    output_samples = opus_decode(dec, lost ? NULL : data[toggle], len[toggle], out, output_samples, 0);
                }
                if (output_samples>0)
                {
                    if (output_samples>skip) 
					{
                       int i;
                       for(i=0;i<(output_samples-skip)*channels;i++)
                       {
                          short s;
                          s=out[i+(skip*channels)];
                          fbytes[2*i]=s&0xFF;
                          fbytes[2*i+1]=(s>>8)&0xFF;
                       }
					   for (counter = 0; counter < (int)sizeof(short) * channels * (output_samples-skip); counter++)
					   {
						   output.push_back(fbytes[counter]);
					   }
                    }
                    if (output_samples<skip) 
						skip -= output_samples;
                    else 
						skip = 0;
                } else {
					boost::format fmt("Opus Error: decoding frame returned %1%");
					fmt % opus_strerror(output_samples);
					errMessage.assign(fmt.str());
					return false; //original code just continued?
                }
            }
        }

        if (!encode_only)
           opus_decoder_ctl(dec, OPUS_GET_FINAL_RANGE(&dec_final_range));
        /* compare final range encoder rng values of encoder and decoder */
        if ( enc_final_range[toggle^use_inbandfec]!=0  && !encode_only
         && !lost && !lost_prev
         && dec_final_range != enc_final_range[toggle^use_inbandfec] ) 
		{
			boost::format fmt("Opus Error: Range coder state mismatch between encoder and decoder in frame %1%: %2% vs %3%");
			fmt % (long)count % (unsigned long)enc_final_range[toggle^use_inbandfec] % (unsigned long)dec_final_range;
			errMessage.assign(fmt.str());
			return false;
        }

        lost_prev = lost;

        /* count bits */
        bits += len[toggle]*8;
        bits_max = ( len[toggle]*8 > bits_max ) ? len[toggle]*8 : bits_max;
        if ( count >= use_inbandfec ) {
            nrg = 0.0;
            if (!decode_only)
            {
                for ( k = 0; k < frame_size * channels; k++ ) {
                    nrg += in[ k ] * (double)in[ k ];
                }
            }
            if ( ( nrg / ( frame_size * channels ) ) > 1e5 ) {
                bits_act += len[toggle]*8;
                count_act++;
            }
            /* Variance */
            bits2 += len[toggle]*len[toggle]*64;
        }
        count++;
        toggle = (toggle + use_inbandfec) & 1;
    }
    //fprintf (stderr, "average bitrate:             %7.3f kb/s\n",
    //                 1e-3*bits*sampling_rate/(frame_size*(double)count));
    //fprintf (stderr, "maximum bitrate:             %7.3f kb/s\n",
    //                 1e-3*bits_max*sampling_rate/frame_size);
    //if (!decode_only)
    //   fprintf (stderr, "active bitrate:              %7.3f kb/s\n",
    //           1e-3*bits_act*sampling_rate/(frame_size*(double)count_act));
    //fprintf (stderr, "bitrate standard deviation:  %7.3f kb/s\n",
    //        1e-3*sqrt(bits2/count - bits*bits/(count*(double)count))*sampling_rate/frame_size);
    opus_encoder_destroy(enc);
    opus_decoder_destroy(dec);
    free(data[0]);
    if (use_inbandfec)
        free(data[1]);
    free(in);
    free(out);
    free(fbytes);
	return true;
}
