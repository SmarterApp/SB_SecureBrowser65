#ifndef _mozSECUREBROWSER_H_
#define _mozSECUREBROWSER_H_

#include "mozISecurebrowser.h"

#include "nsCOMPtr.h"
#include "nsLiteralString.h"
#include "nsString.h"

class mozSecurebrowser : public mozISecurebrowser
{
                                                                                                    
public:
                                                                                                    
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISECUREBROWSER

  mozSecurebrowser();
  virtual ~mozSecurebrowser();

  void GetShellScript(nsAString & aName, FILE **aFp);
  void GetShellScript(nsAString & aName, nsAString & params, FILE **aFp);
  PRBool ShellScriptExists(nsAString & aName);
  int GetCards();

  nsresult Init();
  nsresult GetShellScriptOutputAsAString(nsAString & aName, nsAString &_retval);
  nsresult SynthesizeToSpeech(const nsAString &aText, const nsAString &aPath);

  nsresult GetSystemVolumeInternal(PRInt32 aCardNumber, PRInt32 *aVolume);
  nsresult SetSystemVolumeInternal(PRInt32 aCardNumber, PRInt32 aVolume);
  nsresult SetSystemHeadphoneVolumeInternal(PRInt32 aCardNumber, PRInt32 aVolume);

  nsresult PlayInternal(const nsAString & aPath);

  // DEBUG
  void PrintPointer(const char* aName, nsISupports* aPointer);

private:

  PRInt32 mNum, mVol, mLastMuteVol, mPitchInt;
  PRBool mFestInit, mSoxKludge, mSystemMute;

  char mRateChar[24];

  nsAutoString mStatus, mRate, mPitch, mSoxVersion, mPlayCmd;

  nsCOMPtr<nsIFile> mWavFile;
};
                                                                                                    
#endif

