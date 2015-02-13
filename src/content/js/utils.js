/**
 * Convert a string containing binary values to hex.
 */

const OPEN_KIOSK_DEBUG = false;
const gUtilPrefs = jslibGetService("@mozilla.org/preferences-service;1", "nsIPrefService").getBranch(null);
var gUserTypedPwd = "";

function binaryToHex (input)
{
  return ("0" + input.toString(16)).slice(-2);
}

function md5 (str, aUseRuntime)
{
  var converter = jslibCreateInstance("@mozilla.org/intl/scriptableunicodeconverter", "nsIScriptableUnicodeConverter");

  converter.charset = "UTF-8";

  if (aUseRuntime)
  {
    var r = new Runtime;

    str += atob(r.key);
  }

  var data = converter.convertToByteArray(str, {});

  var ch = jslibCreateInstance("@mozilla.org/security/hash;1", "nsICryptoHash");
  ch.init(Components.interfaces.nsICryptoHash.MD5);

  ch.update(data, data.length);

  var hash = ch.finish(false);

  hash = [binaryToHex(hash.charCodeAt(i)) for (i in hash)].join("");

  return hash;
}

function hash (aString)
{
  return md5(aString);
}

function hashSB (aString)
{
  return md5(aString, true);
}

function goQuitApp ()
{
  if (opener) 
  {
    close();
    return;
  }

  var appStartup = Components.classes['@mozilla.org/toolkit/app-startup;1'].
                   getService(Components.interfaces.nsIAppStartup);

  appStartup.quit(Components.interfaces.nsIAppStartup.eAttemptQuit);
}

function savedInRegistry ()
{
  var rv;

  try 
  {
    rv = gUtilPrefs.getBoolPref("bmakiosk.admin.savedinreg");
  }
    catch (e) 
  {
    rv = false; // default to prefs
  }

  return rv;
}

function validatePassword (userTyped)
{
  try
  {
    const G_LOGIN_USER = "admin";
    var storedPwd;

    var rv = false;

    if (savedInRegistry()) 
    {
      // look in registry (Windows only)
      if ("@mozilla.org/windows-registry-key;1" in jslibCls)
      {
        try 
        {
          var key = jslibCreateInstance("@mozilla.org/windows-registry-key;1", "nsIWindowsRegKey");

          if (key)
          {        
            const nsIWindowsRegKey = jslibI.nsIWindowsRegKey;
            key.open(nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                     "Software\\OpenKiosk",
                     nsIWindowsRegKey.ACCESS_READ);
            storedPwd = key.readStringValue("KioskAdmin");
            key.close();

            if (storedPwd && storedPwd == md5(btoa(userTyped))) rv = true;
          }
        }
          catch (e) { }
      }
    }
      else 
    {
      // look in prefs
      try 
      {
        storedPwd = gUtilPrefs.getCharPref("bmakiosk.admin.login");
      }
        catch (e) 
      {
        // first time in
        if (userTyped == G_LOGIN_USER) 
        {
          gUtilPrefs.setCharPref("bmakiosk.admin.login", md5(btoa(G_LOGIN_USER)));
          storedPwd = md5(btoa(G_LOGIN_USER));
        }
      }
      // var match = (storedPwd == md5(btoa(userTyped)));

      if (storedPwd && storedPwd == md5(btoa(userTyped))) rv = true;
    }  

    if (rv) gUserTypedPwd = userTyped;
  }
    catch (e) { jslibPrintError(e); }

  return rv;
}

function emptyClipBoard ()
{
  try
  {
    var ch = jslibGetService("@mozilla.org/widget/clipboardhelper;1", "nsIClipboardHelper");

    try
    {
      // OS's that support selection like Linux
      var supportsSelect = jslibGetService("@mozilla.org/widget/clipboard;1", "nsIClipboard").supportsSelectionClipboard();

      if (supportsSelect) ch.copyStringToClipboard("", jslibI.nsIClipboard.kSelectionClipboard);

      ch.copyStringToClipboard("", jslibI.nsIClipboard.kGlobalClipboard);

    }
      catch (e) { jslibPribtError(e); }

  }
    catch (e) { jslibPrintError(e); }
}

function getOpenKioskURL () { return "chrome://openkiosk/content/"; }

function restartOpenKiosk () { adminLaunchDialog("restart"); }

function launchJSConsole () { adminLaunchDialog("console"); }

function launchAdminSettings () { adminLaunchDialog("admin"); }

function launchVersion () { adminLaunchDialog("about"); }

function launchAddOns () { adminLaunchDialog("addons"); }

function launchConnectionSettings () { adminLaunchDialog("preferences"); }

function adminLaunchDialog (aCMD)
{
  var ps = jslibGetService("@mozilla.org/embedcomp/prompt-service;1", "nsIPromptService");

  var settingsBundle = document.getElementById("settingsDefaults");

  var pwd = { value: "" };

  var extMsg, action;

  switch (aCMD) 
  {
    case "admin":
      extMsg = "launch admin settings";
      action = launchAdmin;
      break;

    case "restart":
      extMsg = "restart";
      action = jslibRestartApp;
      break;

    case "console":
      extMsg = "launch javascrpt console";
      action = toJavaScriptConsole;
      break;

    case "preferences":
      extMsg = "launch connection settings";
      action = launchProxy;
      break;

    case "addons":
      extMsg = "launch addons dialog";
      action = launchAddons;
      break;

    case "about":
      extMsg = "launch version dialog";
      action = launchAbout;
      break;

    default:
      extMsg = "quit OpenKiosk";
      action = goQuitApp;
  }

  var msg = "Enter password to " + extMsg;

  if (!ps.promptPassword(window,
                         "Open Kiosk Admin",
                         msg,
                         pwd,
                         null,
                         {value: null}))
    return;  // dialog cancelled

  if (!validatePassword(pwd.value)) 
  {
    ps.alert(window, "Open Kiosk Admin", "Incorrect password");
    return;
  }

try
{
  action();
}
  catch (e) { jslibPrintError(e); }
}

function showNotificationBox (aMsg, aPriority, aCallBack)
{
try
{
  var notificationBox = gBrowser.getNotificationBox();

  // debugAlert("showNotificationBox", typeof(aCallBack));

  var buttons = null;

  if (aCallBack && typeof(aCallBack) != "number") 
  {
    buttons = [{
                 label     : "OK",
                 accessKey : null,
                 popup     : null,
                 callback  : aCallBack 
               }];
  }


  /********
    * PRIORITY_INFO_LOW
    * PRIORITY_INFO_MEDIUM
    * PRIORITY_INFO_HIGH
    * PRIORITY_WARNING_LOW
    * PRIORITY_WARNING_MEDIUM
    * PRIORITY_WARNING_HIGH
    * PRIORITY_CRITICAL_LOW
    * PRIORITY_CRITICAL_MEDIUM
    * PRIORITY_CRITICAL_HIGH
    * PRIORITY_CRITICAL_BLOCK
  ********/

  const priority = notificationBox[aPriority];

  // for testing
  // const priority = notificationBox.PRIORITY_CRITICAL_HIGH;

  notificationBox.appendNotification(aMsg,
                                     null,
                                     "chrome://browser/skin/Info.png",
                                     priority, buttons);
}
  catch (e) { debugAlert("ERROR", "showNotificationBox", e); }

}

function closeCurrentNotificatinBox ()
{

  var nb = gBrowser.getNotificationBox();

  if (nb.currentNotification) nb.removeCurrentNotification();
}

function clearContexts ()
{
  var el = document.getElementById("context_openTabInWindow");

  var popup = el.parentNode;

  popup.removeChild(el.previousSibling);
  popup.removeChild(el);

  el = document.getElementById("context_bookmarkTab");
  popup.removeChild(el);

  el = document.getElementById("context_bookmarkAllTabs");
  popup.removeChild(el.previousSibling);
  popup.removeChild(el);

}

// function to manually turn on or off javascript
function toggleGlobalJS (aOn)
{
  // debugAlert("toggleGlobalJS", aOn);

  // set global javascript pref
  gUtilPrefs.setBoolPref("javascript.enabled", aOn);
}

function stripWhiteSpace (aString)
{
  var stripWhitespace = /^\s*(.*)\s*$/;

  return aString.replace(stripWhitespace, "$1");
}

function debugAlert ()
{
  alert(Array.join(arguments, ": "));
}

function launchAdmin ()
{
  var windows = Cc['@mozilla.org/appshell/window-mediator;1']
                  .getService(Ci.nsIWindowMediator)
                  .getEnumerator("bmaKiosk:Settings");

  while (windows.hasMoreElements()) 
  {
    var sw = windows.getNext();

      sw.focus();
      return;
  }

  openDialog("chrome://bmakiosk/content/settings.xul", "_blank",
                          "chrome,modal=yes,resizable=no,centerscreen", gUserTypedPwd);
}

function fixUpURI (aURL)
{
  // tidy up the request URL for compatibility
  var fu = jslibGetService("@mozilla.org/docshell/urifixup;1", "nsIURIFixup");

  var rv = fu.createFixupURI(aURL, jslibI.nsIURIFixup.FIXUP_FLAGS_MAKE_ALTERNATE_URI);

  return rv;
}

function launchAbout ()
{
  const CHROME  = "chrome://bmakiosk/content/aboutDialog.xul";
  openDialog(CHROME, "about", "chrome,modal=yes,resize=no,centerscreen");
}

function launchAddons ()
{
  BrowserOpenAddonsMgr();
}

function launchProxy ()
{
  // openPreferences();
  openDialog("chrome://browser/content/preferences/connection.xul", "_blank",
                          "chrome,modal=yes,resizable=no,centerscreen");
}

function launchCharset ()
{
  // openPreferences();
  openDialog("chrome://global/content/customizeCharset.xul", "PrefWindow", "chrome,modal=yes,resizable=yes", null);
}

function launchLanguageDialog ()
{
  openDialog("chrome://browser/content/preferences/languages.xul", "chrome,modal=yes", null);
}

function sizeWindow ()
{
  var height = screen.height;
  var width = screen.availWidth;

  window.resizeTo(width, height);
  window.moveTo(0, 0);
}

function resizeWindow ()
{
  var height = screen.height;
  var width = screen.availWidth;

  window.resizeTo(width, height);
}

