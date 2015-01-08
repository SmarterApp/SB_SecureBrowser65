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

#include "mozSecurebrowserLinux.h"

#include <stdio.h>
#include <festival.h>
#include <EST_String.h>

#include "nsILocalFile.h"
#include "nsIDOMClassInfo.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"

#define MAX_PATH 256

static PRBool gFestivalWasInitialized = PR_FALSE;

class SBRunnable : public nsRunnable
{
  public:
    SBRunnable(const nsAString & aMsg, const nsAString & aPath, mozSecurebrowser* aSB) :
      mMsg(aMsg),
      mPath(aPath),
      mSB(aSB)
    {
      MOZ_ASSERT(!NS_IsMainThread()); // This should be running on the worker thread

      // printf("-------- RUN ON WORKER THREAD --------\n");

    }

    NS_IMETHOD Run()
    {
      MOZ_ASSERT(NS_IsMainThread()); // This method is supposed to run on the main thread!

      // printf("-------- RUN ON MAIN THREAD --------\n");

      // Convert to a waveform
      EST_Wave wave;

      // printf("-------- Length(%d) FESTIVAL_HEAP_SIZE(%d) --------\n", mMsg.Length(), FESTIVAL_HEAP_SIZE);

      // if (mMsg.Length() >= 1000) mMsg.SetLength(1000);

      // printf("%s\n", NS_ConvertUTF16toUTF8(mMsg).get());

      const EST_String text = NS_ConvertUTF16toUTF8(mMsg).get();

      // printf("-------- START festival_text_to_wave --------\n");
      festival_text_to_wave(text, wave);
      // printf("-------- END festival_text_to_wave --------\n");

      // printf("-------- START wave.save --------\n");
      wave.save(NS_ConvertUTF16toUTF8(mPath).get(),"riff");
      // printf("-------- END wave.save --------\n");

      mSB->PlayInternal(mPath);

      return NS_OK;
    }

  private:
    nsAutoString mMsg, mPath;
    nsRefPtr<mozSecurebrowser> mSB;
};


// constructor
mozSecurebrowser::mozSecurebrowser() 
  : mVol(10),
    mLastMuteVol(0),
    mPitchInt(0),
    mFestInit(gFestivalWasInitialized),
    mSystemMute(PR_FALSE),
    mRate(NS_LITERAL_STRING("13")),
    mPitch(NS_LITERAL_STRING("10"))
{
  printf ("CREATE: mozSecurebrowser (%p)\n", (void*)this);

  mStatus.Assign(NS_LITERAL_STRING("Stopped"));

  strcpy(mRateChar, "1");

  FILE *fp = NULL;
  int status;
  char path[MAX_PATH];

  fp = popen("play --version", "r");

  if (fp == NULL) printf("FAILED TO FIND SOX PLAY\n");

  if (fp)
  {
    nsAutoString out;

    while (fgets(path, PATH_MAX, fp) != NULL)
    {
      // printf("%s", path);

      out.Assign(NS_ConvertUTF8toUTF16(path).get());

      out.StripChars("\r\n");

      break;
    }

    mSoxVersion.Assign(out);

    mSoxKludge = mSoxVersion.EqualsLiteral("play (sox) 3.0");

    status = pclose(fp);

    if (status == -1) printf("FAILED TO GET SOX PLAY VERSION\n");
  }

  int retval = system("amixer get Master | grep off > /dev/null 2>&1");

  if (retval == 0) mSystemMute = PR_TRUE;

  // printf("-------- SYSTEM MUTE (%d) --------\n", mSystemMute);
}

void
mozSecurebrowser::GetShellScript(nsAString & aName, FILE **aFp)
{
  nsAutoString paramList(NS_LITERAL_STRING(" "));

  return GetShellScript(aName, paramList, aFp);
}


void
mozSecurebrowser::GetShellScript(nsAString & aName, nsAString & paramList, FILE **aFp)
{
  nsAutoString shellPath(aName);

  if (ShellScriptExists(shellPath))
  {
  // append the arguments when invoking the shell script
  shellPath.Append(NS_LITERAL_STRING(" "));
  shellPath.Append(paramList);

  printf("-------- Executing Override [%s] --------\n", NS_ConvertUTF16toUTF8(shellPath).get());

    *aFp = popen(NS_ConvertUTF16toUTF8(shellPath).get(), "r");

    return;
  }

  *aFp = NULL;

  return;
}

nsresult
mozSecurebrowser::GetShellScriptOutputAsAString(nsAString & aName, nsAString &_retval)
{
  nsresult rv = NS_OK;

  nsAutoString out;

  FILE *fp = NULL;
  int status;
  char str[MAX_PATH];

  // printf("-------- GetShellScriptOutputAsAString:aName [%s] --------\n", NS_ConvertUTF16toUTF8(aName).get());

  fp = popen(NS_ConvertUTF16toUTF8(aName).get(), "r");

  if (!fp) return NS_ERROR_FAILURE;
  
  while (fgets(str, PATH_MAX, fp) != NULL)
  {
    if (out.Length()) out.Append(NS_LITERAL_STRING(","));

    // printf("str: %s", str);

    out.Append(NS_ConvertUTF8toUTF16(str).get());

    out.StripWhitespace();
  }

  status = pclose(fp);

  if (status == -1) return NS_ERROR_FAILURE;
 
  _retval.Assign(out);

  printf("-------- GetShellScriptOutputAsAString OUT [%s] --------\n", NS_ConvertUTF16toUTF8(out).get());

  return rv;
}

PRBool
mozSecurebrowser::ShellScriptExists(nsAString & aName)
{
  nsresult rv;

  nsCOMPtr<nsIProperties> dirService(do_GetService("@mozilla.org/file/directory_service;1", &rv));
  if (NS_FAILED(rv)) return PR_FALSE;

  nsCOMPtr<nsIFile> shellScriptBin;
  rv = dirService->Get("CurProcD", NS_GET_IID(nsIFile), getter_AddRefs(shellScriptBin));
  if (NS_FAILED(rv)) return PR_FALSE;

  shellScriptBin->Append(NS_LITERAL_STRING("sb_securebrowser_bin"));
  shellScriptBin->Append(aName);

  bool exists;
  shellScriptBin->Exists(&exists);

  if (exists) 
  {
    nsAutoString shellPath;
    shellScriptBin->GetPath(shellPath);

    //printf("-------- EXISTS [%s] --------\n", NS_ConvertUTF16toUTF8(shellPath).get());

    aName.Assign(shellPath);
  }

  return exists;
}

int
mozSecurebrowser::GetCards()
{
  FILE *fp = NULL;
  int status;
  char str[MAX_PATH];

  nsAutoString s;
  s.AssignLiteral("arecord --list-devices | grep card | wc -l");

  fp = popen(NS_ConvertUTF16toUTF8(s).get(), "r");

  if (!fp) return NS_ERROR_FAILURE;
  
  nsAutoString out;
  while (fgets(str, PATH_MAX, fp) != NULL)
  {
    out.Append(NS_ConvertUTF8toUTF16(str).get());
    out.StripWhitespace();
  }

  status = pclose(fp);

  if (status == -1) return NS_ERROR_FAILURE;
 
  PRInt32 error;
  int len = out.ToInteger(&error, 10);

  // printf("******** Number of Cards [%d] ********\n", len);

  return len;
}

nsresult
mozSecurebrowser::Init()
{
  return NS_OK;
}

// destructor
mozSecurebrowser::~mozSecurebrowser()
{
  printf ("DESTROY: mozSecurebrowser (%p)\n", (void*)this);

  Stop();

  if (mWavFile) mWavFile->Remove(PR_FALSE);
}

// QueryInterface implementation for mozSecurebrowser
NS_INTERFACE_MAP_BEGIN(mozSecurebrowser)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, mozISecurebrowser)
    NS_INTERFACE_MAP_ENTRY(mozISecurebrowser)
    NS_INTERFACE_MAP_ENTRY(nsISupports)
    NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Runtime)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(mozSecurebrowser)
NS_IMPL_RELEASE(mozSecurebrowser)

NS_IMETHODIMP
mozSecurebrowser::GetVersion(nsAString& aVersion)
{
  aVersion = NS_LITERAL_STRING(MOZ_APP_VERSION);

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetRunningProcessList(nsAString & _retval)
{
  // printf("mozSecurebrowser::GetRunningProcessList\n"); 

  FILE *fp = NULL;
  int status;
  char path[MAX_PATH];

  nsAutoString name(NS_LITERAL_STRING(".GetRunningProcessList.sh"));  

  GetShellScript(name, &fp);

  if (!fp)
  {
    // ps -eo comm
    // fp = popen("ps -eo comm", "r");
    fp = popen("ps -u `whoami` -o comm", "r");
  }

  if (fp == NULL) return NS_ERROR_FAILURE;

  nsAutoString out;

  while (fgets(path, PATH_MAX, fp) != NULL)
  {
    if (out.Length()) out.Append(NS_LITERAL_STRING(","));

    // printf("%s", path);

    out.Append(NS_ConvertUTF8toUTF16(path).get());

    out.StripWhitespace();
  }

  status = pclose(fp);

  if (status == -1) return NS_ERROR_FAILURE;
 
  _retval = out;

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetMACAddress(nsAString & _retval)
{
  // printf("mozSecurebrowser::GetMACAddress\n"); 

  // ifconfig -a | grep  HWaddr | sed -e's:^.*HWaddr\s::g'

  FILE *fp = NULL;
  int status;
  char path[PATH_MAX];

  nsAutoString name(NS_LITERAL_STRING(".GetMACAddress.sh"));

  GetShellScript(name, &fp);

  if (!fp) fp = popen("ifconfig -a | grep  HWaddr | sed -e's:^.*HWaddr\\s::g'", "r");

  if (fp == NULL) return NS_ERROR_FAILURE;

  nsAutoString out;

  while (fgets(path, PATH_MAX, fp) != NULL)
  {
    if (out.Length()) out.Append(NS_LITERAL_STRING(","));

    // printf("%s", path);

    out.Append(NS_ConvertUTF8toUTF16(path).get());

    out.StripWhitespace();
  }

  status = pclose(fp);

  if (status == -1) return NS_ERROR_FAILURE;
 
  _retval = out;

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::Initialize()
{
  // printf("mozSecurebrowser::Initialize gFestivalWasInitialized (%d)\n", gFestivalWasInitialized); 

  if (gFestivalWasInitialized) return NS_OK;

  nsresult rv;
  nsCOMPtr<nsILocalFile> file(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));

  file->InitWithPath(NS_LITERAL_STRING("/usr/share/festival/init.scm"));

  bool exists = PR_FALSE;

  file->Exists(&exists);

  festival_libdir = "/usr/share/festival";

  if (!exists) 
  {
    file->InitWithPath(NS_LITERAL_STRING("/usr/share/festival/lib/init.scm"));
    festival_libdir = "/usr/share/festival/lib";
  }

  file->Exists(&exists);

  if (!exists) 
  {
    mFestInit = PR_FALSE;
    return NS_ERROR_NOT_INITIALIZED;
  }

  // the heap size can be increased here --pete
  // festival_initialize(1, 210000);
  // festival_initialize(1, 10000000);
  festival_initialize(1, FESTIVAL_HEAP_SIZE);
  gFestivalWasInitialized = PR_TRUE;

  mFestInit = PR_TRUE;

  // Check if an override is available that needs to be invoked 
  nsAutoString shellPath(NS_LITERAL_STRING(".Initialize.sh"));
  // shellPath will change from leafName to full Linux path if it exists
  if (ShellScriptExists(shellPath)) system(NS_ConvertUTF16toUTF8(shellPath).get());


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
  //printf("mozSecurebrowser::Initialize Listener\n"); 

  return NS_OK;
}

nsresult
mozSecurebrowser::SynthesizeToSpeech(const nsAString &aText, const nsAString &aPath)
{
  // printf("mozSecurebrowser::SynthesizeToSpeech\n"); 

  nsAutoString shellPath(NS_LITERAL_STRING(".SynthesizeToSpeech.sh"));  

  // shellPath will change from leafName to full Linux path if it exists
  if (ShellScriptExists(shellPath)) 
  {
    int retval = system(NS_ConvertUTF16toUTF8(shellPath).get());

    if (retval != 0) 
    {
      printf("-------- SynthesizeToSpeech SHELL SCRIPT ERROR --------\n");
      return NS_ERROR_FAILURE;
    }
  }

  nsCOMPtr<nsIRunnable> resultrunnable = new SBRunnable(aText, aPath, this);
  NS_DispatchToMainThread(resultrunnable);

  /********
  // Convert to a waveform
  EST_Wave wave;

  festival_text_to_wave(NS_ConvertUTF16toUTF8(aText).get(), wave);
  wave.save(NS_ConvertUTF16toUTF8(aPath).get(),"riff");
  ********/

  mStatus.Assign(NS_LITERAL_STRING("Buffering"));

  return NS_OK;
}


NS_IMETHODIMP
mozSecurebrowser::Play(const nsAString &text)
{
  // printf("mozSecurebrowser::Play mFestInit(%d)\n", mFestInit); 

  if (!mFestInit) return NS_ERROR_NOT_INITIALIZED;

  if (text.IsEmpty()) return NS_OK;

  Stop();

  mStatus.Assign(NS_LITERAL_STRING("Buffering"));

  nsAutoString str1(text);

  int index;
  while ((index = str1.Find("'")) != -1)
    str1.Replace(index, 1, NS_LITERAL_STRING(""));

  while ((index = str1.Find("\n")) != -1)
    str1.Replace(index, 1, NS_LITERAL_STRING(" "));

  while ((index = str1.Find("\"")) != -1)
    str1.Replace(index, 1, NS_LITERAL_STRING(""));

  /********
  while ((index = str1.Find("-")) != -1)
    str1.Replace(index, 1, NS_LITERAL_STRING(" "));

  while ((index = str1.Find("(")) != -1)
    str1.Replace(index, 1, NS_LITERAL_STRING(""));

  while ((index = str1.Find(")")) != -1)
    str1.Replace(index, 1, NS_LITERAL_STRING(""));

  while ((index = str1.Find(":")) != -1)
    str1.Replace(index, 1, NS_LITERAL_STRING(""));

  while ((index = str1.Find("For at ")) != -1)
    str1.Replace((index+5), 1, NS_LITERAL_STRING("'t"));
  ********/

  // printf("%s\n", NS_ConvertUTF16toUTF8(str1).get());

  festival_eval_command("(audio_mode 'async)");
  // festival_eval_command(NS_ConvertUTF16toUTF8(NS_LITERAL_STRING("(SayText \"") + str1 + NS_LITERAL_STRING("\")")).get());

  nsAutoString s;

  s.Append(NS_LITERAL_STRING("play -v "));

  if (mVol != 10) 
  {
    s.Append(NS_LITERAL_STRING("."));
    s.AppendInt(mVol);
  }
    else
  {
    s.Append(NS_LITERAL_STRING("1"));
  }

  nsresult rv;

  nsCOMPtr<nsIProperties> dirService(do_GetService("@mozilla.org/file/directory_service;1", &rv));

  if (NS_FAILED(rv)) return rv;

  nsAutoString path;

  if (!mWavFile)
  {
    rv = dirService->Get("ProfD", NS_GET_IID(nsIFile), getter_AddRefs(mWavFile));
    if (NS_FAILED(rv)) return rv;

    rv = mWavFile->Append(NS_LITERAL_STRING(".d273c04c-9745-4bf8-845e-38797d6fa99f")); 
    if (NS_FAILED(rv)) return rv;
  }

  // always remove before we create a new one
  mWavFile->Remove(PR_FALSE);

  rv = mWavFile->GetPath(path);
  if (NS_FAILED(rv)) return rv;

  s.Append(NS_LITERAL_STRING(" "));
  s.Append(path);
  s.Append(NS_LITERAL_STRING(" stretch "));
  s.Append(NS_ConvertUTF8toUTF16(mRateChar));

  if (!mSoxKludge)
  {
    s.Append(NS_LITERAL_STRING(" pitch "));
    s.AppendInt(mPitchInt);
  }

  s.Append(NS_LITERAL_STRING(" > /dev/null 2>&1 "));
  s.Append(NS_LITERAL_STRING(" &"));
  
  //printf("-------- (%s) --------\n", NS_ConvertUTF16toUTF8(s).get());

  mPlayCmd.Assign(s);

  rv = SynthesizeToSpeech(str1, path);

  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

nsresult
mozSecurebrowser::PlayInternal(const nsAString & aPath)
{
  mStatus.Assign(NS_LITERAL_STRING("Playing"));

  // printf("-------- PlayInternal --------\n");

  int retval;

  nsAutoString shellPath(NS_LITERAL_STRING(".Play.sh"));  

  // shellPath will change from leafName to full Linux path if it exists
  if (ShellScriptExists(shellPath)) 
  {
    // Append parameters to the call
    shellPath.Append(NS_LITERAL_STRING(" file "));
    shellPath.Append(aPath);

    shellPath.Append(NS_LITERAL_STRING(" volume "));
    shellPath.AppendInt(mVol);

    shellPath.Append(NS_LITERAL_STRING(" pitch "));
    shellPath.AppendInt(mPitchInt);
    
    shellPath.Append(NS_LITERAL_STRING(" rate "));
    shellPath.Append(NS_ConvertUTF8toUTF16(mRateChar));

    // printf("-------- (Invoking %s) --------\n", NS_ConvertUTF16toUTF8(shellPath).get());

    retval = system(NS_ConvertUTF16toUTF8(shellPath).get());
  }
    else 
  {
    // printf("-------- system(%s) --------\n", NS_ConvertUTF16toUTF8(mPlayCmd).get());
    retval = system(NS_ConvertUTF16toUTF8(mPlayCmd).get());
  }

  if (retval != 0) 
  {
    printf("-------- PLAY ERROR --------\n");
    return NS_ERROR_FAILURE;
  }

  // printf("-------- PlayInternal RETURNING --------\n");

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::Stop()
{
  // printf("mozSecurebrowser::Stop\n"); 

  if (!mFestInit) return NS_ERROR_NOT_INITIALIZED;

  // festival_eval_command("(audio_mode 'shutup)");
 
  int retval;

  nsAutoString shellPath(NS_LITERAL_STRING(".Stop.sh"));

  // shellPath will change from leafName to full Linux path if it exists
  if (ShellScriptExists(shellPath)) retval = system(NS_ConvertUTF16toUTF8(shellPath).get());
  else if (mSoxKludge) retval = system("killall -KILL sox > /dev/null 2>&1");
  else retval = system("killall -KILL play > /dev/null 2>&1");

  if (retval != 0) 
  {
    // printf("-------- STOP ERROR --------\n");
  }
  
  mStatus.Assign(NS_LITERAL_STRING("Stopped"));

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::Pause()
{
  // printf("mozSecurebrowser::Pause\n"); 

  if (!mFestInit) return NS_ERROR_NOT_INITIALIZED;

  int retval;

  nsAutoString shellPath(NS_LITERAL_STRING(".Pause.sh"));

  // shellPath will change from leafName to full Linux path if it exists
  if (ShellScriptExists(shellPath)) retval = system(NS_ConvertUTF16toUTF8(shellPath).get());
  else if (mSoxKludge) retval = system("killall -STOP sox");
  else retval = system("killall -STOP play");

  if (retval != 0) 
  {
    //printf("-------- PAUSE ERROR (%d) --------\n", retval);
    return NS_ERROR_FAILURE;
  }
  
  mStatus.Assign(NS_LITERAL_STRING("Paused"));

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::Resume()
{
  // printf("mozSecurebrowser::Resume\n"); 

  if (!mFestInit) return NS_ERROR_NOT_INITIALIZED;

  int retval;

  nsAutoString shellPath(NS_LITERAL_STRING(".Resume.sh"));

  // shellPath will change from leafName to full Linux path if it exists
  if (ShellScriptExists(shellPath)) retval = system(NS_ConvertUTF16toUTF8(shellPath).get());
  else if (mSoxKludge) retval = system("killall -CONT sox");
  else retval = system("killall -CONT play");

  if (retval != 0) 
  {
    //printf("-------- RESUME ERROR --------\n");
    return NS_ERROR_FAILURE;
  }
  
  mStatus.Assign(NS_LITERAL_STRING("Playing"));

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetVoiceName(nsAString &_retval)
{
  // printf("mozSecurebrowser::GetVoiceName\n"); 

  if (!mFestInit) return NS_ERROR_NOT_INITIALIZED;

  /** 
   * IMPORTANT - format of text output of this shell script MUST be single line
   *   eg:
   *       ked_diphone
   */
  nsAutoString shellPath(NS_LITERAL_STRING(".GetVoiceName.sh"));  

  // shellPath will change from leafName to full Linux path if it exists
  if (ShellScriptExists(shellPath)) 
  {
    nsresult rv = GetShellScriptOutputAsAString(shellPath, _retval);

    if (NS_FAILED(rv)) 
    {
      printf("-------- GetVoiceName SHELL SCRIPT ERROR --------\n");
      return rv;
    }

    return NS_OK;
  }

  EST_String command = "current-voice";
  LISP lispCommand = read_from_string((char*)command);
  LISP lispOutput = leval(lispCommand, NULL);

  EST_String voice = siod_sprint(lispOutput);
  _retval.Assign(NS_ConvertUTF8toUTF16(voice.str()));  

  // printf("mozSecurebrowser::GetVoiceName - %s\n", NS_ConvertUTF16toUTF8(_retval).get());

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::SetVoiceName(const nsAString &aVoiceName)
{
  // printf("mozSecurebrowser::SetVoiceName (%s)\n", NS_ConvertUTF16toUTF8(aVoiceName).get()); 

  if (!mFestInit) return NS_ERROR_NOT_INITIALIZED;

  Stop();

  nsAutoString shellPath(NS_LITERAL_STRING(".SetVoiceName.sh"));  

  // shellPath will change from leafName to full Linux path if it exists
  if (ShellScriptExists(shellPath)) 
  {
    // append a space then aVoiceName as argument to script
    shellPath.Append(NS_LITERAL_STRING(" "));
    shellPath.Append(aVoiceName);
    system(NS_ConvertUTF16toUTF8(shellPath).get());

    return NS_OK;
  }

  // printf("%s\n", NS_ConvertUTF16toUTF8(NS_LITERAL_STRING("(voice_") + aVoiceName + NS_LITERAL_STRING(")")).get());

  festival_eval_command(NS_ConvertUTF16toUTF8(NS_LITERAL_STRING("(voice_") + aVoiceName + NS_LITERAL_STRING(")")).get());

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetVolume(PRInt32 *_retval)
{
  // printf("mozSecurebrowser::GetVolume\n"); 

  if (!mFestInit) return NS_ERROR_NOT_INITIALIZED;

  *_retval = mVol;

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::SetVolume(PRInt32 aVolume)
{
  // printf("mozSecurebrowser::SetVolume\n"); 

  if (!mFestInit) return NS_ERROR_NOT_INITIALIZED;

  if (aVolume < 0) aVolume = 0;

  if (aVolume > 10) aVolume = 10;

  mVol = aVolume;

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetStatus(nsAString & _retval)
{
  // printf("mozSecurebrowser::GetStatus\n"); 

  if (!mFestInit) return NS_ERROR_NOT_INITIALIZED;

  // paused, playing stopped

  if (mStatus.Equals(NS_LITERAL_STRING("Playing"))) 
  {
    // printf("USE PS TO SEE IF ITS STILL PLAYING ...\n");

    FILE *fp = NULL;
    int status;
    char path[MAX_PATH];

    nsAutoString name(NS_LITERAL_STRING(".GetStatus.sh"));
    GetShellScript(name, &fp);

    if (!fp) fp = popen("ps -u `whoami` -o comm | grep play", "r");

    if (fp == NULL) return NS_ERROR_FAILURE;

    nsAutoString out;

    while (fgets(path, PATH_MAX, fp) != NULL)
    {
      if (out.Length()) out.Append(NS_LITERAL_STRING(","));

      // printf("%s", path);

      out.Append(NS_ConvertUTF8toUTF16(path).get());

      out.StripWhitespace();
    }

    if (out.IsEmpty()) mStatus.Assign(NS_LITERAL_STRING("Stopped"));

    status = pclose(fp);

    if (status == -1) return NS_ERROR_FAILURE;
  }

  _retval.Assign(mStatus);

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetVoices(nsAString & aVoices)
{
  // printf("mozSecurebrowser::GetVoices\n"); 

  if (!mFestInit) return NS_ERROR_NOT_INITIALIZED;

  /** 
   * IMPORTANT - format of text output of this shell script MUST be single line
   *   eg:
   *       kal_diphone
   *       ked_diphone
   */
  nsAutoString shellPath(NS_LITERAL_STRING(".GetVoices.sh"));  

  // shellPath will change from leafName to full Linux path if it exists
  if (ShellScriptExists(shellPath)) 
  {
    nsresult rv = GetShellScriptOutputAsAString(shellPath, aVoices);

    if (NS_FAILED(rv)) 
    {
      printf("-------- GetVoices SHELL SCRIPT ERROR --------\n");
      return rv;
    }

    return NS_OK;
  }

  EST_String command = "(voice.list)";
  LISP lispCommand = read_from_string((char*)command);

  LISP lispOutput = leval(lispCommand, NULL);

  EST_StrList voiceList;
  siod_list_to_strlist(lispOutput, voiceList);

  for (PRInt32 i = 0; i < voiceList.length(); i++) 
  {
    EST_String voice = voiceList.nth(i);
    aVoices.Append(NS_ConvertUTF8toUTF16(voice.str()));

    if (i != voiceList.length() - 1)
      aVoices.Append(NS_LITERAL_STRING(","));
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
mozSecurebrowser::GetRate(PRInt32 *aRate)
{
  // printf("mozSecurebrowser::GetRate\n");

  PRInt32 error;

  *aRate = mRate.ToInteger(&error, 10);

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::SetRate(PRInt32 aRate)
{
  // printf("mozSecurebrowser::SetRate\n");

  if (aRate > 20) aRate = 20;
  if (aRate < 0)  aRate = 0;

  // convert 0-20 to .5 - 2
  // n = -.075 * aRate + 2

  PRFloat64 n = (-.075 * aRate) + 2;

  if (aRate == 13) strcpy(mRateChar, "1");

  else sprintf(mRateChar, "%.2f", n);

  // printf("-------- mRateChar(%s) --------\n", mRateChar);

  mRate.Assign(NS_LITERAL_STRING(""));
  mRate.AppendInt(aRate);

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetPitch(PRInt32 *aPitch)
{
  // printf("mozSecurebrowser::GetPitch\n");

  PRInt32 error;

  *aPitch = mPitch.ToInteger(&error, 10);

  // printf("-------- Pitch (%d) --------\n", *aPitch);

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::SetPitch(PRInt32 aPitch)
{
  // printf("mozSecurebrowser::SetPitch\n");

  if (aPitch > 20) aPitch = 20;

  if (aPitch < 0) aPitch = 0;

  // convert 1-20 to -500 to 500

  PRFloat64 n = (1000/19) * (aPitch - 1) - 500;

  mPitchInt = (int)n;

  // flatten out pitch to 0
  if (aPitch == 10) mPitchInt = 0;

  // printf("-------- Pitch (%d) --------\n", mPitchInt);

  mPitch.Assign(NS_LITERAL_STRING(""));
  mPitch.AppendInt(aPitch);

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetSystemVolume(PRInt32 *aVolume)
{
  // printf("mozSecurebrowser::GetSystemVolume\n");

  if (mSystemMute)
  {
    *aVolume = 0;
    return NS_OK;
  }

  int i = 0;
  int len = GetCards();

  while (NS_FAILED(GetSystemVolumeInternal(i, aVolume)) && i<len) ++i;

  return NS_OK;
}

nsresult
mozSecurebrowser::GetSystemVolumeInternal(PRInt32 aCardNumber, PRInt32 *aVolume)
{
  // printf("mozSecurebrowser::GetSystemVolume\n");

  // amixer -c 0 sget Front,0  | grep % | sed -e's/Front Left: Playback [0-9]*\s\[//g' | sed -e's/%\].*//g'

  // const char* cmd;
  char cmd[MAX_PATH];

  // headphones
  // const char* hcmd = "amixer -c1 sget Speaker,0 | grep % | sed -e's/Front Right: Playback [0-9]*\\s\\[//g' | sed -e's/%\\].*//g' | sed /Left/d";

  // printf("%s\n", hcmd);

  // amixer get Master | grep % | sed '3d' | sed '2d' | sed -e's:%.*$::g' | sed -e's:^.*\[::g'
  // sprintf(cmd, "amixer get Master | grep %% | sed '3d' | sed '2d' | sed -e's:%%.*$::g' | sed -e's:^.*\\[::g'");
  sprintf(cmd, "amixer get Master | grep %% | sed '3d' | sed '2d' | sed -e's:%%.*$::g' | sed -e's:^.*\\[::g'");

  FILE *fp = NULL;
  int status;
  char volume[MAX_PATH];

  // printf("%s\n", cmd);

  nsAutoString name(NS_LITERAL_STRING(".GetSystemVolume.sh"));
  GetShellScript(name, &fp);

  if (!fp) fp = popen(cmd, "r");

  if (fp == NULL) return NS_ERROR_FAILURE;

  nsAutoString out;

  while (fgets(volume, PATH_MAX, fp) != NULL)
  {
    printf("GET VOLUME: %s", volume);

    out.Assign(NS_ConvertUTF8toUTF16(volume).get());

    out.StripWhitespace();

    break;
  }

  nsresult rv = NS_OK;

  status = pclose(fp);

  if (status == -1) rv = NS_ERROR_FAILURE;

  printf("******** GET VOLUME %s ********\n", NS_ConvertUTF16toUTF8(out).get());

  if (out.IsEmpty()) rv = NS_ERROR_FAILURE;

  PRInt32 error;

  *aVolume = out.ToInteger(&error, 10)/10;

  // printf("Headphone Volume [%d]\n", hVol);
  printf("Master Volume [%d]\n", *aVolume);

  return rv;
}

NS_IMETHODIMP
mozSecurebrowser::SetSystemVolume(PRInt32 aVolume)
{
  // printf("mozSecurebrowser::SetSystemVolume (%d)\n", aVolume);

  if (mSystemMute) 
  {
    SetSystemMute(false);
    GetSystemVolume(&aVolume);
  }

  mSystemMute = false;

  if (aVolume < 0) aVolume = 0;

  if (aVolume > 10) aVolume = 10;

  printf("******** SET VOLUME %d ********\n", aVolume);

  int len = GetCards();

  // printf("******** Number of Cards [%d] ********\n", len);

  int i = 0;
  while (NS_FAILED(SetSystemVolumeInternal(i, aVolume)) && i<len) ++i;

  i = 0;
  while (NS_FAILED(SetSystemHeadphoneVolumeInternal(i, aVolume)) && i<len) ++i;

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetSystemMute(bool *aSystemMute)
{
  *aSystemMute = mSystemMute;

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::SetSystemMute(bool aSystemMute)
{
  printf("******** SET MUTE (%d) ********\n", aSystemMute);

  int len = GetCards();

  if (aSystemMute) 
  {
    system("amixer -D pulse set Master mute > /dev/null 2>&1");

    system("amixer set Master mute  > /dev/null 2>&1");

    for (int i=0; i<len; ++i) 
    {
      char cmd[MAX_PATH];
      sprintf(cmd, "for c in $(amixer | grep Simple | grep -o \\'.*\\' | sort | uniq | sed -e's: :_:g' | sed ':a;N;$!ba;s/\\n/ /g'); do n=`echo $c | sed -e's:_: :g'`; amixer -q -c%d sset $n,0 mute > /dev/null 2>&1; amixer -q -c%d sset Speaker,0 mute > /dev/null 2>&1; done", i, i);

      system(cmd);
      // printf("%s\n", cmd);
    }

    GetSystemVolume(&mLastMuteVol);
  }
    else
  {
    system("amixer -D pulse set Master unmute > /dev/null 2>&1");

    system("amixer set Master unmute > /dev/null 2>&1");

    for (int i=0; i<len; ++i) 
    {
      char cmd[MAX_PATH];
      sprintf(cmd, "for c in $(amixer | grep Simple | grep -o \\'.*\\' | sort | uniq | sed -e's: :_:g' | sed ':a;N;$!ba;s/\\n/ /g'); do n=`echo $c | sed -e's:_: :g'`; amixer -q -c%d sset $n,0 unmute > /dev/null 2>&1; amixer -q -c%d sset Speaker,0 unmute > /dev/null 2>&1; done", i, i);

      system(cmd);
      // printf("%s\n", cmd);
    }
  }
  
  mSystemMute = aSystemMute;

  return NS_OK;
}

NS_IMETHODIMP
mozSecurebrowser::GetPermissive(bool *aPermissive) 
{
  return NS_ERROR_NOT_IMPLEMENTED;
} 

NS_IMETHODIMP
mozSecurebrowser::SetPermissive(bool aPermissive) 
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
mozSecurebrowser::SetSystemVolumeInternal(PRInt32 aCardNumber, PRInt32 aVolume)
{
  // printf("-------- mozSecurebrowser::SetSystemVolumeInternal card(%d) volume(%d) --------\n", aCardNumber, aVolume);

  nsAutoString b;

  PRInt32 vol = aVolume * 10;

  int retval;

  nsAutoString shellPath(NS_LITERAL_STRING(".SetSystemVolume.sh"));

  // shellPath will change from leafName to full Linux path if it exists
  if (ShellScriptExists(shellPath)) 
  {
    shellPath.Append(NS_LITERAL_STRING(" volume "));
    shellPath.AppendInt(vol);

    shellPath.Append(NS_LITERAL_STRING(" cardnumber "));
    shellPath.AppendInt(aCardNumber);

    retval = system(NS_ConvertUTF16toUTF8(shellPath).get());
  } 
    else
  {
    b.AssignLiteral("amixer set Master ");
    b.AppendInt(vol);
    b.AppendLiteral("% > /dev/null 2>&1");

    // printf("******** [%s] ********\n", NS_ConvertUTF16toUTF8(b).get());
    retval = system(NS_ConvertUTF16toUTF8(b).get());

    int len = GetCards();

    for (int i=0; i<len; ++i) 
    {
      char cmd[MAX_PATH];
      sprintf(cmd, "for c in $(amixer | grep Simple | grep -o \\'.*\\' | sort | uniq | sed -e's: :_:g' | sed ':a;N;$!ba;s/\\n/ /g'); do n=`echo $c | sed -e's:_: :g'`; amixer -q -c%d sset $n,0 %d%% > /dev/null 2>&1; amixer -q -c%d sset Speaker,0 %d%% > /dev/null 2>&1; done", i, vol, i, vol);
      system(cmd);
      // printf("%s\n", cmd);
    }
  }

  nsresult rv = NS_OK;

  if (retval != 0) 
  {
    printf("-------- SET SYSTEM VOLUME ERROR --------\n");
    rv = NS_ERROR_FAILURE;
  }
  
  return rv;
}

nsresult
mozSecurebrowser::SetSystemHeadphoneVolumeInternal(PRInt32 aCardNumber, PRInt32 aVolume)
{
  // printf("-------- mozSecurebrowser::SetSystemHeadphoneVolumeInternal card(%d) volume(%d) --------\n", aCardNumber, aVolume);

  nsAutoString h,a;

  // usb headphones
  // amixer -c1 sset Speaker,0 100% 100%
  h.AssignLiteral("amixer -q -c");
  h.AppendInt(aCardNumber);
  h.AppendLiteral(" sset Speaker,0 ");

  PRInt32 vol = aVolume * 10;

  h.AppendInt(vol);
  h.AppendLiteral("% ");

  h.AppendInt(vol);
  h.AppendLiteral("%");

  // hide output as card may not be present
  h.AppendLiteral(" > /dev/null 2>&1");

  a.AssignLiteral("amixer set Headphone ");
  a.AppendInt(vol);
  a.AppendLiteral("% ");
  a.AppendLiteral("> /dev/null 2>&1");

  // amixer set Headphone
  system(NS_ConvertUTF16toUTF8(a).get());

  // printf("******** Headphones [%s] ********\n", NS_ConvertUTF16toUTF8(h).get());

  nsresult rv = NS_OK;

  int retval;

  nsAutoString shellPath(NS_LITERAL_STRING(".SetSystemHeadphoneVolume.sh"));

  // shellPath will change from leafName to full Linux path if it exists
  if (ShellScriptExists(shellPath)) 
  {
    shellPath.Append(NS_LITERAL_STRING(" volume "));
    shellPath.AppendInt(vol);

    shellPath.Append(NS_LITERAL_STRING(" cardnumber "));
    shellPath.AppendInt(aCardNumber);

    retval = system(NS_ConvertUTF16toUTF8(shellPath).get());
  } 
    else 
  {
    retval = system(NS_ConvertUTF16toUTF8(h).get());
  }

  if (retval != 0) 
  {
    // printf("-------- SET HEADPHONE VOLUME ERROR --------\n");
    rv = NS_ERROR_FAILURE;
  }
  
  return rv;
}

NS_IMETHODIMP
mozSecurebrowser::GetSoxVersion(nsAString& aVersion)
{
  aVersion.Assign(mSoxVersion);

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

