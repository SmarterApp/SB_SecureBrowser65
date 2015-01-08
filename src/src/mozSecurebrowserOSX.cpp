/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Securebrowser code.
 *
 * The Initial Developer of the Original Code is
 * Securebrowser forSecurebrowser Free Software Development. 
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozSecurebrowserOSX.h"
#include "stdio.h"

#include "nsTArray.h"
#include "nsServiceManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"

using namespace mozilla::dom;

class SBRunnable : public nsRunnable
{
  public:
    SBRunnable(const nsAString & aMsg) :
      mMsg(aMsg) 
    {
      MOZ_ASSERT(!NS_IsMainThread()); // This should be running on the worker thread
    }

    NS_IMETHOD Run() 
    {
      MOZ_ASSERT(NS_IsMainThread()); // This method is supposed to run on the main thread!

      printf("-------- RUN ON MAIN THREAD --------\n");

      nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1");
      nsresult rv = observerService->NotifyObservers(nsnull, "sb-word-speak", mMsg.get());

      return rv;
    }

  private:
    nsAutoString mMsg;
};


// #define MAXPATHLEN 256

// Returns an iterator containing the primary (built-in) Ethernet interface. The caller is responsible for
// releasing the iterator after the caller is done with it.
kern_return_t mozSecurebrowser::FindEthernetInterfaces(io_iterator_t *matchingServices)
{
    kern_return_t    kernResult; 
    mach_port_t      masterPort;
    CFMutableDictionaryRef  matchingDict;
    CFMutableDictionaryRef  propertyMatchDict;
    
    // Retrieve the Mach port used to initiate communication with I/O Kit
    kernResult = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (KERN_SUCCESS != kernResult)
    {
        printf("IOMasterPort returned %d\n", kernResult);
        return kernResult;
    }
    
    // Ethernet interfaces are instances of class kIOEthernetInterfaceClass. 
    // IOServiceMatching is a convenience function to create a dictionary with the key kIOProviderClassKey and 
    // the specified value.
    matchingDict = IOServiceMatching(kIOEthernetInterfaceClass);

    // Note that another option here would be:
    // matchingDict = IOBSDMatching("en0");
        
    if (NULL == matchingDict)
    {
        printf("IOServiceMatching returned a NULL dictionary.\n");
    }
    else {
        // Each IONetworkInterface object has a Boolean property with the key kIOPrimaryInterface. Only the
        // primary (built-in) interface has this property set to TRUE.
        
        // IOServiceGetMatchingServices uses the default matching criteria defined by IOService. This considers
        // only the following properties plus any family-specific matching in this order of precedence 
        // (see IOService::passiveMatch):
        //
        // kIOProviderClassKey (IOServiceMatching)
        // kIONameMatchKey (IOServiceNameMatching)
        // kIOPropertyMatchKey
        // kIOPathMatchKey
        // kIOMatchedServiceCountKey
        // family-specific matching
        // kIOBSDNameKey (IOBSDNameMatching)
        // kIOLocationMatchKey
        
        // The IONetworkingFamily does not define any family-specific matching. This means that in            
        // order to have IOServiceGetMatchingServices consider the kIOPrimaryInterface property, we must
        // add that property to a separate dictionary and then add that to our matching dictionary
        // specifying kIOPropertyMatchKey.
            
        propertyMatchDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
                                                       &kCFTypeDictionaryKeyCallBacks,
                                                       &kCFTypeDictionaryValueCallBacks);
    
        if (NULL == propertyMatchDict)
        {
            printf("CFDictionaryCreateMutable returned a NULL dictionary.\n");
        }
        else {
            // Set the value in the dictionary of the property with the given key, or add the key 
            // to the dictionary if it doesn't exist. This call retains the value object passed in.
            CFDictionarySetValue(propertyMatchDict, CFSTR(kIOPrimaryInterface), kCFBooleanTrue); 
            
            // Now add the dictionary containing the matching value for kIOPrimaryInterface to our main
            // matching dictionary. This call will retain propertyMatchDict, so we can release our reference 
            // on propertyMatchDict after adding it to matchingDict.
            CFDictionarySetValue(matchingDict, CFSTR(kIOPropertyMatchKey), propertyMatchDict);
            CFRelease(propertyMatchDict);
        }
    }
    
    // IOServiceGetMatchingServices retains the returned iterator, so release the iterator when we're done with it.
    // IOServiceGetMatchingServices also consumes a reference on the matching dictionary so we don't need to release
    // the dictionary explicitly.
    kernResult = IOServiceGetMatchingServices(masterPort, matchingDict, matchingServices);    
    if (KERN_SUCCESS != kernResult)
    {
        printf("IOServiceGetMatchingServices returned %d\n", kernResult);
    }
        
    return kernResult;
}

// Given an iterator across a set of Ethernet interfaces, return the MAC address of the last one.
// If no interfaces are found the MAC address is set to an empty string.
// In this sample the iterator should contain just the primary interface.
kern_return_t mozSecurebrowser::GetMACAddress(io_iterator_t intfIterator, UInt8 *MACAddress)
{
    io_object_t    intfService;
    io_object_t    controllerService;
    kern_return_t  kernResult = KERN_FAILURE;
    
    // Initialize the returned address
    bzero(MACAddress, kIOEthernetAddressSize);
    
    // IOIteratorNext retains the returned object, so release it when we're done with it.
    while ((intfService = IOIteratorNext(intfIterator)))
    {
        CFTypeRef  MACAddressAsCFData;        

        // IONetworkControllers can't be found directly by the IOServiceGetMatchingServices call, 
        // since they are hardware nubs and do not participate in driver matching. In other words,
        // registerService() is never called on them. So we've found the IONetworkInterface and will 
        // get its parent controller by asking for it specifically.
        
        // IORegistryEntryGetParentEntry retains the returned object, so release it when we're done with it.
        kernResult = IORegistryEntryGetParentEntry( intfService,
                                                    kIOServicePlane,
                                                    &controllerService );

        if (KERN_SUCCESS != kernResult)
        {
            printf("IORegistryEntryGetParentEntry returned 0x%08x\n", kernResult);
        }
        else {
            // Retrieve the MAC address property from the I/O Registry in the form of a CFData
            MACAddressAsCFData = IORegistryEntryCreateCFProperty( controllerService,
                                                                  CFSTR(kIOMACAddress),
                                                                  kCFAllocatorDefault,
                                                                  0);
            if (MACAddressAsCFData)
            {
                CFShow(MACAddressAsCFData); // for display purposes only; output goes to stderr

                // Get the raw bytes of the MAC address from the CFData
                CFDataGetBytes((CFDataRef)MACAddressAsCFData, CFRangeMake(0, kIOEthernetAddressSize), MACAddress);

                CFRelease(MACAddressAsCFData);
            }
                
            // Done with the parent Ethernet controller object so we release it.
            (void) IOObjectRelease(controllerService);
        }
        
        // Done with the Ethernet interface object so we release it.
        (void) IOObjectRelease(intfService);
    }
        
    return kernResult;
}

static void 
WordCallBackProc(SpeechChannel inSpeechChannel, long inRefCon, long inWordPos, short inWordLen)
{
  printf("-------- WORD CALLBACK CALLED --------\n");

  nsAutoString msg;
  msg.Assign(NS_LITERAL_STRING("WordStart:"));

  PRInt64 start = inWordPos;
  msg.AppendInt(start);

  msg.Append(NS_LITERAL_STRING(", WordLength:"));

  PRInt64 length = inWordLen;
  msg.AppendInt(length);

  printf("-------- (%s) --------\n", NS_ConvertUTF16toUTF8(msg).get());

  nsCOMPtr<nsIRunnable> resultrunnable = new SBRunnable(msg);
  NS_DispatchToMainThread(resultrunnable);
}

static void
SyncCallBackProc(SpeechChannel inSpeechChannel, long inRefCon, OSType syncMessage)
{
  printf("-------- SYNC CALLBACK CALLED --------\n");

  nsAutoString msg;
  msg.Assign(NS_LITERAL_STRING("Sync"));

  printf("-------- (%s) --------\n", NS_ConvertUTF16toUTF8(msg).get());

  nsCOMPtr<nsIRunnable> resultrunnable = new SBRunnable(msg);
  NS_DispatchToMainThread(resultrunnable);
}

/********
static void 
SpeechDoneCallBackProc(SpeechChannel chan, long refCon)
{
  printf("-------- SPEECH DONE CALLBACK CALLED --------\n");

  nsAutoString msg;
  msg.Assign(NS_LITERAL_STRING("SpeechDone"));

  printf("-------- (%s) --------\n", NS_ConvertUTF16toUTF8(msg).get());

  nsCOMPtr<nsIRunnable> resultrunnable = new SBRunnable(msg);
  NS_DispatchToMainThread(resultrunnable);
}

static void 
SpeechPhonemeProc(SpeechChannel chan, long refCon)
{
  printf("-------- SPEECH PHONEME CALLBACK CALLED --------\n");

  nsAutoString msg;
  msg.Assign(NS_LITERAL_STRING("SpeechPhonem"));

  printf("-------- (%s) --------\n", NS_ConvertUTF16toUTF8(msg).get());

  nsCOMPtr<nsIRunnable> resultrunnable = new SBRunnable(msg);
  NS_DispatchToMainThread(resultrunnable);
}
********/

// constructor
mozSecurebrowser::mozSecurebrowser()
  : mCH(NULL),
  mPrefs(NULL),
  mSoundDevice(PR_TRUE),
  mVoiceSpecInitialized(PR_FALSE),
  mListenerInitialized(PR_FALSE)
{
  printf ("CREATE: mozSecurebrowser (%p)\n", (void*)this);

  mSoundDevice = PR_TRUE;
  mLastPitch = mLastVolume = -1;

  mPrefs = do_GetService(NS_PREFSERVICE_CONTRACTID);

  // get these prefs so we can then reset back after using permissive mode
  mPrefs->GetBoolPref("bmakiosk.system.enableKillProcess", &mKillProcess);
  mPrefs->GetBoolPref("bmakiosk.system.shutdownOnNewProcess", &mShutDown);
  // mPrefs->GetBoolPref("bmakiosk.mode.permissive", &mPermissive);

  mPermissive = PR_FALSE;

  // call to ensure we have a sound card ...
  PRInt32 vol;
  GetSystemVolume(&vol);
}

nsresult
mozSecurebrowser::Init()
{
  return NS_OK;
}

// destructor
mozSecurebrowser::~mozSecurebrowser()
{
  if (mCH)
  {
    // printf("Bookmark1 --\n");
    // Balaji - This is causing a crash on some Mac 10.4.11 machines. 
    //OSStatus err = DisposeSpeechChannel(mCH);

    //if (err != noErr) printf("DESTRUCTOR:ERROR (%d)\n", err); 
  }

  // NS_IF_RELEASE(gObserverService);
  // NS_IF_RELEASE(gSBObject);

  printf ("DESTROY: mozSecurebrowser (%p)\n", (void*)this);
}

// QueryInterface implementation for mozSecurebrowser
NS_INTERFACE_MAP_BEGIN(mozSecurebrowser)
    NS_INTERFACE_MAP_ENTRY(mozISecurebrowser)
    NS_INTERFACE_MAP_ENTRY(nsISupports)
    NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Runtime)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(mozSecurebrowser)
NS_IMPL_RELEASE(mozSecurebrowser)


NS_IMETHODIMP
mozSecurebrowser::GetVersion(nsAString & _retval)
{
  _retval = NS_LITERAL_STRING(MOZ_APP_VERSION);

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetRunningProcessList(nsAString & _retval)
{
  printf("mozSecurebrowser::GetRunningProcessList\n"); 

  ProcessSerialNumber PSN;
  ProcessInfoRec info;
  Str63 name;
  info.processInfoLength = sizeof(ProcessInfoRec);
  info.processName       = name;
#ifndef HAVE_64BIT_OS
  info.processAppSpec    = nsnull;
#else
  info.processAppRef = nsnull;
#endif
  
  GetProcessInformation(&PSN, &info);
             
  ProcessSerialNumber     p;
  p.highLongOfPSN = p.lowLongOfPSN  = kNoProcess;

  nsAutoString out;

  while (GetNextProcess(&p) == noErr)
  {
    if (p.lowLongOfPSN == PSN.lowLongOfPSN) continue;

    OSStatus err;

    info.processInfoLength = sizeof(ProcessInfoRec);

    CFStringRef n = NULL;

    err = CopyProcessName(&p, &n);

    if (err == noErr) 
    {
      if (out.Length()) out.Append(NS_LITERAL_STRING(","));

      nsAutoTArray<UniChar, 1024> buffer;

      CFIndex size = ::CFStringGetLength(n);
      buffer.SetLength(size);

      // if (buffer.EnsureElemCapacity(size))
      // {

        CFRange range = ::CFRangeMake(0, size);
        ::CFStringGetCharacters(n, range, buffer.Elements());
        // buffer.get()[size] = 0;

        nsAutoString str;
        str.Assign(buffer.Elements(), size);

        int pos = 0;

        // if ((pos = str.Find("@", PR_FALSE, 0, -1)) > -1) 
        // {
          // str.Cut(pos, 1);
        // }

        if ((pos = str.RFind("@", PR_FALSE)) > -1) 
        {
          str.Cut(pos, 1);
        }

        out.Append(str);

        // printf("PROCESS NAME (%s)\n", NS_ConvertUTF16toUTF8(str).get());
      // }

      // move here for issue 43417 to see if it's causing EXC_BAD_ACCESS crash?
      ::CFRelease(n);
    }

  }

  // printf("%s\n", NS_ConvertUTF16toUTF8(out).get());

   _retval = out;

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetMACAddress(nsAString & _retval)
{
  // printf("mozSecurebrowser::GetMACAddress\n"); 

  kern_return_t  kernResult = KERN_SUCCESS; // on PowerPC this is an int (4 bytes)
  /*
   *  error number layout as follows (see mach/error.h and IOKit/IOReturn.h):
   *
   *  hi                lo
   *  | system(6) | subsystem(12) | code(14) |
   */

  io_iterator_t  intfIterator;
  UInt8    MACAddress[ kIOEthernetAddressSize ];
 
  kernResult = FindEthernetInterfaces(&intfIterator);
    
  if (KERN_SUCCESS != kernResult)
  {
      printf("FindEthernetInterfaces returned 0x%08x\n", kernResult);
  }
    else 
  {
      kernResult = GetMACAddress(intfIterator, MACAddress);
      
      if (KERN_SUCCESS != kernResult)
      {
          printf("GetMACAddress returned 0x%08x\n", kernResult);
      }
  }
    
  (void) IOObjectRelease(intfIterator);  // Release the iterator.

  char out[256];

  sprintf(out, "%02x:%02x:%02x:%02x:%02x:%02x", MACAddress[0], MACAddress[1], MACAddress[2], MACAddress[3], MACAddress[4], MACAddress[5] );

  // printf("MAC ADDRESS: (%s)\n", out);

  _retval = NS_ConvertUTF8toUTF16(out).get();

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::Initialize()
{
  // printf("mozSecurebrowser::Initialize\n"); 

  if (!mSoundDevice) return NS_ERROR_FAILURE;

  if (SpeechBusy() != 0 && mCH) 
  {
    bool dispose = PR_TRUE;

    if (mPrefs) mPrefs->GetBoolPref("bmakiosk.speech.disposeSpeechChannel", &dispose);

    // printf("Bookmark2 -- dispose(%d)\n", dispose);

    if (dispose) DisposeSpeechChannel(mCH);

    mCH = NULL;
  }

  OSStatus err;

  if (!mCH) 
  {
    err = NewSpeechChannel(0, &mCH);

    if (err != noErr) 
    {
      printf("ERROR (%d)\n", (int)err); 

      return NS_ERROR_NOT_INITIALIZED;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::Uninitialize()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozSecurebrowser::InitializeListener(const nsAString & aType)
{
  printf("mozSecurebrowser::Initialize Listener\n"); 

  if (!mSoundDevice) return NS_ERROR_FAILURE;

  if (!mCH) return NS_ERROR_FAILURE;

  if (mListenerInitialized) return NS_OK;

  mListenerType.Assign(aType);

  printf("-------- type(%s) --------\n", NS_ConvertUTF16toUTF8(aType).get());

  OSStatus err = -1;

  if (aType.Equals(NS_LITERAL_STRING("soWordCallBack")))
  {
    printf("-------- soWordCallBack TYPE MATCH --------\n");
    err = SetSpeechInfo(mCH, soWordCallBack, (SpeechWordProcPtr*)WordCallBackProc);
  }
    else if (aType.Equals(NS_LITERAL_STRING("soSyncCallBack")))
  {
    printf("-------- soSyncCallBack TYPE MATCH --------\n");
    err = SetSpeechInfo(mCH, soSyncCallBack, (SpeechSyncProcPtr*)SyncCallBackProc);
  }
/********
    else if (aType.Equals(NS_LITERAL_STRING("soSpeechDoneCallBack")))
  {
    printf("-------- soSpeechDoneCallBack TYPE MATCH --------\n");
    err = SetSpeechInfo(mCH, soTextDoneCallBack, (SpeechDoneProcPtr*)SpeechDoneCallBackProc);
  }

    else if (aType.Equals(NS_LITERAL_STRING("soPhonemeCallBack")))
  {
    printf("-------- soPhonemeCallBack TYPE MATCH --------\n");
    err = SetSpeechInfo(mCH, soPhonemeCallBack, (SpeechPhonemeProcPtr*)SpeechPhonemeProc);
  }
********/

  if (err != noErr) 
  {
    printf("InitializeListener:ERROR (%d)\n", (int)err); 
    return NS_ERROR_FAILURE;
  }
    else
  {
    mListenerInitialized = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::Play(const nsAString &text)
{
  // printf("mozSecurebrowser::Play\n"); 

  if (!mSoundDevice) return NS_ERROR_FAILURE;

  // call to ensure we have a sound card ...
  PRInt32 vol;
  GetSystemVolume(&vol);

  if (!mSoundDevice) return NS_ERROR_FAILURE;

  Stop();

  OSStatus err;

  if (!mCH)
  {
    err = NewSpeechChannel(&mVoiceSpec, &mCH);

    if (err != noErr) 
    {
      printf("Play::NewSpeechChannel:ERROR (%d)\n", (int)err); 
      return NS_ERROR_FAILURE;
    }

    if (mListenerInitialized)
    {
      mListenerInitialized = PR_FALSE;
      InitializeListener(mListenerType);
    }
  }

  // printf("::Play mLastVolume (%d)\n", mLastVolume);

  if (mLastVolume >= 0) SetVolume(mLastVolume);

  err = SpeakText(mCH, NS_ConvertUTF16toUTF8(text).get(), text.Length());

  if (err != noErr) 
  {
    printf("Play:ERROR (%d)\n", (int)err); 
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::Pause()
{
  // printf("mozSecurebrowser::Pause\n"); 

  if (!mSoundDevice) return NS_ERROR_FAILURE;

  if (!mCH || !mSoundDevice) return NS_OK;

  // this call fails on OSX 10.9
  // if (SpeechBusy() == 0) return NS_OK;

  // kImmediate, kEndOfWord, kEndOfSentence 

  nsAutoString name;

  GetVoiceName(name);

  if (name.Equals(NS_LITERAL_STRING("Bad News"))    ||
      name.Equals(NS_LITERAL_STRING("Good News"))   ||
      name.Equals(NS_LITERAL_STRING("Cellos"))      ||
      name.Equals(NS_LITERAL_STRING("Pipe Organ")))
    Stop();
  else
    PauseSpeechAt(mCH, kImmediate);

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::Stop()
{
  // printf("mozSecurebrowser::Stop\n"); 

  if (!mSoundDevice) return NS_ERROR_FAILURE;

  if (!mCH) return NS_OK;

  if (!mSoundDevice) 
  {
    mCH = NULL;

    // Initialize();
    // printf("STOP NO SOUND DEVICE - NOT REINITIALIZING - SPEECH CHANNEL IS NOW NULL - RETURN FAILURE\n");

    return NS_ERROR_FAILURE;
  }

  OSErr err = StopSpeech(mCH);

  if (err != noErr) printf("-------- STOP API FAILED --------\n");

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::Resume()
{
  // printf("mozSecurebrowser::Resume\n"); 

  if (!mSoundDevice) return NS_ERROR_FAILURE;

  if (!mCH || !mSoundDevice) return NS_OK;

  if (SpeechBusy()) return NS_OK;

  ContinueSpeech(mCH);

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetVoices(nsAString & _retval)
{
  // printf("mozSecurebrowser::GetVoices\n"); 

  if (!mSoundDevice) return NS_ERROR_FAILURE;

  if (!mCH) return NS_OK; 

  short voiceCount;

  nsAutoString out;

  if (CountVoices( &voiceCount ) == noErr)
  {
    VoiceDescription description;
    VoiceSpec voiceSpec;

    // printf("voiceCount (%d)\n", voiceCount);

    for (int i = 1; i <= voiceCount; ++i)
    {
      if ( (GetIndVoice( i, &voiceSpec ) == noErr)  && (GetVoiceDescription( &voiceSpec, &description, sizeof(description) ) == noErr ) )
      {
        CFStringRef str = CFStringCreateWithPascalString(kCFAllocatorDefault, description.name, kCFStringEncodingMacRoman);

        out.Append(NS_ConvertUTF8toUTF16(CFStringGetCStringPtr(str, kCFStringEncodingMacRoman)));

        if (i != voiceCount) out.Append(NS_LITERAL_STRING(","));
      }
    }
  }

  _retval = out;

  return NS_OK;

}

NS_IMETHODIMP
mozSecurebrowser::GetVoiceName(nsAString & _retval)
{
  // printf("mozSecurebrowser::GetVoiceName\n"); 

  if (!mSoundDevice) return NS_ERROR_FAILURE;

  if (!mCH) return NS_OK; 

  OSStatus err;

  VoiceDescription info;

  if (!mVoiceSpecInitialized) err = GetVoiceDescription(NULL, &info, sizeof(info));
  else err = GetVoiceDescription(&mVoiceSpec, &info, sizeof(info));

  // printf("info.name (%s)\n", info.name);

  // printf("info.name (%s), info.comment(%s), info.gender(%d), info.language(%d)\n", info.name, info.comment, info.gender, info.language);

  if (err != noErr) printf("GetVoice:ERROR (%d)\n", (int)err); 

  CFStringRef str = CFStringCreateWithPascalString(kCFAllocatorDefault, info.name, kCFStringEncodingMacRoman);

  _retval = NS_ConvertUTF8toUTF16(CFStringGetCStringPtr(str, kCFStringEncodingMacRoman));

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::SetVoiceName(const nsAString & aVoiceName)
{
  // printf("mozSecurebrowser::SetVoiceName (%s)\n", NS_ConvertUTF16toUTF8(aVoiceName).get()); 

  if (!mSoundDevice) return NS_ERROR_FAILURE;

  if (!mCH) return NS_OK; 

  OSStatus err = noErr;

  short voiceCount;

  nsAutoString out;

  bool dispose = PR_TRUE;

  if (mPrefs) mPrefs->GetBoolPref("bmakiosk.speech.disposeSpeechChannel", &dispose);

  if (CountVoices( &voiceCount ) == noErr)
  {
    for (int i = 1; i <= voiceCount; ++i)
    {
      VoiceDescription description;
      VoiceSpec voiceSpec;

      if ( (GetIndVoice( i, &voiceSpec ) == noErr)  && (GetVoiceDescription( &voiceSpec, &description, sizeof(description) ) == noErr ) )
      {
        CFStringRef str = CFStringCreateWithPascalString(kCFAllocatorDefault, description.name, kCFStringEncodingMacRoman);

        out.Assign(NS_ConvertUTF8toUTF16(CFStringGetCStringPtr(str, kCFStringEncodingMacRoman)));

        if (out.Equals(aVoiceName)) 
        {
          // printf("VOICE MATCH FOUND! %s %p\n", str, (void*)&voiceSpec);

          // printf("Bookmark3 -- dispose(%d)\n", dispose);

          if (dispose) 
          {
            err = DisposeSpeechChannel(mCH);

            if (err != noErr) printf("SetVoiceName:ERROR disposing speech channel ... (%d)\n", (int)err); 
          }

          mCH = NULL;

          err = NewSpeechChannel(&voiceSpec, &mCH);

          if (err != noErr) printf("SetVoiceName:ERROR (%d)\n", (int)err); 

          if (mListenerInitialized) 
          {
            mListenerInitialized = PR_FALSE;
            InitializeListener(mListenerType);
          }

          mVoiceSpec = voiceSpec;
          mVoiceSpecInitialized = PR_TRUE;

          // printf("::SetVolume (%d)\n", mLastVolume);

          if (mLastVolume >= 0 && mLastVolume <= 10) SetVolume(mLastVolume);

          break;
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetKey(nsAString & _retval)
{
  // printf("mozSecurebrowser::GetKey\n");

  // XKTEEDmtrDznFw312vbxW4bhlaKYxVYL0Eo6pQo6PzbYy0Fl4PbDXOKd227KsL7
  _retval.Assign(NS_LITERAL_STRING("WEtURUVEbXRyRHpuRnczMTJ2YnhXNGJobGFLWXhWWUwwRW82cFFvNlB6Yll5MEZsNFBiRFhPS2QyMjdLc0w3"));

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetVolume(PRInt32 *_retval)
{
  // printf("mozSecurebrowser::GetVolume\n"); 

  if (!mSoundDevice) return NS_ERROR_FAILURE;

  NS_ENSURE_ARG(_retval);

  if (!mCH) return NS_OK; 

  OSStatus err;

  Fixed volume;
  short n = -1;

  err = GetSpeechInfo(mCH, soVolume, &volume);

  if (err != noErr) 
  {
    if (mLastVolume)
      *_retval = mLastVolume;
    else
      *_retval = 10;

    printf("GetVolume:ERROR (%d)\n", (int)err); 
    return NS_ERROR_FAILURE;
  }

  volume = FixMul( volume, 0x000A0000 );        // Multiply by 10.
  n = Fix2Long( volume );

  // printf("volume (%d)\n", n);

  if (mLastVolume == -1) mLastVolume = volume;

  // printf("mLastVolume (%d)\n", mLastVolume);

  *_retval = (PRInt32)n;

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::SetVolume(PRInt32 aVolume)
{
  if (!mSoundDevice) return NS_ERROR_FAILURE;

  if (!mCH) return NS_OK; 

  OSStatus err;

  if (aVolume < 0) aVolume = 0;
  if (aVolume > 10) aVolume = 10;

 //  printf("mozSecurebrowser::SetVolume (%d)\n", aVolume); 

  if (aVolume >=0 && aVolume <=10)
  {
    Fixed volume = FixRatio(aVolume, 10);

    err = SetSpeechInfo(mCH, soVolume, &volume);

    if (err != noErr) 
    {
      printf("SetVolume:ERROR (%d)\n", (int)err); 

      bool dispose = PR_TRUE;

      if (mPrefs) mPrefs->GetBoolPref("bmakiosk.speech.disposeSpeechChannel", &dispose);

      // printf("Bookmark4 -- dispose (%d)\n", dispose);

      if (dispose) DisposeSpeechChannel(mCH);

      mCH = NULL;

      return NS_ERROR_FAILURE;
    }
  }

  mLastVolume = aVolume;

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetStatus(nsAString & _retval)
{
  // printf("mozSecurebrowser::GetStatus\n"); 

  if (!mSoundDevice) return NS_ERROR_FAILURE;

  _retval.Assign(NS_LITERAL_STRING(""));

  if (!mCH || !mSoundDevice) 
  {
    _retval.Assign(NS_LITERAL_STRING("Stopped"));
    return NS_OK; 
  }

  OSStatus err;

  SpeechStatusInfo info;

  err = GetSpeechInfo(mCH, soStatus, &info);

  if (err != noErr) printf("GetStatus:ERROR (%d)\n", (int)err); 

  if (info.outputPaused) 
    _retval.Assign(NS_LITERAL_STRING("Paused"));
  else if (info.outputBusy) 
    _retval.Assign(NS_LITERAL_STRING("Playing"));
  else if (!info.outputBusy && ! info.outputPaused) 
    _retval.Assign(NS_LITERAL_STRING("Stopped"));

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetRate(PRInt32 *aRate)
{
  // printf("mozSecurebrowser::GetRate\n");

  if (!mSoundDevice) return NS_ERROR_FAILURE;

  if (!mCH) return NS_OK; 

  Fixed rate = 0;

  OSStatus err = GetSpeechRate(mCH, &rate);

  if (err != noErr) 
  {
    printf("ERROR: getting speech rate ...\n");

    return NS_ERROR_FAILURE;
  }

  // min 87 max 350
  float val = floor(Fix2X(rate));
  float new_val = ((val - 87) / (263)) * 20;

  *aRate = (int)ceil(new_val);

  // just in case
  if (*aRate > 20) *aRate = 20;
  if (*aRate < 0) *aRate = 0;

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::SetRate(PRInt32 aRate)
{
  // printf("mozSecurebrowser::SetRate\n");

  if (!mSoundDevice) return NS_ERROR_FAILURE;

  if (!mCH) return NS_OK; 

  // 40 appears to be the minimal
  if (aRate < 0) aRate = 0;

  // lets make 300 the max
  if (aRate > 20) aRate = 20;

  float val = (float)aRate;

  // old range 0 - 20
  // ( (old_val - old_min) / (old_max - old_min) ) * (new_max - new_min) + new_min
  // 350 - 87 = 263
  float new_val = (val/20) * 263 + 87;

  Fixed rate = X2Fix(new_val);
  OSStatus err = SetSpeechRate(mCH, rate);

  if (err != noErr) 
  {
    printf("ERROR: setting speech rate ...\n");

    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetPitch(PRInt32 *aPitch)
{
  // printf("mozSecurebrowser::GetPitch\n");

  if (!mSoundDevice) return NS_ERROR_FAILURE;

  if (!mCH) return NS_OK; 

  if (mLastPitch >= 0) 
  {
    *aPitch = mLastPitch;
    return NS_OK;
  }

  Fixed pitch = 0;

  OSStatus err = GetSpeechPitch(mCH, &pitch);

  float val = floor(Fix2X(pitch));

  if (err != noErr) 
  {
    printf("ERROR: getting speech pitch ...\n");

    return NS_ERROR_FAILURE;
  }

  // pitch values in the ranges of 30.000 to 40.000 and 55.000 to 65.000, respectively

  // old range 30 - 65
  // ( (old_val - old_min) / (old_max - old_min) ) * (new_max - new_min) + new_min

  float new_val = ( (val - 30) / 35) * 20;

  // 55, 30
  *aPitch = (int)ceil(new_val);

  if (*aPitch > 20) *aPitch = 20;
  if (*aPitch < 0) *aPitch = 0;

  mLastPitch = *aPitch;

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::SetPitch(PRInt32 aPitch)
{
  if (!mSoundDevice) return NS_ERROR_FAILURE;

  if (aPitch < 0) aPitch = 0;
  if (aPitch > 20) aPitch = 20;

  if (!mCH) return NS_OK; 

  // old range 0 - 20, back to orig range 30 - 65
  // ( (old_val - old_min) / (old_max - old_min) ) * (new_max - new_min) + new_min

  float val = (float)aPitch;
  float new_val = (val/20) * 35 + 30;

  Fixed pitch = X2Fix(new_val);

  OSStatus err = SetSpeechPitch(mCH, pitch);

  if (err != noErr) 
  {
    printf("ERROR: setting speech pitch ...\n");

    return NS_ERROR_FAILURE;
  }

  // reset cached pitch
  mLastPitch = -1;

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetSystemVolume(PRInt32 *aVolume)
{
  // printf("mozSecurebrowser::GetSystemVolume\n");

  if (!mSoundDevice) return NS_ERROR_FAILURE;

  float           b_vol;
  OSStatus        err;
  AudioDeviceID   device;
  UInt32          size;
  UInt32          channels[2];
  float           volume[2];
  
  *aVolume = 0;

  // get device
  size = sizeof device;
  err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &size, &device);

  if (err != noErr) 
  {
    printf("ERROR: audio-volume get device\n");
    *aVolume = 0;

    return NS_ERROR_FAILURE;
  }
  
  b_vol = 0;

  // try set master volume (channel 0)
  size = sizeof b_vol;
  err = AudioDeviceGetProperty(device, 0, 0, kAudioDevicePropertyVolumeScalar, &size, &b_vol);  //kAudioDevicePropertyVolumeScalarToDecibels
  if (noErr == err) 
  {
    // printf("******** b_vol (%f) ********\n", b_vol);
    float f = b_vol * 10;
    *aVolume = (int)round(f);

    // printf("******** out vol (%d) ********\n", *aVolume);

    return NS_OK;
  }
  
  // otherwise, try seperate channels
  // get channel numbers
  size = sizeof(channels);
  err = AudioDeviceGetProperty(device, 0, 0,kAudioDevicePropertyPreferredChannelsForStereo, &size,&channels);

  if (err != noErr) 
  {
    printf("ERROR getting channel-numbers\n");
    mSoundDevice = PR_FALSE;
    return NS_ERROR_FAILURE;
  }
  
  size = sizeof(float);

  volume[0] = 0.0;
  volume[1] = 0.0;

  err = AudioDeviceGetProperty(device, channels[0], 0, kAudioDevicePropertyVolumeScalar, &size, &volume[0]);

  if (noErr != err) 
  {
    printf("ERROR: getting volume of channel %d\n", (int)channels[0]);
    return NS_ERROR_FAILURE;
  }

  err = AudioDeviceGetProperty(device, channels[1], 0, kAudioDevicePropertyVolumeScalar, &size, &volume[1]);

  if (noErr != err) 
  {
    printf("ERROR: getting volume of channel %d\n", (int)channels[1]);
    return NS_ERROR_FAILURE;
  }

  // printf("******** V1(%f) V2(%f) ********\n", volume[0], volume[1]);
  
  b_vol = (volume[0]+volume[1]);

  // printf("********  b_vol(%f) ********\n",  b_vol);

  float f = b_vol * 10;

  // printf("********  f(%f) ********\n",  f);
  
  *aVolume =  (int)round(f)/2;

  // printf("********  *aVolume(%d) ********\n",  *aVolume);
  
  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::SetSystemVolume(PRInt32 aVolume)
{
  // printf("mozSecurebrowser::SetSystemVolume (%d)\n", aVolume);

  if (!mSoundDevice) return NS_ERROR_FAILURE;

  OSStatus        err;
  AudioDeviceID   device;
  UInt32          size;
  Boolean         canset  = false;
  UInt32          channels[2];
  float           volume = (float)aVolume/10;

  UInt32 mute = 0;

  // printf("******** converted inVolume (%f) ********\n", volume);
    
  // get default device
  size = sizeof device;
  err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &size, &device);

  if (err != noErr) 
  {
    printf("ERROR: audio-volume get device");

    return NS_ERROR_FAILURE;
  }
  
  // try set master-channel (0) volume
  size = sizeof canset;
  err = AudioDeviceGetPropertyInfo(device, 0, false, kAudioDevicePropertyVolumeScalar, &size, &canset);

  if (err == noErr && canset == true) 
  {
    // printf("******** Setting Volume to (%f) ********\n", volume);

    size = sizeof volume;

    // unmute
    err = AudioDeviceSetProperty(device, NULL, 0, false, kAudioDevicePropertyMute, sizeof(UInt32), &mute);
    if (noErr!=err) printf("ERROR: Setting Master MUTE\n");

    err = AudioDeviceSetProperty(device, NULL, 0, false, kAudioDevicePropertyVolumeScalar, size, &volume);

    return NS_OK;
  }

  // else, try seperate channes
  // get channels
  size = sizeof(channels);
  err = AudioDeviceGetProperty(device, 0, false, kAudioDevicePropertyPreferredChannelsForStereo, &size, &channels);

  if (err != noErr) 
  {
    printf("ERROR: getting channel-numbers\n");

    return NS_ERROR_FAILURE;
  }
  
  // set volume
  size = sizeof(float);

  // unmute
  err = AudioDeviceSetProperty(device, 0, channels[0], false, kAudioDevicePropertyMute, sizeof(UInt32), &mute);
  if (noErr!=err) printf("ERROR: Setting Left MUTE\n");

  err = AudioDeviceSetProperty(device, 0, channels[0], false, kAudioDevicePropertyVolumeScalar, size, &volume);

  if (noErr != err) 
  {
    printf("ERROR: setting volume of channel %d\n", (int)channels[0]);

    return NS_ERROR_FAILURE;
  }

  // unmute
  err = AudioDeviceSetProperty(device, 0, channels[1], false, kAudioDevicePropertyMute, sizeof(UInt32), &mute);
  if (noErr!=err) printf("ERROR: Setting Right MUTE\n");

  err = AudioDeviceSetProperty(device, 0, channels[1], false, kAudioDevicePropertyVolumeScalar, size, &volume);

  if (noErr!=err)
  {
    printf("ERROR: setting volume of channel %d\n", (int)channels[1]);

    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetSystemMute(bool *aSystemMute)
{
  OSStatus        err;
  AudioDeviceID   device;
  UInt32          size;
  UInt32          channels[2];

  UInt32          mute = 0;

  // get default device
  size = sizeof device;
  err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &size, &device);

  if (err != noErr) 
  {
    printf("ERROR: set system mute get device");

    return NS_ERROR_FAILURE;
  }

  // else, try seperate channes
  // get channels
  size = sizeof(channels);
  err = AudioDeviceGetProperty(device, 0, false, kAudioDevicePropertyPreferredChannelsForStereo, &size, &channels);

  if (err != noErr) 
  {
    printf("ERROR: getting channel-numbers\n");

    return NS_ERROR_FAILURE;
  }
  
  size = sizeof(UInt32);
  err = AudioDeviceGetProperty(device, channels[1], 0, kAudioDevicePropertyMute, &size, &mute);

  if (err != noErr) 
  {
    printf("ERROR: getting system mute\n");

    return NS_ERROR_FAILURE;
  }
  
  // printf("-------- MUTE (%ld) --------\n", mute);

  *aSystemMute = (mute == 0);

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::SetSystemMute(bool aSystemMute)
{

  OSStatus        err;
  AudioDeviceID   device;
  UInt32          size;
  UInt32          channels[2];

  UInt32 mute = aSystemMute ? 0 : 1;

  // get default device
  size = sizeof device;
  err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &size, &device);

  if (err != noErr) 
  {
    printf("ERROR: set system mute get device");

    return NS_ERROR_FAILURE;
  }

  // else, try seperate channes
  // get channels
  size = sizeof(channels);
  err = AudioDeviceGetProperty(device, 0, false, kAudioDevicePropertyPreferredChannelsForStereo, &size, &channels);

  if (err != noErr) 
  {
    printf("ERROR: getting channel-numbers\n");

    return NS_ERROR_FAILURE;
  }
  
  err = AudioDeviceSetProperty(device, 0, channels[0], false, kAudioDevicePropertyMute, sizeof(UInt32), &mute);
  if (noErr != err) printf("ERROR: Setting Left MUTE\n");

  err = AudioDeviceSetProperty(device, 0, channels[1], false, kAudioDevicePropertyMute, sizeof(UInt32), &mute);
  if (noErr != err) printf("ERROR: Setting Right MUTE\n");

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetPermissive(bool *aPermissive)
{
  *aPermissive = mPermissive;

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::SetPermissive(bool aPermissive)
{
  return SetUIMode(aPermissive);
}

nsresult
mozSecurebrowser::SetUIMode(bool aPermissive)
{
  OSStatus status = noErr;

  if (!aPermissive)
  {
    printf("-------- SETTING TO NOT PERMISSIVE --------\n");
    status = SetSystemUIMode(kUIModeAllHidden, kUIOptionDisableProcessSwitch |
                             kUIOptionDisableForceQuit                       |
                             kUIOptionDisableSessionTerminate                |
                             kUIOptionDisableAppleMenu);
    if (status != noErr) 
    {
      printf("-------- ERROR --------\n");
      return NS_ERROR_FAILURE;
    }

    // reset back to default
    mPrefs->SetBoolPref("bmakiosk.system.enableKillProcess", mKillProcess);
    mPrefs->SetBoolPref("bmakiosk.system.shutdownOnNewProcess", mShutDown);
  }
    else
  {
    printf("-------- SETTING TO PERMISSIVE --------\n");
    status = SetSystemUIMode(kUIModeContentHidden, kUIOptionDisableAppleMenu|kUIOptionDisableProcessSwitch|kUIOptionDisableForceQuit);

    ProcessSerialNumber np, p;

    GetCurrentProcess(&p);

    CFStringRef n = NULL;
    CFStringRef fn = NULL;

    CopyProcessName(&p, &n);

    // printf("CurrentProcess (%s)\n", CFStringGetCStringPtr(n, kCFStringEncodingMacRoman));

    GetFrontProcess(&np);
    np.lowLongOfPSN  = kNoProcess;

    // set the next process to the front 
    while (GetNextProcess(&np) == noErr) 
    {
      CopyProcessName(&np, &fn);

      CFStringRef s = CFStringCreateWithCString(NULL, "Dock", kCFStringEncodingMacRoman);

      if (CFStringCompare(s, fn, 0) == kCFCompareEqualTo) 
      {
        printf("Show (%s)\n", CFStringGetCStringPtr(fn, kCFStringEncodingMacRoman));
        ShowHideProcess(&np, true);
        SetFrontProcess(&np);
        break;
      }
    }

    if (status != noErr) 
    {
      printf("-------- ERROR --------\n");
      return NS_ERROR_FAILURE;
    }

    mPrefs->SetBoolPref("bmakiosk.system.enableKillProcess", PR_FALSE);
    mPrefs->SetBoolPref("bmakiosk.system.shutdownOnNewProcess", PR_FALSE);
  }

  mPermissive = aPermissive;
  mPrefs->SetBoolPref("bmakiosk.mode.permissive", mPermissive);

  return NS_OK;
}


NS_IMETHODIMP
mozSecurebrowser::GetSoxVersion(nsAString& aVersion)
{
  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetClearTypeEnabled(bool *aClearTypeEnabled)
{
  return NS_OK;
}

void
mozSecurebrowser::PrintPointer(const char* aName, nsISupports* aPointer)
{
  printf ("%s (%p)\n", aName, (void*)aPointer);
}

