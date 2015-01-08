#ifndef _mozSECUREBROWSER_H_
#define _mozSECUREBROWSER_H_

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetController.h>

#include <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>
#include <CoreAudio/CoreAudio.h>

#include "mozISecurebrowser.h"

#include "nsCOMPtr.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "nsIDOMClassInfo.h"

class mozSecurebrowser : public mozISecurebrowser
{
                                                                                                    
public:
                                                                                                    
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISECUREBROWSER

  mozSecurebrowser();
  virtual ~mozSecurebrowser();

  nsresult Init();

  kern_return_t FindEthernetInterfaces(io_iterator_t *matchingServices);
  kern_return_t GetMACAddress(io_iterator_t intfIterator, UInt8 *MACAddress);

  nsresult SetUIMode(bool aPermissive);

  // DEBUG
  void PrintPointer(const char* aName, nsISupports* aPointer);

private:

  SpeechChannel mCH;
  nsCOMPtr<nsIPrefBranch> mPrefs; 
  PRBool mSoundDevice;
  PRBool mVoiceSpecInitialized;
  PRBool mListenerInitialized;
  bool mPermissive;
  bool mKillProcess, mShutDown;

  nsAutoString mListenerType;

  PRInt32 mNum, mLastVolume, mLastPitch;
  VoiceSpec mVoiceSpec;
};
                                                                                                    
#endif

