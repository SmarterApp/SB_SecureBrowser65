/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

#include "nsXPCOMGlue.h"
#include "nsXULAppAPI.h"
#if defined(XP_WIN)
#include <windows.h>
#include <stdlib.h>
#elif defined(XP_UNIX)
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "plstr.h"
#include "prprf.h"
#include "prenv.h"

#include "nsCOMPtr.h"
#include "nsILocalFile.h"
#include "nsStringGlue.h"

// #include <festival.h>

#ifdef XP_WIN
// we want a wmain entry point
#include "nsWindowsWMain.cpp"
#define snprintf _snprintf
#define strcasecmp _stricmp
#endif
#include "BinaryPath.h"

#include "nsXPCOMPrivate.h" // for MAXPATHLEN and XPCOM_DLL

#include "mozilla/Telemetry.h"

// OPEN KIOSK
#ifdef XP_MACOSX
#include <prthread.h>
#include <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>

#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)

static PRInt32 gTL, gTR, gBL, gBR;

static PRBool gActivated = PR_FALSE;

static void RestoreWindows ()
{
  // KIOSK - show other open apps
  ProcessSerialNumber cp, p;

  GetFrontProcess(&cp);

  // ShowHideProcess(&cp, false);

  GetCurrentProcess(&p);

  CFStringRef n = NULL;
  CFStringRef fn = NULL;

  CopyProcessName(&p, &n);

  cp.lowLongOfPSN  = kNoProcess;

  while (GetNextProcess(&cp) == noErr)
  {
    CopyProcessName(&cp, &fn);

    if (CFStringCompare(n, fn, 0) != kCFCompareEqualTo)
    {
       // printf("Show (%s)\n", CFStringGetCStringPtr(fn, kCFStringEncodingMacRoman));
       ShowHideProcess(&cp, true);
    }
  }
}

static PRBool isOldVersion ()
{
  // printf("-------- isOlderVersion --------\n");

  SInt32 versMaj, versMin, versBugFix;
  Gestalt(gestaltSystemVersionMajor, &versMaj);
  Gestalt(gestaltSystemVersionMinor, &versMin);
  Gestalt(gestaltSystemVersionBugFix, &versBugFix);

  // printf("%ld.%ld.%ld\n", versMaj, versMin, versBugFix);

  return (versMin <= 5);
}

static PRBool isEightOrGreater ()
{
  SInt32 versMin;
  Gestalt(gestaltSystemVersionMinor, &versMin);

  return (versMin >= 8);
}

static void waitForProcess (const char *name, PRBool aSleep)
{
  // printf("-------- waitForProcess [%s] sleep (%d)--------\n", name, aSleep);

  ProcessSerialNumber p = { kNoProcess, kNoProcess };
  CFStringRef pn = NULL;

  PRBool found = PR_FALSE;

  while (GetNextProcess(&p) == noErr)
  {
    CopyProcessName(&p, &pn);

    // printf("(%s) Is Running\n", CFStringGetCStringPtr(pn, kCFStringEncodingMacRoman));

    if (strcmp(name, CFStringGetCStringPtr(pn, kCFStringEncodingMacRoman)) == 0)
    {
      // printf("-------- waitForProcess: Is Running (%s) --------\n", CFStringGetCStringPtr(pn, kCFStringEncodingMacRoman));
      found = PR_TRUE;

      if (strcmp("Dock", CFStringGetCStringPtr(pn, kCFStringEncodingMacRoman)) == 0) 
      { 
        // printf("-------- CALL SetSystemUIMode --------\n");
        // this is only called when the force quit window is open
        SetSystemUIMode(kUIModeNormal, 0);
      }

      break;
    }
  }

  if (!found) 
  {
    // printf("-------- PROCESS NOT FOUND SLEEPING --------\n");
    PR_Sleep(PR_MillisecondsToInterval(1000));
    waitForProcess(name, PR_TRUE);
  }
}

static void restartProcess (const char* aName, PRBool shouldWait=PR_TRUE)
{
  // printf("-------- restartProcess (%s) --------\n", aName);

  // SIGHUP
  ProcessSerialNumber p = { kNoProcess, kNoProcess };
  CFStringRef pn = NULL;

  while (GetNextProcess(&p) == noErr)
  {
    CopyProcessName(&p, &pn);

    if (strcmp(aName, CFStringGetCStringPtr(pn, kCFStringEncodingMacRoman)) == 0)
    {
      // printf("RESTARTING (%s)\n", CFStringGetCStringPtr(pn, kCFStringEncodingMacRoman));
      KillProcess(&p);
      break;
    }
  }

  if (shouldWait) waitForProcess(aName, PR_FALSE);
}

static CFURLRef getFileURL (const char* aID)
{
  // printf("-------- getFileURL --------\n");

  FSRef homeDir;
  if (FSFindFolder(kUserDomain, kCurrentUserFolderType, kDontCreateFolder, &homeDir) != noErr) return NULL;

  CFURLRef userDirURL = ::CFURLCreateFromFSRef(kCFAllocatorDefault, &homeDir);
  if (!userDirURL) return NULL;

  char id[MAXPATHLEN];
  id[0] = 0;

  PL_strcat(id, "Library/Preferences/");
  PL_strcat(id, aID);
  PL_strcat(id, ".plist");

  // printf("-------- getFileURL (%s) --------\n", id);

  CFStringRef s = CFStringCreateWithCString(kCFAllocatorDefault, id, kCFStringEncodingMacRoman);

  CFURLRef fileURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault, userDirURL, s, false);

  if (!fileURL) 
  {
    printf("------ ERROR CAN'T CREATE URL --------\n");
    return NULL;
  }

  return fileURL;
}

static CFPropertyListRef getPropertyList (CFURLRef aFileURL)
{
  // printf("-------- getPropertyList --------\n");

  CFStringRef pathStr = CFURLCopyFileSystemPath(aFileURL, kCFURLPOSIXPathStyle);

  char path[MAXPATHLEN];
  CFStringGetCString(pathStr, path, sizeof(path), kCFStringEncodingUTF8);

  // printf("------ PATH (%s) --------\n", path);

  SInt32 errorCode;
  CFDataRef resourceData;
  CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault, aFileURL, &resourceData, NULL, NULL, &errorCode);

  // printf("-------- errorCode (%d) --------\n", (int)errorCode);

  if (errorCode < 0) return NULL;

  CFStringRef errorString;
  CFPropertyListRef propertyList = CFPropertyListCreateFromXMLData(kCFAllocatorDefault, resourceData, kCFPropertyListMutableContainersAndLeaves, &errorString);

  return propertyList;
}

static void writeXMLData (CFURLRef aFileURL, CFDataRef xmlData)
{
  // printf("-------- writeXMLData --------\n");

  Boolean status;
  SInt32 errorCode;

  // Write the XML data to the file.
  status = CFURLWriteDataAndPropertiesToResource (aFileURL, xmlData, NULL, &errorCode);
}

static void enableScreenCapture ()
{
  // printf("-------- enableScreenCapture --------\n");

  CFURLRef fileURL = getFileURL("com.apple.screencapture");

  if (!fileURL) return;

  CFPropertyListRef pl = getPropertyList(fileURL);

  if (!pl) return;

  CFMutableDictionaryRef dict = CFDictionaryCreateMutableCopy(NULL, 0, static_cast<CFDictionaryRef>(pl));

  CFDictionarySetValue(dict, CFSTR("location"), CFSTR("~/Desktop"));

  // CFDataRef xmlData = CFPropertyListCreateXMLData(kCFAllocatorDefault, dict);
  // writeXMLData(fileURL, xmlData);

  CFStringRef error; 
  CFWriteStreamRef fs = CFWriteStreamCreateWithFile(kCFAllocatorDefault, fileURL);

  CFWriteStreamOpen(fs);

  if (fs == NULL) printf("-------- NULL FS --------\n");

  CFIndex ret = CFPropertyListWriteToStream(dict, fs, kCFPropertyListBinaryFormat_v1_0, &error);

  if (ret == 0) printf("-------- ERROR WRITING TO STREAM --------\n");
  // else printf("-------- BYTES WRITTEN (%ld) --------\n", ret);

  // printf("ERROR MSG (%s)\n", CFStringGetCStringPtr(error, kCFStringEncodingMacRoman));

  CFWriteStreamClose(fs);

  if (isEightOrGreater()) system("/usr/bin/defaults write com.apple.screencapture location ~/Desktop");

  // restartProcess("SystemUIServer", PR_TRUE);

  // PR_Sleep(PR_MillisecondsToInterval(3000));

  if (pl) CFRelease(pl);
  if (dict) CFRelease(dict);
  // if (xmlData) CFRelease(xmlData);
  if (fileURL) CFRelease(fileURL);

  system("/usr/bin/killall -HUP SystemUIServer");
}

static void disableVoiceOverSplashScreen ()
{
  // printf("-------- disableVoiceOverSplashScreen --------\n");

  // ~/Library/Preferences/com.apple.VoiceOverTraining doNotShowSplashScreen -boolean true

  CFURLRef fileURL = getFileURL("com.apple.VoiceOverTraining");

  if (!fileURL) return;

  CFPropertyListRef pl = getPropertyList(fileURL);

  CFMutableDictionaryRef dict;

  // create property list and value pairs
  if (!pl)
  {
    dict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);

    CFDictionaryAddValue(dict, CFSTR("doNotShowSplashScreen"), kCFBooleanFalse);
  }
    else
  {
    dict = CFDictionaryCreateMutableCopy(NULL, 0, static_cast<CFDictionaryRef>(pl));
  }

  CFDictionarySetValue(dict, CFSTR("doNotShowSplashScreen"), kCFBooleanTrue);

  CFDataRef xmlData = CFPropertyListCreateXMLData(kCFAllocatorDefault, dict);

  writeXMLData(fileURL, xmlData);
  
  if (pl) CFRelease(pl);

  if (dict) CFRelease(dict);
  if (xmlData) CFRelease(xmlData);
  if (fileURL) CFRelease(fileURL);
}

static void disableScreenCapture ()
{
  // printf("-------- disableScreenCapture --------\n");

  CFURLRef fileURL = getFileURL("com.apple.screencapture");

  if (!fileURL) return;

  CFPropertyListRef pl = getPropertyList(fileURL);

  CFMutableDictionaryRef dict;

  // create property list and value pairs
  if (!pl)
  {
    dict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);

    CFDictionaryAddValue(dict, CFSTR("location"), CFSTR("~/Desktop"));
  }
    else
  {
    dict = CFDictionaryCreateMutableCopy(NULL, 0, static_cast<CFDictionaryRef>(pl));
  }

  char id[MAXPATHLEN];
  id[0] = 0;

  PL_strcat(id, "~/Library/Application Support/");
  PL_strcat(id, STR(MOZ_APP_NAME));
  PL_strcat(id, "/Profiles");

  // printf("-------- %s --------\n", id);

  CFStringRef s = CFStringCreateWithCString(kCFAllocatorDefault, id, kCFStringEncodingMacRoman);

  CFDictionarySetValue(dict, CFSTR("location"), s);

  // CFDataRef xmlData = CFPropertyListCreateXMLData(kCFAllocatorDefault, dict);

  // CFDataRef copy = CFDataCreateCopy(kCFAllocatorDefault, dict);

  // writeXMLData(fileURL, xmlData);

  CFStringRef error; 
  CFWriteStreamRef fs = CFWriteStreamCreateWithFile(kCFAllocatorDefault, fileURL);

  CFWriteStreamOpen(fs);

  if (fs == NULL) printf("-------- NULL FS --------\n");

  CFIndex ret = CFPropertyListWriteToStream(dict, fs, kCFPropertyListBinaryFormat_v1_0, &error);

  if (ret == 0) printf("-------- ERROR WRITING TO STREAM --------\n");
  // else printf("-------- BYTES WRITTEN (%ld) --------\n", ret);

  // printf("ERROR MSG (%s)\n", CFStringGetCStringPtr(error, kCFStringEncodingMacRoman));

  CFWriteStreamClose(fs);

  if (isEightOrGreater()) 
  {
    char cmd[MAXPATHLEN];
    cmd[0] = 0;

    PL_strcat(cmd, "/usr/bin/defaults write com.apple.screencapture location \"");
    PL_strcat(cmd, id);
    PL_strcat(cmd, "\"");

    // printf("-------- cmd(%s) --------\n", cmd);

    system(cmd);
  }

  // restartProcess("SystemUIServer");
  system("/usr/bin/killall SystemUIServer");

  if (pl) CFRelease(pl);

  if (dict) CFRelease(dict);
  // if (xmlData) CFRelease(xmlData);
  if (fileURL) CFRelease(fileURL);
}

static PRBool setActiveCorners (CFMutableDictionaryRef dict, SInt32 aTL, SInt32 aTR, SInt32 aBL, SInt32 aBR)
{
  // printf("-------- setActiveCorners --------\n");

  PRBool rv = PR_FALSE;

  if (CFDictionaryContainsKey(dict, CFSTR("wvous-tl-corner"))) 
  {
    // printf("-------- CONTAINS KEY (wvous-tl-corner) --------\n");

    CFNumberRef intValue = (CFNumberRef) CFDictionaryGetValue(dict, CFSTR("wvous-tl-corner"));

    PRInt32 value = 0;
    CFNumberGetValue(intValue, kCFNumberSInt32Type, &value);

    // printf("-------- TL VALUE (%d) --------\n", value);

    rv = (value != 1);

    // printf("-------- rv (%d) --------\n", rv);
 
    if (rv)
    {
      if (gTL < 0) gTL = value;

      CFNumberRef num = CFNumberCreate(NULL, kCFNumberSInt32Type, &aTL);

      CFDictionarySetValue(dict, CFSTR("wvous-tl-corner"), num);

      if (num) CFRelease(num);
      if (intValue) CFRelease(intValue);
    }
  }

  if (CFDictionaryContainsKey(dict, CFSTR("wvous-tr-corner"))) 
  {
    // printf("-------- CONTAINS KEY (wvous-tr-corner) --------\n");

    CFNumberRef intValue = (CFNumberRef) CFDictionaryGetValue(dict, CFSTR("wvous-tr-corner"));

    PRInt32 value = 0;
    CFNumberGetValue(intValue, kCFNumberSInt32Type, &value);

    // printf("-------- TR VALUE (%d) --------\n", value);

    if (!rv) rv = (value != 1);

    // printf("-------- rv (%d) --------\n", rv);
 
    if (rv)
    {
      if (gTR < 0) gTR = value;

      CFNumberRef num = CFNumberCreate(NULL, kCFNumberSInt32Type, &aTR);

      CFDictionarySetValue(dict, CFSTR("wvous-tr-corner"), num);

      if (num) CFRelease(num);
      if (intValue) CFRelease(intValue);
    }
  }

  if (CFDictionaryContainsKey(dict, CFSTR("wvous-bl-corner"))) 
  {
    // printf("-------- CONTAINS KEY (wvous-bl-corner) --------\n");

    CFNumberRef intValue = (CFNumberRef) CFDictionaryGetValue(dict, CFSTR("wvous-bl-corner"));

    PRInt32 value = 0;
    CFNumberGetValue(intValue, kCFNumberSInt32Type, &value);

    // printf("-------- BL VALUE (%d) --------\n", value);

    if (!rv) rv = (value != 1);

    // printf("-------- rv (%d) --------\n", rv);
 
    if (rv)
    {
      if (gBL < 0) gBL = value;

      CFNumberRef num = CFNumberCreate(NULL, kCFNumberSInt32Type, &aBL);

      CFDictionarySetValue(dict, CFSTR("wvous-bl-corner"), num);

      if (num) CFRelease(num);
      if (intValue) CFRelease(intValue);
    }
  }

  if (CFDictionaryContainsKey(dict, CFSTR("wvous-br-corner"))) 
  {
    // printf("-------- CONTAINS KEY (wvous-br-corner) --------\n");

    CFNumberRef intValue = (CFNumberRef) CFDictionaryGetValue(dict, CFSTR("wvous-br-corner"));

    PRInt32 value = 0;
    CFNumberGetValue(intValue, kCFNumberSInt32Type, &value);

    // printf("-------- BR VALUE (%d) --------\n", value);

    if (!rv) rv = (value != 1);

    // printf("-------- rv (%d) --------\n", rv);
 
    if (rv)
    {
      if (gBR < 0) gBR = value;

      CFNumberRef num = CFNumberCreate(NULL, kCFNumberSInt32Type, &aBR);

      CFDictionarySetValue(dict, CFSTR("wvous-br-corner"), num);

      if (num) CFRelease(num);
      if (intValue) CFRelease(intValue);
    }
  }

  return rv;
}

static void disableActiveCorners ()
{
  // printf("-------- disableActiveCorners --------\n");

  gTL = gTR = gBL = gBR = -1;
  
  CFURLRef fileURL = getFileURL("com.apple.dock");

  if (!fileURL) return;

  CFPropertyListRef propertyList = getPropertyList(fileURL);

  CFMutableDictionaryRef dict;

  // create property list and value pairs
  if (!propertyList)
  {
    dict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);

    PRInt32 value = 1;
    CFNumberRef num = CFNumberCreate(NULL, kCFNumberSInt32Type, &value);

    CFDictionaryAddValue(dict, CFSTR("wvous-tl-corner"), num);
    CFDictionaryAddValue(dict, CFSTR("wvous-tr-corner"), num);
    CFDictionaryAddValue(dict, CFSTR("wvous-bl-corner"), num);
    CFDictionaryAddValue(dict, CFSTR("wvous-br-corner"), num);

    if (num) CFRelease(num);
  }
    else 
  {
    dict = CFDictionaryCreateMutableCopy(NULL, 0, static_cast<CFDictionaryRef>(propertyList));

    // printf("-------- SET ACTIVE CORNERS TO -  TL(1) TR(1) BL(1) BR(1)  --------\n");

    PRBool shouldSet = setActiveCorners(dict, 1, 1, 1, 1);

    // printf("-------- SHOULD SET ACTIVE CORNERS (%d) --------\n", shouldSet);

    if (!shouldSet) return;
  }

  // printf("-------- dict (%p) --------\n", dict);

  CFDataRef xmlData = CFPropertyListCreateXMLData(kCFAllocatorDefault, dict);

  // Write the XML data to the file.
  writeXMLData(fileURL, xmlData);

  // printf("-------- RESTARTING DOCK --------\n");

  restartProcess("Dock");

  if (xmlData) CFRelease(xmlData);
  if (dict) CFRelease(dict);
}

#endif

static void Output(const char *fmt, ... )
{
  va_list ap;
  va_start(ap, fmt);

#if defined(XP_WIN) && !MOZ_WINCONSOLE
  PRUnichar msg[2048];
  _vsnwprintf(msg, sizeof(msg)/sizeof(msg[0]), NS_ConvertUTF8toUTF16(fmt).get(), ap);
  MessageBoxW(NULL, msg, L"XULRunner", MB_OK | MB_ICONERROR);
#else
  vfprintf(stderr, fmt, ap);
#endif

  va_end(ap);
}

/**
 * Return true if |arg| matches the given argument name.
 */
static PRBool IsArg(const char* arg, const char* s)
{
  if (*arg == '-')
  {
    if (*++arg == '-')
      ++arg;
    return !strcasecmp(arg, s);
  }

#if defined(XP_WIN) || defined(XP_OS2)
  if (*arg == '/')
    return !strcasecmp(++arg, s);
#endif

  return PR_FALSE;
}

/**
 * A helper class which calls NS_LogInit/NS_LogTerm in its scope.
 */
class ScopedLogging
{
public:
  ScopedLogging() { NS_LogInit(); }
  ~ScopedLogging() { NS_LogTerm(); }
};

XRE_GetFileFromPathType XRE_GetFileFromPath;
XRE_CreateAppDataType XRE_CreateAppData;
XRE_FreeAppDataType XRE_FreeAppData;
#ifdef XRE_HAS_DLL_BLOCKLIST
XRE_SetupDllBlocklistType XRE_SetupDllBlocklist;
#endif
XRE_TelemetryAccumulateType XRE_TelemetryAccumulate;
XRE_mainType XRE_main;

static const nsDynamicFunctionLoad kXULFuncs[] = {
    { "XRE_GetFileFromPath", (NSFuncPtr*) &XRE_GetFileFromPath },
    { "XRE_CreateAppData", (NSFuncPtr*) &XRE_CreateAppData },
    { "XRE_FreeAppData", (NSFuncPtr*) &XRE_FreeAppData },
#ifdef XRE_HAS_DLL_BLOCKLIST
    { "XRE_SetupDllBlocklist", (NSFuncPtr*) &XRE_SetupDllBlocklist },
#endif
    { "XRE_TelemetryAccumulate", (NSFuncPtr*) &XRE_TelemetryAccumulate },
    { "XRE_main", (NSFuncPtr*) &XRE_main },
    { nsnull, nsnull }
};

static int do_main(const char *exePath, int argc, char* argv[])
{
  nsCOMPtr<nsILocalFile> appini;
#ifdef XP_WIN
  // exePath comes from mozilla::BinaryPath::Get, which returns a UTF-8
  // encoded path, so it is safe to convert it
  nsresult rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(exePath), PR_FALSE,
                                getter_AddRefs(appini));
#else
  nsresult rv = NS_NewNativeLocalFile(nsDependentCString(exePath), PR_FALSE,
                                      getter_AddRefs(appini));
#endif
  if (NS_FAILED(rv)) {
    return 255;
  }

  appini->SetNativeLeafName(NS_LITERAL_CSTRING("application.ini"));

  // Allow firefox.exe to launch XULRunner apps via -app <application.ini>
  // Note that -app must be the *first* argument.
  const char *appDataFile = getenv("XUL_APP_FILE");
  if (appDataFile && *appDataFile) {
    rv = XRE_GetFileFromPath(appDataFile, getter_AddRefs(appini));
    if (NS_FAILED(rv)) {
      Output("Invalid path found: '%s'", appDataFile);
      return 255;
    }
  }
  else if (argc > 1 && IsArg(argv[1], "app")) {
    if (argc == 2) {
      Output("Incorrect number of arguments passed to -app");
      return 255;
    }

    rv = XRE_GetFileFromPath(argv[2], getter_AddRefs(appini));
    if (NS_FAILED(rv)) {
      Output("application.ini path not recognized: '%s'", argv[2]);
      return 255;
    }

    char appEnv[MAXPATHLEN];
    snprintf(appEnv, MAXPATHLEN, "XUL_APP_FILE=%s", argv[2]);
    if (putenv(appEnv)) {
      Output("Couldn't set %s.\n", appEnv);
      return 255;
    }
    argv[2] = argv[0];
    argv += 2;
    argc -= 2;
  }

  nsXREAppData *appData;
  rv = XRE_CreateAppData(appini, &appData);
  if (NS_FAILED(rv)) {
    Output("Couldn't read application.ini");
    return 255;
  }

  int result = XRE_main(argc, argv, appData);
  XRE_FreeAppData(appData);
  return result;
}

int main(int argc, char* argv[])
{
// OPEN KIOSK
#ifdef XP_MACOSX
  CFStringRef s = CFStringCreateWithCString (kCFAllocatorDefault, argv[argc-1], kCFStringEncodingMacRoman);

  CFRange range = CFStringFind(s, CFSTR("-psn_"), 0);

  CFRange http = CFStringFind(s, CFSTR("http"), 0);

  CFRelease(s);

  if (argc == 1 || range.length == 5 || http.length == 4)
  {
    PR_SetEnv("KIOSK_FULL_SCREEN=1");

    const char *k = PR_GetEnv("KIOSK_DISABLE_ACTIVE_CORNERS");

    PR_SetEnv("KIOSK_DISABLE_ACTIVE_CORNERS=1");

    if (!k && !gActivated) 
    {
      // SystemUIServer
      disableScreenCapture();

      // if we are an old version we 
      // need active corners disabled
      if (isOldVersion())
      {
        // Dock
        disableVoiceOverSplashScreen();
        disableActiveCorners();
      }

      gActivated = PR_TRUE;
    }
  }
#endif

#if defined(XP_UNIX)
#ifndef XP_MACOSX
  // Disable PrintScreen Screen Shot Dialog
  system("gconftool-2 --type string --set /apps/metacity/global_keybindings/run_command_screenshot \"disabled\" > /dev/null 2>&1");
  system("gsettings set org.gnome.settings-daemon.plugins.media-keys screenshot \"disabled\" > /dev/null 2>&1");
  system("gsettings set org.gnome.settings-daemon.plugins.media-keys screenshot-clip \"disabled\" > /dev/null 2>&1");
  system("gsettings set org.gnome.settings-daemon.plugins.media-keys window-screenshot \"disabled\" > /dev/null 2>&1");
  system("gsettings set org.gnome.settings-daemon.plugins.media-keys window-screenshot-clip \"disabled\" > /dev/null 2>&1");
  system("gsettings set org.gnome.settings-daemon.plugins.media-keys area-screenshot \"disabled\" > /dev/null 2>&1");
  system("gsettings set org.gnome.settings-daemon.plugins.media-keys area-screenshot-clip \"disabled\" > /dev/null 2>&1");
  system("gsettings set org.gnome.mutter overlay-key \"\" > /dev/null 2>&1");
  system("gsettings set org.gnome.desktop.wm.keybindings minimize \"\" > /dev/null 2>&1");
  system("gsettings set org.gnome.desktop.wm.keybindings switch-to-workspace-down \"\" > /dev/null 2>&1");
  system("gsettings set org.gnome.desktop.wm.keybindings switch-to-workspace-up \"\" > /dev/null 2>&1");
  system("gsettings set org.gnome.shell.keybindings toggle-message-tray \"['']\" > /dev/null 2>&1");
  system("gsettings set org.gnome.shell.keybindings toggle-overview \"['']\" > /dev/null 2>&1");
  system("gsettings set org.gnome.shell.keybindings toggle-application-view \"['']\" > /dev/null 2>&1");

  // system("metacity --replace &");

  // Call startup script to cover any hot fixes
  system("./sbstartup.sh");
#endif
#endif

  char exePath[MAXPATHLEN];

  nsresult rv = mozilla::BinaryPath::Get(argv[0], exePath);
  if (NS_FAILED(rv)) {
    Output("Couldn't calculate the application directory.\n");
    return 255;
  }

  char *lastSlash = strrchr(exePath, XPCOM_FILE_PATH_SEPARATOR[0]);
  if (!lastSlash || (lastSlash - exePath > MAXPATHLEN - sizeof(XPCOM_DLL) - 1))
    return 255;

  strcpy(++lastSlash, XPCOM_DLL);

  int gotCounters;
#if defined(XP_UNIX)
  struct rusage initialRUsage;
  gotCounters = !getrusage(RUSAGE_SELF, &initialRUsage);
#elif defined(XP_WIN)
  // GetProcessIoCounters().ReadOperationCount seems to have little to
  // do with actual read operations. It reports 0 or 1 at this stage
  // in the program. Luckily 1 coincides with when prefetch is
  // enabled. If Windows prefetch didn't happen we can do our own
  // faster dll preloading.
  IO_COUNTERS ioCounters;
  gotCounters = GetProcessIoCounters(GetCurrentProcess(), &ioCounters);
  if (gotCounters && !ioCounters.ReadOperationCount)
#endif
  {
      XPCOMGlueEnablePreload();
  }


  rv = XPCOMGlueStartup(exePath);
  if (NS_FAILED(rv)) {
    Output("Couldn't load XPCOM.\n");
    return 255;
  }

  rv = XPCOMGlueLoadXULFunctions(kXULFuncs);
  if (NS_FAILED(rv)) {
    Output("Couldn't load XRE functions.\n");
    return 255;
  }

#ifdef XRE_HAS_DLL_BLOCKLIST
  XRE_SetupDllBlocklist();
#endif

  if (gotCounters) {
#if defined(XP_WIN)
    XRE_TelemetryAccumulate(mozilla::Telemetry::EARLY_GLUESTARTUP_READ_OPS,
                            int(ioCounters.ReadOperationCount));
    XRE_TelemetryAccumulate(mozilla::Telemetry::EARLY_GLUESTARTUP_READ_TRANSFER,
                            int(ioCounters.ReadTransferCount / 1024));
    IO_COUNTERS newIoCounters;
    if (GetProcessIoCounters(GetCurrentProcess(), &newIoCounters)) {
      XRE_TelemetryAccumulate(mozilla::Telemetry::GLUESTARTUP_READ_OPS,
                              int(newIoCounters.ReadOperationCount - ioCounters.ReadOperationCount));
      XRE_TelemetryAccumulate(mozilla::Telemetry::GLUESTARTUP_READ_TRANSFER,
                              int((newIoCounters.ReadTransferCount - ioCounters.ReadTransferCount) / 1024));
    }
#elif defined(XP_UNIX)
    XRE_TelemetryAccumulate(mozilla::Telemetry::EARLY_GLUESTARTUP_HARD_FAULTS,
                            int(initialRUsage.ru_majflt));
    struct rusage newRUsage;
    if (!getrusage(RUSAGE_SELF, &newRUsage)) {
      XRE_TelemetryAccumulate(mozilla::Telemetry::GLUESTARTUP_HARD_FAULTS,
                              int(newRUsage.ru_majflt - initialRUsage.ru_majflt));
    }
#endif
  }

  int result;
  {
    ScopedLogging log;
    result = do_main(exePath, argc, argv);
  }

  XPCOMGlueShutdown();

// OPEN KIOSK
#ifdef XP_MACOSX
  RestoreWindows();

  if (argc == 1 || PR_GetEnv("KIOSK_FULL_SCREEN"))
  {
    const char *child = PR_GetEnv("MOZ_LAUNCHED_CHILD");

    if (!child || strlen(child) == 0) enableScreenCapture();
  }
#endif

#if defined(XP_UNIX)
  // Enable PrintScreen Screen Shot Dialog
#ifndef XP_MACOSX
  const char *child = PR_GetEnv("MOZ_LAUNCHED_CHILD");
  // printf("MOZ_LAUNCHED_CHILD (%s) \n", child);

  if (strlen(child) == 0)
  {
    system("gconftool-2 --type string --set /apps/metacity/global_keybindings/run_command_screenshot \"Print\" > /dev/null 2>&1");
    system("gsettings reset org.gnome.settings-daemon.plugins.media-keys screenshot  > /dev/null 2>&1");
    system("gsettings reset org.gnome.settings-daemon.plugins.media-keys screenshot-clip  > /dev/null 2>&1");
    system("gsettings reset org.gnome.settings-daemon.plugins.media-keys window-screenshot  > /dev/null 2>&1");
    system("gsettings reset org.gnome.settings-daemon.plugins.media-keys window-screenshot-clip  > /dev/null 2>&1");
    system("gsettings reset org.gnome.settings-daemon.plugins.media-keys area-screenshot  > /dev/null 2>&1");
    system("gsettings reset org.gnome.settings-daemon.plugins.media-keys area-screenshot-clip  > /dev/null 2>&1");
    system("gsettings reset org.gnome.mutter overlay-key  > /dev/null 2>&1");
    system("gsettings reset org.gnome.desktop.wm.keybindings minimize  > /dev/null 2>&1");
    system("gsettings reset org.gnome.desktop.wm.keybindings switch-to-workspace-down  > /dev/null 2>&1");
    system("gsettings reset org.gnome.desktop.wm.keybindings switch-to-workspace-up  > /dev/null 2>&1");
    system("gsettings reset org.gnome.shell.keybindings toggle-message-tray  > /dev/null 2>&1");
    system("gsettings reset org.gnome.shell.keybindings toggle-overview  > /dev/null 2>&1");
    system("gsettings reset org.gnome.shell.keybindings toggle-application-view  > /dev/null 2>&1");

  // system("compiz --replace &");

  // Call shutdown script to cover any hot fixes
  system("./sbshutdown.sh");
  }
#endif
#endif

  return result;
}
