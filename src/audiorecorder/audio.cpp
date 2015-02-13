#include "audio.h"
#include <boost/lexical_cast.hpp>
#include <stdio.h>
#include "Opuswrap.h"
#include "OpusOgg.h"
#include <iostream>
#include <iterator>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <boost/algorithm/string.hpp>


//Initialize static members
int audio::sampling_rate = 48000;
int audio::frame_size = sampling_rate/50;
int audio::channels = 2;
bool audio::paused = false;


// Description: CTOR
// Params: n/a
// Returns: n/a
audio::audio()
{
  stat = CREATED;
  initialized = false;
  err = paNoError;
  complexity = 10;
  numSamples = NextPowerOf2((unsigned)(sampling_rate * 0.5 * channels));
  numBytes = numSamples * sizeof(SAMPLE);
  data.ringBufferData = (SAMPLE *) PaUtil_AllocateMemory( numBytes );
  data.ringBuffSize = numBytes;
  data.rawData.reserve(65536);
  encodingFormat.assign("OPUS");
}

// Description: DTOR
// Params: n/a
// Returns: n/a
audio::~audio()
{
  stopRecordingFlag = 1;

  clean();

  if ( data.ringBufferData ) PaUtil_FreeMemory( data.ringBufferData );

  Pa_Terminate();
}

// Description: Initialize the ringbuffer memory and the port audio library
// Params: callback function that receives 'events'
// Returns: true on no error, otherwise false
bool audio::initialize(eventCallback* evtFunc)
{
  bool retval = true;

  //clean up from previous state
  switch (stat)
  {
    case ACTIVE:
      stopCapture();
      break;
    case STOPPING:
      //stop capture already called and in progress, sleep to try and let it finish
      Pa_Sleep(2000);
      break;
    case PLAYING:
      stopPlay();
      break;
    case PAUSED:
      stopPlay();
      break;
    default:
      break;
  }

  if (stat != CREATED && stat != INIT)
  {
    Pa_Terminate();
  }

  stopWatchForEndPlay = 0;

  sampling_rate = 48000;
  frame_size = sampling_rate/50;
  channels = 2;
  paused = false;
  stopRecordingFlag = 0;

  stat = INIT;
  eventFunc = evtFunc;
  eventFunc(INITIALIZING, "");
  err = Pa_Initialize();

  if (err != paNoError)
  {
    retval = false;
    std::string msg("Error during initialize");
    eventFunc(AUDIOERROR, getErrorMessage(msg));
  }
    else
  {
    initialized = true;
    stat = IDLE;
    eventFunc(READY, "");
  }

  return retval;
}

bool audio::clean()
{
  bool retval = true;
  switch (stat)
  {
    case ACTIVE:
      retval = stopCapture();
      break;
    case STOPPING:
      //stop capture already called and in progress, sleep to try and let it finish
      Pa_Sleep(2000);
      break;
    case PLAYING:
      retval = stopPlay();
      break;
    case PAUSED:
      retval = stopPlay();
      break;
    default:
      break;
  }

  if (stat != CREATED && stat != INIT)
  {
    Pa_Terminate();
  }
  stopWatchForEndPlay = 0;
  sampling_rate = 48000;
  frame_size = sampling_rate/50;
  channels = 2;
  paused = false;
  stopRecordingFlag = 0;
  stat = CREATED;
  eventFunc = NULL;
  initialized = false;
  return retval;
}

std::string audio::getErrorMessage(std::string &message)
{
  message.append("\n");
  message.append(Pa_GetErrorText(err));
  if (err == paUnanticipatedHostError || err == paInternalError)
  {
    const PaHostErrorInfo *pErrInfo = Pa_GetLastHostErrorInfo();
    if (pErrInfo)
    {
      message.append("\nError number: ");
      std::string number;
      std::stringstream strstream;
      strstream << pErrInfo->errorCode;
      strstream >> number;
      message.append(number);
      message.append("\n");
      if (pErrInfo->errorText)
      {
        message.append(pErrInfo->errorText);
        message.append("\n");
      }
    }
    else
    {
      message.append("\nUnable to get last host error information\n");
    }
  }

  return message;
}

// Description: gets the current status of the audio library
// Params: (out) status
// Returns: false if the status is unknown (status added?), otherwise true
bool audio::getStatus(char* status)
{
  bool retval = true;
  switch (stat)
  {
    case CREATED:
      strcpy(status, "CREATED");
      break;
    case INIT:
      strcpy(status, "INITIALIZING");
      break;
    case IDLE:
      strcpy(status, "IDLE");
      break;
    case ACTIVE:
      strcpy(status, "ACTIVE");
      break;
    case ERRORSTAT:
      strcpy(status, "ERRORSTAT");
      break;
    case STOPPING:
      strcpy(status, "STOPPING");
      break;
    case PLAYING:
      strcpy(status, "PLAYING");
      break;
    case PAUSED:
      strcpy(status, "PAUSED");
      break;
    default:
      retval = false;
      strcpy(status, "INTERNAL ERROR");
      break;
  }

  return retval;
}

// Description: internal method getCapabilities uses to determine sample rates of a device
// Params: (out) device - this json encoded device object has a sample rates sub array added to it
//         inputParameters - current setting of port audio parameters (device, sample size, etc)
// Returns: n/a
void audio::GetSupportedStandardSampleRates(json_spirit::Object& device, const PaStreamParameters *inputParameters)
{
  /********
  static double standardSampleRates[] = 
  {
      8000.0, 9600.0, 11025.0, 12000.0, 16000.0, 22050.0, 24000.0, 32000.0,
      44100.0, 48000.0, 88200.0, 96000.0, 192000.0, -1 // negative terminated  list
  };
  ********/

  static double standardSampleRates[] = { 8000.0, -1 };

  int i;
  PaError err;

  json_spirit::Array sampRates;
  for( i=0; standardSampleRates[i] > 0; i++ )
  {
    err = Pa_IsFormatSupported( inputParameters, NULL, standardSampleRates[i] );
    if ( err == paFormatIsSupported )
    {
      sampRates.push_back((int)standardSampleRates[i]);
    }
  }

  device.push_back(json_spirit::Pair("sampleRates", json_spirit::Value(sampRates)));
}

// Description: add one device's info to the json array of devices for getCapabilities
// Params:
// Returns:
void audio::addOneDevice(int i, json_spirit::Array& devices)
{
  const   PaDeviceInfo *deviceInfo;
  PaStreamParameters inputParameters;

  deviceInfo = Pa_GetDeviceInfo( i );
      
  inputParameters.device = i;
  inputParameters.channelCount = deviceInfo->maxInputChannels;
  inputParameters.sampleFormat = paInt16;
  inputParameters.suggestedLatency = 0; /* ignored by Pa_IsFormatSupported() */
  inputParameters.hostApiSpecificStreamInfo = NULL;
  
  if ( deviceInfo->maxInputChannels > 0 )
  {
    json_spirit::Object device;
    device.push_back(json_spirit::Pair("id",i));
    device.push_back(json_spirit::Pair("description", deviceInfo->name));
    json_spirit::Array sampSizes;
    sampSizes.push_back(16);
    device.push_back(json_spirit::Pair("sampleSizes", json_spirit::Value(sampSizes)));
    GetSupportedStandardSampleRates( device, &inputParameters );
    json_spirit::Array chanCnts;
    int j;
    for (j=1; j <= deviceInfo->maxInputChannels; j++)
    {
      chanCnts.push_back(j);
    }
    device.push_back(json_spirit::Pair("channelCounts", json_spirit::Value(chanCnts)));
    json_spirit::Array formats;
    formats.push_back("RAW");
    formats.push_back("OPUS");
    device.push_back(json_spirit::Pair("formats", json_spirit::Value(formats)));
    devices.push_back(device);
  }
}

// Description: API method to get the capabilities of the computer
// Params: (out) dat - json encoded array of objects
// Returns: false if library not initialized, true on success
bool audio::getCapabilities(json_spirit::Object& dat)
{
  if (!initialized) return false;

  const   PaDeviceInfo *deviceInfo;
  int     i, numDevices;

  numDevices = Pa_GetDeviceCount();
  bool isAvailable = false;
  for( i=0; i<numDevices; i++ )
  {
    deviceInfo = Pa_GetDeviceInfo( i );
    if ( deviceInfo->maxInputChannels > 0 )
    {
      isAvailable = true;
      break;
    }
  }
  dat.push_back(json_spirit::Pair("isAvailable", isAvailable));

  int defaultMic = Pa_GetDefaultInputDevice();

  json_spirit::Array devices;

  addOneDevice(defaultMic, devices);

  for( i=0; i<numDevices; i++ )
  {
    if (i != defaultMic)
    {
      addOneDevice(i, devices);
    }
  }

  dat.push_back(json_spirit::Pair("supportedInputDevices", json_spirit::Value(devices)));

  return true;
}

void audio::initData()
{
  numSamples = NextPowerOf2((unsigned)(sampling_rate * 0.5 * channels));
  numBytes = numSamples * sizeof(SAMPLE);

  data.channels = channels;
  if ( data.ringBufferData == NULL )
  {
    stat = ERRORSTAT;
    eventFunc(AUDIOERROR, "Insufficient Memory");
    err = paInsufficientMemory;
  }
    else
  {
    if (PaUtil_InitializeRingBuffer(&data.ringBuffer, sizeof(SAMPLE), numSamples, data.ringBufferData) < 0)
    {
      stat = ERRORSTAT;
      eventFunc(AUDIOERROR, "Invalid ring buffer size");
      err = paInternalError;  //Failed to initialize ring buffer. Size is not power of 2 ??
    }
  }
}

// Description: kicks off background thread to save data then starts recording
// Params:
// Returns:
bool audio::startCapture(int deviceId, int sampleRate, int channelCount, int sampleSize, std::string& encodingFmt, bool quality, std::string& reportFreqType, int reportInterval, std::string& limitType, int limitSize, eventCallback evtFunc)
{
  bool retval = true;

  if (stat != IDLE)
  {
    eventFunc(AUDIOERROR, "Error: not Idle, cannot start capture");
    return false;
  }

  channels = channelCount;
  initData();

  PaUtil_FlushRingBuffer(&data.ringBuffer);
  data.rawData.clear();
  data.encodedData.clear();

  data.limitSize = limitSize;
  data.limitType = limitType;

  if (limitType == "time")
  {
    data.maxFrameIndex = limitSize * sampleRate; //seconds * sample rate
  }

  doQualityCheck = quality;
  sampling_rate = sampleRate;
  channels = channelCount;
  frame_size = sampling_rate/50;
  data.frameIndex = 0;
  data.channels = channelCount;
  data.reportFreqType = reportFreqType;
  data.reportInterval = reportInterval;
  data.eventFunc = evtFunc;
  eventFunc = evtFunc;
  encodingFormat = encodingFmt;

  numSamples = NextPowerOf2((unsigned)(sampling_rate * 0.5 * channels));
  numBytes = numSamples * sizeof(SAMPLE);

  inputParameters.device = deviceId;

  if (inputParameters.device == paNoDevice) 
  {
    stat = ERRORSTAT;
    eventFunc(AUDIOERROR, "Invalid input device");
    retval = false;
  }

  inputParameters.channelCount = channelCount;                    /* stereo input */
  inputParameters.sampleFormat = PA_SAMPLE_TYPE;
  inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
  inputParameters.hostApiSpecificStreamInfo = NULL;

  /* Record some audio. -------------------------------------------- */
  err = Pa_OpenStream(
        &stream,
        &inputParameters,
        NULL,                  /* &outputParameters, */
        sampleRate,
        FRAMES_PER_BUFFER,
        paClipOff,      /* we won't output out of range samples so don't bother clipping them */
        recordCallback,
        &data );

  if ( err == paNoError )
  {
    err = startThread(&data, threadFunctionSaveBuff);

    if ( err == paNoError )
    {
      stopRecordingFlag = 0;
      boost::thread(watchforEndRecord, this);
      eventFunc(START, "");
      err = Pa_StartStream( stream );
      stat = ACTIVE;
    }
  }

  if ( err != paNoError ) 
  {
    stat = ERRORSTAT;
    std::string msg("Error during start capture");
    eventFunc(AUDIOERROR, getErrorMessage(msg));
    retval = false;
  }

  return retval;
}

// Description: stops port audio stream from recording and stops the background thread.  if encoding then calls encoding
// Params:
// Returns:
bool audio::stopCapture()
{
  bool retval = true;

  if (stat != ACTIVE)
  {
    eventFunc(AUDIOERROR, "Error: not recording, cannot stop capture");
    return false;
  }

  stat = STOPPING;

  // PETE: comment out sleep call
  // Pa_Sleep(2000);

  err = Pa_CloseStream( stream );

  if ( err == paNoError )
  {
    /* Stop the thread */
    err = stopThread(&data);
    if ( err != paNoError ) 
    {
      stat = ERRORSTAT;
      eventFunc(AUDIOERROR, "Error stopping background thread");
      retval = false;
    }
      else
    {
      Qual qual = UNKNOWN;
      json_spirit::Object results;

      if (doQualityCheck)
      {
        qual = qualityCheck(16, sampling_rate, &data);
      }

      switch (qual)
      {
        case GOOD:
          results.push_back(json_spirit::Pair("qualityIndicator", "GOOD"));
          break;
        case POOR:
          results.push_back(json_spirit::Pair("qualityIndicator", "POOR"));
          break;
        case UNKNOWN:
          results.push_back(json_spirit::Pair("qualityIndicator", "UNKNOWN"));
          break;
      }

      if (encodingFormat=="OPUS")
      {
        std::string message;
        //encode the data
        if (OpusOgg::encode(sampling_rate, channels, -1, data.rawData, data.encodedData, message))
        {
          results.push_back(json_spirit::Pair("base64", ToBase64(data.encodedData)));
        }
          else
        {
          stat = ERRORSTAT;
          eventFunc(AUDIOERROR, message);
          retval = false;
        }
      }
        else
      {
        results.push_back(json_spirit::Pair("base64", ToBase64(data.rawData)));
      }

      std::string output;

      if (retval)
      {
        output = json_spirit::write(results, json_spirit::pretty_print);
        eventFunc(END, output);
      }
      stat = IDLE;
    }
  }
    else
  {
    stopThread(&data); //still try and stop the thread
    stat = ERRORSTAT;
    std::string msg("Error during stop capture");
    eventFunc(AUDIOERROR, getErrorMessage(msg));
    retval = false;
  }


  return retval;
}

// Description: Method to estimate the peak signal level given the number of bytes
// Params:
// Returns:
short audio::peakSignalLevel(int sampleSize, std::vector<unsigned char>& rawData, int offset, int numberOfSamples) 
{
  short max = SHRT_MIN;
    
  if (sampleSize != 16) return max;  // We assume 16 bit for now 
    
  for (int p = offset; p < offset + numberOfSamples*2; p += 2) 
  {
    short thisValue = (short) (rawData[p] + (rawData[p + 1] << 8));
    max = abs(thisValue) > max ? thisValue : max;     
  }
    
  return max;
}

// Description: performs a quality check on the audio (taken from original AIR java)
// Params:
// Returns:
audio::Qual audio::qualityCheck(int sampleSize, int sampleRate, paData* dat)
{
  Qual retval = UNKNOWN;

  // if not 1 channel and 16 bit mono audio
  if (dat->channels == 1 && sampleSize == 16)
  {
    // if at least 1 second of audio
    {
      // Step 1 - Establish the noise floor.
      // Basically, we are using the first 500ms of the audio to figure out what 
      // the noise floor should be assuming that in the first 500ms of the recording, it is mostly
      // ambient noise and the speaker has not had a chance to speak yet. 
      int numberOfSamples = (int)(sampleRate * 500/1000); //500ms worth of audio data   
      double noiseFloor = abs(peakSignalLevel(sampleSize, dat->rawData, 0, numberOfSamples));
    
      // Step 2 - Now estimate the level of 500ms chunks. Compare this against the noise floor to 
      // estimate the number of chunks over the noise floor and those below.
      numberOfSamples = (int)(sampleRate * 500/1000) ; //500ms of audio data    
      int chunkSize = numberOfSamples*sampleSize/8;
      int numChunksOverNoiseFloor = 0, numNoiseChunks = 0;
      for (int i=0;((int)dat->rawData.size() - i)>chunkSize;i=i+chunkSize) 
      {
        // get the average signal level over the time period by sliding the window
        double signalLevel = peakSignalLevel(sampleSize, dat->rawData, i, numberOfSamples);
        // If the signal level is higher than twice the noise floor, treat this chunk as good.
        if (abs(signalLevel) > noiseFloor*2.0) 
        { 
          numChunksOverNoiseFloor++;
        } 
          else 
        {
          numNoiseChunks++;
        }     
      }
    
      // Step 3 - If the number of 500ms "good" chunks is greater than 25% of total, then we are good
      // We are also OK if we have 15 seconds worth of chunks over Noise floor
      if ((numChunksOverNoiseFloor*1.0/(numNoiseChunks+numChunksOverNoiseFloor) > 0.25) ||
          numChunksOverNoiseFloor > 30) 
      {
        retval = GOOD;
      }
        else
      {
        retval = POOR;
      }
    }
  }

  return retval;
}

int audio::stopWatchForEndPlay = 0;

// Description: method to run as background thread that will watch for play to finish
// Params: void* to the instance of the audio object instance that started the thread
// Returns:
void audio::watchforEndPlay(void* p)
{
  audio* inst = (audio*)p;
  PaError err = paNoError;

  while(!stopWatchForEndPlay && (paused || ( err = Pa_IsStreamActive( inst->stream ) ) == 1 ))
  {
    Pa_Sleep(100);
  }

  // if we didn't already close the stream on the main thread
  if ( !stopWatchForEndPlay && err >= 0 ) 
  {
    Pa_CloseStream( inst->stream );
    inst->stat = IDLE;
    inst->eventFunc(PLAYSTOP, "");
  }
}

// Description: opens default output device and starts port audio to begin playing. also starts up background thread to load data.
// Params:
// Returns:
bool audio::play(std::string audioData, eventCallback* func)
{
  bool retval = true;

  if (stat != IDLE && stat != PAUSED)
  {
    eventFunc(AUDIOERROR, "Error: not idle, cannot start playback");
    return false;
  }

  if (stat == PAUSED)
  {
    stopPlay();
  }

  eventFunc = func;
  stopWatchForEndPlay = 0;
  data.frameIndex = 0;
  data.rawPos = 0;
  data.playComplete = false;
  paused = false;
  PaUtil_FlushRingBuffer(&data.ringBuffer);

  if (encodingFormat=="OPUS")
  {
    data.rawData.clear();
    
    try
    {
      data.encodedData = ToBinary(audioData);
    }
      catch (...)
      {
        data.rawData.clear();
        stat = ERRORSTAT;
        eventFunc(AUDIOERROR, "Invalid Base64 encoding");
        err = paInternalError;
        retval = false;
      }

      if (retval)
      {
        std::string message;

        if (!OpusOgg::decode(data.encodedData, data.rawData, message, sampling_rate, channels))
        {
          stat = ERRORSTAT;
          eventFunc(AUDIOERROR, message);
          err = paInternalError;
          retval = false;
        }
          else
        {
          data.channels = channels;
        }
      }
    }
    else
    {
      data.rawData.clear();
      try
      {
        data.rawData = ToBinary(audioData);
      }
        catch (...)
        {
          data.rawData.clear();
          stat = ERRORSTAT;
          eventFunc(AUDIOERROR, "Invalid Base64 encoding");
          err = paInternalError;
          retval = false;
        }
      }

      if (retval)
      {
        initData();
    
        err = Pa_OpenDefaultStream(
          &stream,
          0,
          channels,
          PA_SAMPLE_TYPE,
          sampling_rate,
          FRAMES_PER_BUFFER,
          playCallback,
          &data );

    if ( err == paNoError ) 
    {
      if ( stream )
      {
        err = startThread(&data, threadFunctionLoadBuff);
        if ( err == paNoError ) 
        {
          err = Pa_StartStream( stream );
          boost::thread(watchforEndPlay, this);
          if ( err == paNoError )
          {
            stat = PLAYING;
            eventFunc(PLAYSTART, "");
          }
        }
      }
    }

  if ( err != paNoError )
  {
    stat = ERRORSTAT;
    std::string msg("Error during play");
    eventFunc(AUDIOERROR, getErrorMessage(msg));
    retval = false;
  }
      }

  return retval;
}

// Description: stops port audio stream and stops background thread
// Params:
// Returns:
bool audio::stopPlay()
{
  bool retval = true;

  if (stat != PLAYING && stat != PAUSED)
  {
    eventFunc(AUDIOERROR, "Error: not playing, cannot stop play");
    return false;
  }

  stopWatchForEndPlay = 1;

  err = Pa_CloseStream( stream );
  if ( err == paNoError )
  {
    /* Stop the thread */
    err = stopThread(&data);
  }
    else
  {
    //just try and stop thread but preserve previous error
    stopThread(&data);
  }

  if ( err != paNoError )
  {
    stat = ERRORSTAT;
    std::string msg("Error during stop play");
    eventFunc(AUDIOERROR, getErrorMessage(msg));
    retval = false;
  }
    else
  {
    stat = IDLE;
    eventFunc(PLAYSTOP, "");
  }

  return retval;
}

// Description:
// Params:
// Returns:
bool audio::pausePlay()
{
  bool retval = true;
  
  if (stat != PLAYING)
  {
    eventFunc(AUDIOERROR, "Error: not playing, cannot pause");
    return false;
  }

  // err = Pa_StopStream( stream );
  paused = true;
  stat = PAUSED;
  eventFunc(PLAYPAUSED, "");

  /********
  if ( err != paNoError )
  {
    stat = ERRORSTAT;
    eventFunc(AUDIOERROR, getErrorMessage(std::string("Error during pause play")));
    retval = false;
  }
  ********/

  return retval;
}

// Description:
// Params:
// Returns:
bool audio::resumePlay()
{
  bool retval = true;

  if (stat != PAUSED)
  {
    eventFunc(AUDIOERROR, "Error: not paused, cannot resume playing");
    return false;
  }

  // err = Pa_StartStream( stream );
  paused = false;
  stat = PLAYING;
  eventFunc(PLAYRESUMED, "");

  /********
  if ( err != paNoError )
  {
    stat = ERRORSTAT;
    eventFunc(AUDIOERROR, getErrorMessage(std::string("Error during resume play")));
    retval = false;
  }
  ********/

  return retval;
}

// Description:
// Params:
// Returns:
int audio::threadFunctionSaveBuff(void* ptr)
{
  long lastReported = 0;
    paData* pData = (paData*)ptr;

  /* Mark thread started */
  pData->threadSyncFlag = 0;
  while (1)
  {
    ring_buffer_size_t elementsInBuffer = PaUtil_GetRingBufferReadAvailable(&pData->ringBuffer);
    if ( (elementsInBuffer >= pData->ringBuffer.bufferSize / NUM_WRITES_PER_BUFFER) ||
        pData->threadSyncFlag )
    {
      void* ptr[2] = {0};
      ring_buffer_size_t sizes[2] = {0};

      /* By using PaUtil_GetRingBufferReadRegions, we can read directly from the ring buffer */
      ring_buffer_size_t elementsToRead = PaUtil_GetRingBufferReadRegions(&pData->ringBuffer, elementsInBuffer, ptr + 0, sizes + 0, ptr + 1, sizes + 1);
      if (elementsToRead > 0)
      {
        int i;
        for (i = 0; i < 2 && ptr[i] != NULL; ++i)
        {
          //store in temp buffer
          int j;
          unsigned char *dat = (unsigned char*)ptr[i];
          for (j = 0; j < pData->ringBuffer.elementSizeBytes * sizes[i]; j++)
          {
            //without encoding:
            pData->rawData.push_back(*dat);
            dat++;
          }

        }
        PaUtil_AdvanceRingBufferReadIndex(&pData->ringBuffer, elementsToRead);
      }

      if (pData->reportFreqType == "size")
      {
        if ((int)pData->rawData.size() > lastReported + pData->reportInterval)
        {
          pData->eventFunc(INPROGRESS, boost::lexical_cast<std::string>(pData->rawData.size()));
          lastReported = pData->rawData.size();
        }
      }
      else if (pData->reportFreqType == "time")
      {
        if ((int)pData->rawData.size()/sampling_rate > lastReported + pData->reportInterval)
        {
          pData->eventFunc(INPROGRESS, boost::lexical_cast<std::string>(pData->rawData.size()/sampling_rate));
          lastReported = pData->rawData.size()/sampling_rate;
        }
      }

      if (pData->limitType == "size")
      {
        if ((int)pData->rawData.size() >= pData->limitSize)
        {
          stopRecordingFlag = 1;
        }
      }

      if (pData->threadSyncFlag && PaUtil_GetRingBufferReadAvailable(&pData->ringBuffer) == 0)
      {
        break;
      }
    }

    /* Sleep a little while... */
    // TEST SEEMS UNNECESSARY --PETE
    // Pa_Sleep(10);
  }

  pData->threadSyncFlag = 0;
  return 0;
}

int audio::stopRecordingFlag = 0;

void audio::watchforEndRecord(void* p)
{
  audio* inst = (audio*)p;
  PaError err = 1;

  while(!stopRecordingFlag && err == 1)
  {
    Pa_Sleep(100);
    err = Pa_IsStreamActive(inst->stream);
  }

  if ( stopRecordingFlag ) 
  {
    inst->stopCapture();
  }
}

// Description: This routine will be called by the PortAudio engine when audio is ready.
//        It may be called at interrupt level on some machines so don't do anything
//        that could mess up the system like calling malloc() or free().
// Params:
// Returns:
int audio::recordCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
  paData *data = (paData*)userData;

  if ( data->limitType == "time" && (data->maxFrameIndex - data->frameIndex) < framesPerBuffer )
  {
    stopRecordingFlag = 1;
  }

  ring_buffer_size_t elementsWriteable = PaUtil_GetRingBufferWriteAvailable(&data->ringBuffer);
  ring_buffer_size_t elementsToWrite = std::min(elementsWriteable, (ring_buffer_size_t)(framesPerBuffer * data->channels));
  const SAMPLE *rptr = (const SAMPLE*)inputBuffer;

  (void) outputBuffer; /* Prevent unused variable warnings. */
  (void) timeInfo;
  (void) statusFlags;
  (void) userData;

  data->frameIndex += PaUtil_WriteRingBuffer(&data->ringBuffer, rptr, elementsToWrite);

  return paContinue;
}

// Description:
// Params:
// Returns:
int audio::threadFunctionLoadBuff(void* ptr)
{
    paData* pData = (paData*)ptr;

    while (1)
    {
      ring_buffer_size_t elementsInBuffer = PaUtil_GetRingBufferWriteAvailable(&pData->ringBuffer);

      if (elementsInBuffer >= pData->ringBuffer.bufferSize / NUM_WRITES_PER_BUFFER)
      {
        void* ptr[2] = {0};
        ring_buffer_size_t sizes[2] = {0};

        /* By using PaUtil_GetRingBufferWriteRegions, we can write directly into the ring buffer */
        PaUtil_GetRingBufferWriteRegions(&pData->ringBuffer, elementsInBuffer, ptr + 0, sizes + 0, ptr + 1, sizes + 1);

        if ((int)pData->rawPos < (int)pData->rawData.size()) // if !end of large buffer
        {
          ring_buffer_size_t itemsReadFromFile = 0;
          int i;
          for (i = 0; i < 2 && ptr[i] != NULL; ++i)
          {
            int j;
            unsigned char* dat = (unsigned char*)ptr[i];
            for (j = 0; j < (int)pData->ringBuffer.elementSizeBytes * sizes[i] && (int)pData->rawPos < (int)pData->rawData.size(); j++)
            {
              *dat = pData->rawData[pData->rawPos];
              pData->rawPos++;
            
              dat++;
            }

            itemsReadFromFile += j/2; //sizes[i];
          }

          PaUtil_AdvanceRingBufferWriteIndex(&pData->ringBuffer, itemsReadFromFile);

          /* Mark thread started here, that way we "prime" the ring buffer before playback */
          pData->threadSyncFlag = 0;
        }
          else
        {
          /* No more data to read */
          pData->playComplete = true;
          break;
        }
      }

      // if stop thread is called, threadsyncflag will be 1 again, signal 0 back and break
      if (pData->threadSyncFlag)
      {
        pData->threadSyncFlag = 0;
        break;
      }

      /* Sleep a little while... */
      Pa_Sleep(20);
    }

    return 0;
}

// Description: This routine will be called by the PortAudio engine when audio is needed.
//        It may be called at interrupt level on some machines so don't do anything
//        that could mess up the system like calling malloc() or free().
// Params:
// Returns:
int audio::playCallback( const void *inputBuffer, void *outputBuffer,
              unsigned long framesPerBuffer,
              const PaStreamCallbackTimeInfo* timeInfo,
              PaStreamCallbackFlags statusFlags,
              void *userData )
{
  int retval = paContinue;
  paData *data = (paData*)userData;
  ring_buffer_size_t elementsToPlay = PaUtil_GetRingBufferReadAvailable(&data->ringBuffer);
  ring_buffer_size_t elementsToRead = std::min(elementsToPlay, (ring_buffer_size_t)(framesPerBuffer * data->channels));
  SAMPLE* wptr = (SAMPLE*)outputBuffer;

  (void) inputBuffer; /* Prevent unused variable warnings. */
  (void) timeInfo;
  (void) statusFlags;
  (void) userData;

  if (!paused)
  {
    data->frameIndex += PaUtil_ReadRingBuffer(&data->ringBuffer, wptr, elementsToRead);
    if (data->playComplete)
    {
      retval = paComplete;
    }
  }
    else
  {
    int i;
    for (i=0; i < (int)(framesPerBuffer * data->channels); i++)
    {
      *wptr++ = 0;
    }
  }

  return retval;
}

// Description:
// Params:
// Returns:
PaError audio::startThread( paData* pData, ThreadFunctionType fn )
{
  PaError retval = paNoError;
  int counter = 0;

  pData->threadSyncFlag = 1;
  pData->backgroundThread = boost::thread(fn, pData);

  /* Wait for thread to startup */
  while (pData->threadSyncFlag && counter < 1000) 
  {
    Pa_Sleep(10);
    counter++;
  }

  if (pData->threadSyncFlag)
  {
    //timed out waiting for thread to start
    retval = paInternalError;
  }

  return retval;
}

// Description:
// Params:
// Returns:
int audio::stopThread( paData* pData )
{
  PaError retval = paNoError;
  int counter = 0;
  pData->threadSyncFlag = 1;

  /* Wait for thread to stop */
  while (pData->threadSyncFlag && counter < 1000) 
  {
    Pa_Sleep(10);
    counter++;
  }

  if (pData->threadSyncFlag)
  {
    //timed out waiting for thread to start
    retval = paInternalError;
  }

  pData->backgroundThread.detach();

  return retval;
}

// Description: converts from binary to base64
// Params:
// Returns:
std::string audio::ToBase64(std::vector<unsigned char> data)
{
  using namespace boost::archive::iterators;

  // Pad with 0 until a multiple of 3
  unsigned int paddedCharacters = 0;
  while(data.size() % 3 != 0)
  {
    paddedCharacters++;
    data.push_back(0x00);
  }

  // Crazy typedef black magic
  typedef
    base64_from_binary<    // convert binary values to base64 characters
    transform_width<   // retrieve 6 bit integers from a sequence of 8 bit bytes
    const unsigned char *, 6 ,8
    >
    >
    base64Iterator; // compose all the above operations in to a new iterator

  // Encode the buffer and create a string
  std::string encodedString(base64Iterator(&data[0]), base64Iterator(&data[0] + (data.size() - paddedCharacters)));

  // Add '=' for each padded character used
  for(unsigned int i = 0; i < paddedCharacters; i++)
  {
    encodedString.push_back('=');
  }

  return encodedString;
}

// Description: Converts from base64 to binary
// Params:
// Returns:
std::vector<unsigned char> audio::ToBinary(std::string base64)
{
  using namespace boost::archive::iterators;

  boost::algorithm::trim(base64);
  unsigned int paddChars = count(base64.begin(), base64.end(), '=');
  std::replace(base64.begin(),base64.end(),'=','A'); // replace '=' by base64 encoding of '\0'
  typedef 
    transform_width< //change back to 8 bit bytes from 6
    binary_from_base64< //convert base64 back to binary
    remove_whitespace<
    std::string::const_iterator
    > 
    >, 8, 6 
    > it_binary_t;
  std::vector<unsigned char> decodedDat(it_binary_t(base64.begin()), it_binary_t(base64.end()));
  decodedDat.erase(decodedDat.end()-paddChars, decodedDat.end());  // erase padding '\0' characters

  return decodedDat;
}
