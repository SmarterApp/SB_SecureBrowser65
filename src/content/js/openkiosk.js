// Hooks for Open Kiosk

var gURIFixup = jslibGetService("@mozilla.org/docshell/urifixup;1", "nsIURIFixup");
const nsIURIFixup = jslibI.nsIURIFixup;

var gKioskBundle, gDefaultsBundle, gNavBar;
var gAttractUIShown = false;
function openKioskHooks ()
{
  try
  {
    window.fullScreen = true;

    try
    {
      var r = jslibCreateInstance("@mozilla.org/securebrowser;1", "mozISecurebrowser");
      r.permissive = false;
    } 
      catch (e) {}

#ifndef XP_MACOSX
    // important turn off FullScreen Toolbar
    FullScreen.showXULChrome("toolbar", false);
#endif

    // set window to full screen
    setTimeout(sizeWindow, 1);

    gNavBar = document.getElementById("nav-bar");

    emptyClipBoard();

    kioskClearCache(jslibI.nsICache.STORE_ON_DISK);

    // user data may need to be cleared
    clearUserData();
    resetAUPSession();

    gBrowser.addProgressListener(bmaProgressListener);
    gBrowser.tabContainer.addEventListener("TabOpen", onTabOpen, false);

    gKioskBundle    = document.getElementById("bundle_bmakiosk");
    gDefaultsBundle = document.getElementById("bmakioskDefaults");

    readToolbarSettings();
    BrowserAttractHome();

    parseWhitelist();
    parseWhitelist("aup");
    // parseWhitelist("js");

    SecureBrowserHooks();
  }
    catch (e) { jslibPrintError(e); }
}

function kioskClearMemCache ()
{
  var cacheService = jslibGetService("@mozilla.org/network/cache-service;1", "nsICacheService");
  cacheService.evictEntries(jslibI.nsICache.STORE_IN_MEMORY);
}

function kioskClearCache (aType, aForce)
{
  try
  {
    var de = gPref.getBoolPref("cache.diskenabled");

    if (!aForce && de) return;

    var cacheService = jslibGetService("@mozilla.org/network/cache-service;1", "nsICacheService");

    cacheService.evictEntries(aType);
  }
    catch (e) { }
}

function BrowserAttractHome ()
{
  var homepage;
  var attractOn = document.getElementById("prefAttractEnabled").value;

  if (attractOn)
    homepage = document.getElementById("prefAttractPage").value || "about:blank";
  else    
    homepage = atob(document.getElementById("prefHomePage").value) || gDefaultsBundle.getString("defaultHomepage");

  gAttractUIShown = false;
  attractUI(homepage, attractOn);

  closeCurrentNotificatinBox();

  loadURI(homepage);
}

function setZoomDisplay (aZoom)
{
  try
  {
    var zoomFactor = aZoom.toFixed(2)*100;

    // jslibPrintCaller("Zoom", aZoom, "zoomFactor", zoomFactor);

    document.getElementById("zoomText").value = parseInt(zoomFactor) + "%";
  }
    catch (e) { jslibPrintError(e); } 
}

function bmaSavePage ()
{
  var skipFP = !gPref.getBoolPref("save.usefilepicker");

  saveDocument(window.content.document, skipFP);
}

function readToolbarSettings ()
{
  try
  {
    var currentSet = document.getElementById("prefButtons").value;

    var navBar = document.getElementById("nav-bar");

    navBar.currentSet = currentSet;

    var uiShow = gPrefService.getBoolPref("bmakiosk.ui.show");

    if (!currentSet || !uiShow) navBar.hidden = true;
    else navBar.hidden = false;

    // document.getElementById("status-bar").hidden = !document.getElementById("prefStatusbar").value;

    var throbber = XULBrowserWindow.throbberElement;

    if (throbber && !/navigator-throbber/.test(currentSet)) 
    {
      throbber.hidden = true;
      throbberElement = null;
    }
      else
    {
      XULBrowserWindow.throbberElement = document.getElementById("navigator-throbber");
      XULBrowserWindow.throbberElement.hidden = false;
    }

    bmaSetButtonLabels();
  }
    catch (e) { jslibPrintError(e); } 
}

function bmaSetButtonLabels ()
{
  try
  {
    var b = document.getElementById("save-button");

    if (b) b.label = gPref.getCharPref("save.buttontext");

    b = document.getElementById("reset-button");

    if (b) b.label = gPref.getCharPref("reset.buttontext");
  }
    catch (e) { jslibPrintError(e); } 
}

function removeCookies ()
{
  try
  {
    var cookiemanager = jslibGetService("@mozilla.org/cookiemanager;1", "nsICookieManager");

    cookiemanager.removeAll();
  }
    catch (e) { jslibError("Failed to remove cookies", e); }
}

function zoomReset ()
{
  FullZoom.reset();
  document.getElementById("zoomText").value = "100%";
}

var bmaProgressListener = 
{
  mPageBlocked : false,
  mBlockedMsg : null,
  mTabOpened : false,

  onProgressChange : function (wp, req, cur, max, curtotal, maxtotal) {},

  onStateChange : function (wp, req, state, status) 
  {
    try
    {
      rewrite();

      if (!req) return;

      if (req.name == "about:blank" || req.name == "about:document-onload-blocker" || req.name == "about:addons") return;

      if (this.mPageBlocked && state & Components.interfaces.nsIWebProgressListener.STATE_STOP) 
      {
        var uri = jslibQI(req, "nsIChannel").URI;

        if (!uri) uri = fixUpURI(req.name);

        if (!validateProtocol(uri.scheme)) 
        {
          req.cancel(Components.results.NS_ERROR_ABORT);
          return;
        }

        // debugAlert("onStateChange BLOCK");
        setTimeout(showNotificationBox, 500, bmaProgressListener.mBlockedMsg, "PRIORITY_CRITICAL_HIGH");
      }
    }
      catch (e) { jslibPrinterror(e); }
  },

  onLocationChange : function (wp, req, loc) 
  {
    try
    {
      rewrite();

      if (!req && !loc) 
      {
        this.mPageBlocked = true;
        return;
      }

      this.mPageBlocked = false;

      showUI(loc);

      if (loc.spec == "about:blank" || loc.spec == "about:document-onload-blocker" || loc.spec == "about:addons") return;

      if (URLIsHomeOrAttract(loc.spec)) return;

      var flags = jslibI.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY; 

      if (!validateProtocol(loc.scheme))
      {

        if (req) req.cancel(Components.results.NS_ERROR_ABORT);
        // debugAlert("BLOCKED PROTOCOL", loc.spec);
        this.mPageBlocked = true;
        this.mBlockedMsg = "Restricted Protocol:  "+loc.scheme;

        // loadURI("about:blank");
        getWebNavigation().loadURI("about:blank", flags, null, null, null);

        return;
      }

      if (!whiteListCheck(loc.spec, "url")) 
      {
        if (req) req.cancel(Components.results.NS_ERROR_ABORT);
        this.mPageBlocked = true;
        this.mBlockedMsg = "Restricted Page:  " + (loc.host || loc.spec);
        // debugAlert("BLOCKED PAGE", this.mBlockedMsg);

        getWebNavigation().loadURI("about:blank", flags, null, null, null);
        // loadURI("about:blank"); 

        return;
      }

      if (req) handleAUP(loc.spec, req);
    }
      catch (e) { jslibPrintError(e); }
  },

  onStatusChange : function (wp, req, status, message) {},

  onSecurityChange : function (wp, req, state) {}
}

function resetAUPSession ()
{
  // reset AUP permission
  var aupPref = document.getElementById("prefAUPAccepted").value = false;
}

function handleAUP (aURL, aReq)
{
  if (!aURL) return;

  var url = aURL;

  var aupOn = document.getElementById("prefAUPFiltering").value;

  if (!aupOn) return;

  var aupPref = document.getElementById("prefAUPAccepted");

  if (!aupPref.value && !whiteListCheck(url, "aup"))
  {
    var permission = { granted: true };
    var aupPage = document.getElementById("prefAUPAgreementFile").value;
    window.openDialog("chrome://bmakiosk/content/aup.xul", "",
                      "chrome,dialog,centerscreen,dependent,modal",
                      permission, aupPage);

    aupPref.value = permission.granted;

    if (!permission.granted)
    {

      loadURI("about:blank");

      // debugAlert("handleAUP", aReq);

      aReq && aReq.cancel(Components.results.NS_ERROR_ABORT);

      BrowserAttractHome();

/********
      // try to go home, or if that's not in the list then go to the
      // first site in the list
      var home = document.getElementById("prefHomePage").value
      if (whiteListCheck(home, "aup"))
        url = home;
      else
      {
        url = getFirstEntry(gAUPWhiteList);

        // tt may be that the AUP file is not configured correctly
        if (url == "about:blank")
          setTimeout(statusMessage, 1500,
                     gKioskBundle.getString('auplistproblem'));
      }
********/
    }
  }
}

function attractUI(requestUrl, aAttractOn)
{
  // tidy up the homepage URL for compatibility

  var fixedRequest = gURIFixup.createFixupURI(requestUrl, nsIURIFixup.FIXUP_FLAGS_MAKE_ALTERNATE_URI);

  var show = aAttractOn || !document.getElementById("prefShowUI").value

  // debugAlert("attractUI", "show", show);

  gNavBar.collapsed = show;
  // gStatusBar && (gStatusBar.collapsed = show);
}

function attractURI ()
{
  var attractURL = document.getElementById("prefAttractPage").value;

  var rv = gURIFixup.createFixupURI(attractURL, nsIURIFixup.FIXUP_FLAGS_MAKE_ALTERNATE_URI);

  return rv;
}

function showUI (aURI)
{
  if (gAttractUIShown) return;

  if (aURI && aURI.equals(attractURI()))
  {
    // debugAlert("showUI", "URL IS ATTRACT PAGE");
    return;
  }

  // debugAlert("showUI", aURI.spec);

  gNavBar.collapsed = false;
  // gStatusBar && (gStatusBar.collapsed = false);
  gAttractUIShown = true;
}

function openKioskShutDown ()
{
  emptyClipBoard();
  clearUserData();
  kioskClearCache(jslibI.nsICache.STORE_ON_DISK);
}

// **** Print function ****
function openKioskPrint ()
{
  var dialog = gPref.getBoolPref("print.dialog");

  // debugAlert(dialog);

  if (dialog) PrintUtils.print();
  else openKioskPrintSilent();
}

function openKioskPrintSilent ()
{
  var printSettings = PrintUtils.getPrintSettings();

  // debugAlert(printSettings);

try
{
  // No feedback to user
  printSettings.printSilent   = true;
  printSettings.showPrintProgress = false;

  // Other settings
  printSettings.printBGImages = true;
  printSettings.shrinkToFit = true;

  // Frame settings
  printSettings.howToEnableFrameUI = printSettings.kFrameEnableAll;
  printSettings.printFrameType = printSettings.kFramesAsIs;

  try 
  {
    var ifreq = jslibQI(content, "nsIInterfaceRequestor");
    var webBrowserPrint = ifreq.getInterface(jslibI.nsIWebBrowserPrint);
    var printProgress = new kioskPrintListener;
    webBrowserPrint.print(printSettings, printProgress);
  } 
    catch (e) 
  { 
    var failmessage = gKioskBundle.getString('failedprinting');
    statusMessage(failmessage);
  }
}
  catch (e) { jslibPrintError(e); }
}

function Print () 
{
try
{
  var printService = jslibGetService("@mozilla.org/gfx/printsettings-service;1", 
                                     "nsIPrintSettingsService");

  var printSettings = printService.globalPrintSettings;

  if (!printSettings.printerName)
  {
    jslibQI(printService, "nsIPrintOptions");
    var ap = printService.availablePrinters();
    while (ap.hasMoreElements()) 
    {
      var pName = ap.getNext();
      jslibQI(pName, "nsISupportsString");
      break;
    }

    if (pName) printSettings.printerName = pName;
  }

  try 
  {
    var pn = printSettings.printerName;

    // Get any defaults from the printer 
    printService.initPrintSettingsFromPrinter(pn, printSettings);

  } 
    catch (e) 
  {
    printService.initPrintSettingsFromPrefs(printSettings, 
                                            true, 
                                            printSettings.kInitSaveAll);
  }

  // No feedback to user
  printSettings.printSilent   = true;
  printSettings.showPrintProgress = false;

  // Other settings
  printSettings.printBGImages = true;
  printSettings.shrinkToFit = true;

  // Frame settings
  printSettings.howToEnableFrameUI = printSettings.kFrameEnableAll;
  printSettings.printFrameType = printSettings.kFramesAsIs;

  try 
  {
    var ifreq = jslibQI(content, "nsIInterfaceRequestor");
    var webBrowserPrint = ifreq.getInterface(jslibI.nsIWebBrowserPrint);
    var printProgress = new kioskPrintListener;
    webBrowserPrint.print(printSettings, printProgress);
  } 
    catch (e) 
  { 
    var failmessage = gKioskBundle.getString('failedprinting');
    statusMessage(failmessage);
  }
}
  catch (e) { jslibPrintError(e); }
}

function kioskPrintListener ()
{
  this.init();
}

function statusMessage(message)
{
  var sbd = document.getElementById("statusbar-display");

  if (sbd) sbd.label = message;
}

kioskPrintListener.prototype =
{
  status : "",

  init : function()
  {
    gKioskBundle.getString('nowprinting');
  },
  
  onProgressChange : function() { },

  onStateChange : function() { },

  onStatusChange : function() { }
}

function makeURI (aURL)
{
  // tidy up the request URL for compatibility
  var fixup = jslibGetService("@mozilla.org/docshell/urifixup;1", "nsIURIFixup");

  var rv = fixup.createFixupURI(aURL, jslibI.nsIURIFixup.FIXUP_FLAGS_MAKE_ALTERNATE_URI);

  return rv;
}

function onTabOpen ()
{
  bmaProgressListener.mTabOpened = true;
}

function aboutEnabled ()
{
  return gPref.getBoolPref("protocols.about")
}
