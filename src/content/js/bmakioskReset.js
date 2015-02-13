/*
The contents of this file are subject to the Mozilla Public
License Version 1.1 (the "License"); you may not use this file
except in compliance with the License. You may obtain a copy of
the License at http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS
IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
implied. See the License for the specific language governing
rights and limitations under the License.

The Original Code is copyright (C) 2004 Jim Massey.
The Initial Developer of the Original Code is Jim Massey

Portions created by Mozdev Group, Inc. are
Copyright (C) 2004 Mozdev Group, Inc.  All
Rights Reserved.

Original Author: Jim Massey <massey@stlouis-shopper.com>
Contributor(s): Brian King <brian@mozdevgroup.com>
*/

var gPref = jslibGetService("@mozilla.org/preferences-service;1", "nsIPrefService").getBranch("bmakiosk.");


function sessionEnabled () { return gPref.getBoolPref("reset.timer.on"); }

function startSessionListener ()
{
  addEventListener("click", resetTimer, true);
  addEventListener("mousemove", resetTimer, false);
  addEventListener("scroll", resetTimer, false);
}

if (sessionEnabled()) { startSessionListener(); }

// globals
var gTimerCount = 0;
var gTimer = 0;
var gCountdownTimer;
var gWarningSeconds;

// **** reset bmakiosk to initial startup state *********
function bmakioskReset (aForce) 
{
  try
  {
    aForce = (aForce == true);

    closeCurrentNotificatinBox();

    // Clear timer now to avoid reset being called again 
    // when the warning window is open
    clearTimeout(gTimer);

    var warningEnabled;
    try
    {
      warningEnabled = gPref.getBoolPref("reset.warningenabled");
    }
      catch (e) { warningEnabled = true; }

    try
    {
      if (warningEnabled) handleWarning(aForce);
      else finishClearSession();
    }
      catch (e) { jslibPrintError(e); }
  }
    catch (e) { jslibPrintError(e); }
}

function finishClearSession ()
{
  clearTimer();

  // XXX close any open tabs --pete
  closeTabs();

  // document.getElementById("FindToolbar").close();
  gFindBar.close();

  // User session cleanup
  kioskClearMemCache();  // openkiosk.js
  removeCookies();       // openkiosk.js
  zoomReset();           // openkiosk.js

  // call this if the urlbar is present in UI
  if (typeof(uribarListDump) == "function") uribarListDump();

  // reset AUP permission
  var aupPref = document.getElementById("prefAUPAccepted").value = false;

  // For now, load blank page until aup permission is granted/denied
  var aupOn = document.getElementById("prefAUPFiltering").value;
  if (aupOn) loadURI("chrome://bmakiosk/content/AUPwait.html");  

  BrowserAttractHome();

  clearUserData();
}

function handleWarning (aForce)
{

  if (!aForce)
  {

    var wn = getWebNavigation();
    // if we can't go back or forward, then no need to prompt ...
    if (!wn.canGoBack && !wn.canGoForward && gBrowser.browsers.length == 1 && 
        URLIsHomeOrAttract(gBrowser.currentURI.spec) && !bmaProgressListener.mTabOpened) 
    {
      return;
    }
  }


  startCountdown();
}

function clearTimer ()
{
  clearTimeout(gCountdownTimer);
  resetTimer();
  gCountdownTimer = null;
}

function continueSession ()
{
  clearTimer();

  gBrowser.getNotificationBox().removeCurrentNotification();
}

function startCountdown ()
{
  var defaultCountdown, resetCountdown;
  try
  {
    resetCountdown = gPref.getIntPref("reset.warningtimer");
  }
    catch (e)
  {
    defaultCountdown = gDefaultsBundle.getString("defaultWarningSecs")
    resetCountdown = defaultCountdown;
  }

  gWarningSeconds = resetCountdown;
  continueCountdown();
}

function continueCountdown ()
{
  try
  {
    var message = gKioskBundle.getFormattedString("countdown.text", [gWarningSeconds]);

    gWarningSeconds = (gWarningSeconds - 1);

    var notificationBox = gBrowser.getNotificationBox();

    if (gCountdownTimer && !notificationBox.currentNotification) 
    {
      clearTimer();
      return;
    }

    if (gWarningSeconds >= 0)
    {
      if (!notificationBox.currentNotification)
      {
        var label = "Cancel " + gPref.getCharPref("reset.buttontext");
        var buttons = [{
                         label: label,
                         accessKey: null,
                         popup: null,
                         callback: continueSession
                       }];

        const priority = notificationBox.PRIORITY_WARNING_HIGH;
        notificationBox.appendNotification(message,
                                           null,
                                           "chrome://browser/skin/Info.png",
                                           priority, buttons);
      }
        else
      {
        notificationBox.currentNotification.label = message;
      }

      gCountdownTimer = setTimeout(continueCountdown, 1000);
    }
      else
    {
      gBrowser.getNotificationBox().removeCurrentNotification();
      finishClearSession();
    }
  } 
    catch (e) { jslibPrintError(e); }
}

// **** bmakiosk timer - reset after period in inactivity ********
function resetTimer ()
{
  if (!sessionEnabled()) return;

  var defaultTime, resetTime;
  try
  {
    resetTime = gPref.getIntPref("reset.timer");
  }
    catch (e)
  {
    defaultTime = gDefaultsBundle.getString("defaultResetMins")
    resetTime = defaultTime;
  }

  // 1 min = 60000ms
  resetTime = resetTime * 60000;

  if (gTimerCount >= 1) clearTimeout(gTimer);

  gTimer = setTimeout(bmakioskReset, resetTime);
  gTimerCount = gTimer;
}

// from reset button
function manualReset () 
{
  bmakioskReset(true);
}

function closeTabs ()
{
  gBrowser.removeAllTabsBut(gBrowser.mCurrentTab);
  bmaProgressListener.mTabOpened = false;
}

// Bug 203
function clearUserData ()
{
  try
  {
    clearHttpPass();

    // var de = gPref.getBoolPref("cache.diskenabled");
    var de = false;

    if (!de)
    {
      var gh = jslibGetService("@mozilla.org/browser/global-history;2", "nsIBrowserHistory");

      gh.removeAllPages();
    }

    var ce = gPref.getBoolPref("cookies.enabled");

    if (!ce)
    {
      var cm = jslibGetService("@mozilla.org/cookiemanager;1", "nsICookieManager");
      cm.removeAll();
    }

    var memEnabled = gPref.getBoolPref("cache.memenabled");

    if (!memEnabled) 
    {
      var os = jslibGetService("@mozilla.org/observer-service;1", "nsIObserverService");
      os.addObserver(bmaObserver, "EndDocumentLoad", false);
    }
  } 
    catch (e) { alert(e); jslibPrintError(e); }
}

// Bug 202
function clearHttpPass ()
{
  try
  {
    var signedon;
    try
    {
      gPref.getCharPref("signon.SignonFileName");
      signedon = true;
    }
      catch (e) { signedon = false; }

    if (signedon)
    {
      try
      {
        gPref.clearUserPref("signon.SignonFileName");
      }
        catch (e) { }
    }
  
    // set memory cache to off - not needed --pete
    // setMemory(OFF);
  
    // clear all http passwords
    var am = jslibGetService("@mozilla.org/network/http-auth-manager;1", "nsIHttpAuthManager");
    am.clearAll();

    try
    {
      var pm = Components.classes["@mozilla.org/passwordmanager;1"].getService(Components.interfaces.nsIPasswordManager);

      var enm = pm.enumerator;

      while (enm.hasMoreElements())
      {
        var p = enm.getNext();

        p instanceof jslibI.nsIPassword;

        pm.removeUser(p.host, p.user);
      }
    }
      catch (e) 
    {
      // For FF3
      var lm = Components.classes["@mozilla.org/login-manager;1"].getService(Components.interfaces.nsILoginManager);
      
      lm.removeAllLogins();
    } 
  } 
    catch (e) { jslibPrintMsg("ERROR:clearHttpPass", e); }
}

// when memory pfref is set to off, it block httpass altogether
// need to do some more testing to get the session htpass purged --pete
function setMemory (aBoolSetting)
{
  gPref.setBoolPref("browser.cache.memory.enable", aBoolSetting);
}

function notifyHistoryObservers ()
{
  try
  {
    if (OPEN_KIOSK_DEBUG) return;

    var os = jslibGetService("@mozilla.org/observer-service;1", "nsIObserverService");
    os.notifyObservers(null, "browser:purge-session-history", "");
  } 
    catch (e) { jslibPrintMsg("ERROR:notifyHistoryObservers", e); }
}

const bmaObserver = 
{
  observe : function (subject, topic, data)
  {
    if (topic != "EndDocumentLoad") return;

    notifyHistoryObservers();

    var os = jslibGetService("@mozilla.org/observer-service;1", "nsIObserverService");
    os.removeObserver(bmaObserver, "EndDocumentLoad");
  }
};

function clearSession ()
{
try
{
  removeEventListener("click", resetTimer, true);
  removeEventListener("mousemove", resetTimer, false);
}
  catch (e) { jslibPrintError(e); }
}

