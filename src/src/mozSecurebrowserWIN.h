#ifndef _mozSECUREBROWSER_H_
#define _mozSECUREBROWSER_H_

#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <tchar.h>
#include <psapi.h>
#include <Iphlpapi.h>

#include <atlbase.h>
#include <sapi.h>
#include <sphelper.h>


#include "mozISecurebrowser.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

class mozSecurebrowser : public mozISecurebrowser
{
                                                                                                    
public:
                                                                                                    
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISECUREBROWSER

  mozSecurebrowser();
  virtual ~mozSecurebrowser();

  nsresult Init();
  nsresult ReInit();
  nsresult ResetCallBack();

  void GetStatus(SPVOICESTATUS *aEventStatus);
  void GetVoice(ISpVoice **aVoice);

  PRBool IsVistaOrGreater();

  // DEBUG
  void PrintPointer(const char* aName, nsISupports* aPointer);

private:

  PRInt32 mNum;

  ISpVoice *mVoice;

  PRBool mInitialized, mListenerInited, mCallBackInited;

  bool mPermissive;

  CComPtr<ISpObjectToken> mLastVoice;

  nsAutoString mStatus, mListenerString;

  PRInt32 mLastVolume, mPitch, mRate;

  BOOL GetLogonSID (HANDLE hToken, PSID *ppsid);

  nsCOMPtr<nsIPrefBranch> mPrefs;

};
                                                                                                    
#endif

