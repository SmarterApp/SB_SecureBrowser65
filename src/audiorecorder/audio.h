#ifndef __AUDIO_H__
#define __AUDIO_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <sstream>
#include "portaudio.h"
#include "pa_ringbuffer.h"
#include "pa_util.h"
#include "opus.h"
#include "opus_types.h"
#include "json_spirit_writer.h"

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>
#include <boost/archive/iterators/transform_width.hpp>

#include <boost/thread.hpp>

#define FRAMES_PER_BUFFER (480)
#define NUM_SECONDS     (5)
#define NUM_WRITES_PER_BUFFER   (4)
#define MAXPAYLOAD 1500
#define DITHER_FLAG     (0) 
#define PA_SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)


// Audio class that provides the functionality needed for the AIR API methods
class audio
{
public:
	audio();
	~audio();

        enum Qual { UNKNOWN, POOR, GOOD };
        enum Stat { CREATED, INIT, IDLE, ACTIVE, ERRORSTAT, STOPPING, PLAYING, PAUSED };
        enum Events { INITIALIZING, READY, AUDIOERROR, START, INPROGRESS, END, PLAYSTART, PLAYSTOP, PLAYPAUSED, PLAYRESUMED };
        Stat stat;

        typedef short SAMPLE;
        typedef int (*ThreadFunctionType)(void*);
	typedef void eventCallback(Events evt, std::string data);

	//data structrue that is passed between start/stop methods and background threads that load and save data
	typedef struct
	{
		unsigned			frameIndex;
		unsigned			maxFrameIndex;
		int                 threadSyncFlag;
		SAMPLE             *ringBufferData;
		int					ringBuffSize;
		PaUtilRingBuffer    ringBuffer;
		std::vector<unsigned char>	rawData;
		int					rawPos;
		boost::thread		backgroundThread;
		int					channels;
		std::string			reportFreqType; 
		int					reportInterval;
		std::string			limitType; 
		int					limitSize;
		bool				playComplete;
		eventCallback*		eventFunc;
		std::vector<unsigned char>	encodedData;
	} paData;

	typedef struct
	{
		int id; // 1; Id you want us to pass in the startCapture method to 
		std::string description; // “this is a sample”;
		std::vector<int> sampleSizes; // [8, 16, 32];
		std::vector<int> sampleRates; //[8000,11000,44100,48000];
		std::vector<int> channelCounts; //[1,2]
		std::vector<std::string> formats; //:[“SPX”, “WAV”];				
	} supportedDevice;

	typedef struct
	{
		bool isAvailable;
		std::vector<supportedDevice> devices;
	} capabilities;


	static unsigned NextPowerOf2(unsigned val)
	{
		val--;
		val = (val >> 1) | val;
		val = (val >> 2) | val;
		val = (val >> 4) | val;
		val = (val >> 8) | val;
		val = (val >> 16) | val;
		return ++val;
	};

	static PaError startThread( paData* pData, ThreadFunctionType fn );
	static int stopThread( paData* pData );

	static int threadFunctionSaveBuff(void* ptr);
	static int recordCallback( const void *inputBuffer, void *outputBuffer,
								unsigned long framesPerBuffer,
								const PaStreamCallbackTimeInfo* timeInfo,
								PaStreamCallbackFlags statusFlags,
								void *userData );

	static int threadFunctionLoadBuff(void* ptr);
	static int playCallback( const void *inputBuffer, void *outputBuffer,
								unsigned long framesPerBuffer,
								const PaStreamCallbackTimeInfo* timeInfo,
								PaStreamCallbackFlags statusFlags,
								void *userData );

	static short peakSignalLevel(int sampleSize, std::vector<unsigned char>& rawData, int offset, int numberOfSamples);

	//API methods:
	bool initialize(eventCallback* evtFunc);
	bool getStatus(char *stat);
	void GetSupportedStandardSampleRates(json_spirit::Object& device, const PaStreamParameters *inputParameters);
	bool getCapabilities(json_spirit::Object& dat);
	bool startCapture(int deviceId, int sampleRate, int channelCount, int sampleSize, std::string& encodingFmt, bool quality, std::string& reportFreqType, int reportInterval, std::string& limitType, int limitSize, eventCallback* func);
	bool stopCapture();
	bool play(std::string audioData, eventCallback* func);
	bool stopPlay();
	bool pausePlay();
	bool resumePlay();
	int opusEncode();
	int opusDecode();
        bool clean();

private:
	void initData();
	void addOneDevice(int i, json_spirit::Array& devices);
	static void watchforEndPlay(void* p);
	static void watchforEndRecord(void* p);
	static int stopWatchForEndPlay;
	Qual qualityCheck(int sampleSize, int sampleRate, paData* data);
	std::string ToBase64(std::vector<unsigned char> data);
	std::vector<unsigned char> ToBinary(std::string);
        std::string getErrorMessage(std::string& info);

	eventCallback*		eventFunc;
	std::string			encodingFormat;
	std::vector<unsigned char>	encodedData;
    PaStreamParameters  inputParameters,
                        outputParameters;
    PaStream*           stream;
    PaError             err;
    paData				data;
    int					complexity;
    int                 totalFrames;
    int                 numSamples;
    int                 numBytes;
    SAMPLE              max, val;
    double              average;
	bool				initialized;
	bool				doQualityCheck;

	static int sampling_rate;
	static int frame_size;
	static int channels;
	static bool paused;
	static int stopRecordingFlag;

};

#endif
