// Hooks for SecureBrowser
include(jslib_dirutils);  
include(jslib_dir);  

function SecureBrowserHooks ()
{
  rewrite();

  createUserPluginsDir();

  var os = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);

  // rewrite js close to quit app --pete
  os.addObserver(rewriteClose, "StartDocumentLoad", false);
  os.addObserver(rewriteClose, "EndDocumentLoad", false);
}

var rewriteClose = 
{
  observe: function (aSubject, aTopic, aState)
  {
    if (aTopic == "StartDocumentLoad" || aTopic == "EndDocumentLoad")
    {
      rewrite();
    }
  }
}

function rewrite ()
{
  if (!gBrowser) return;

  var win = gBrowser.contentWindow.wrappedJSObject;

  // jslibPrintCaller("URI", gBrowser.currentURI.spec, "win", win);

  win.SecureBrowser = new Object;
  win.SecureBrowser.CloseWindow = goQuitApp;
  win.SecureBrowser.Restart = jslibRestartApp;
  win.SecureBrowser.emptyClipBoard = emptyClipBoard;

  // cache functions
  win.SecureBrowser.clearCache   = kioskClearCacheAll;
  win.SecureBrowser.clearCookies = removeCookies;

  // print functions
  win.print = openKioskPrint;
  win.SecureBrowser.printWithDialog = SecureBrowserPrint;

  // md5 hash function
  win.SecureBrowser.hash = hashSB;

  win.SecureBrowser.showChrome = SecureBrowserShowChrome;
}

function kioskClearCacheAll ()
{
  kioskClearCache(jslibI.nsICache.STORE_ANYWHERE, true);
}

function SecureBrowserPrint ()
{
  PrintUtils.print();
}

function SecureBrowserShowChrome (aShow)
{
try
{
  window.fullScreen = !aShow;
  var r = jslibCreateInstance("@mozilla.org/securebrowser;1", "mozISecurebrowser");

  gPrefService.setBoolPref("bmakiosk.mode.showChrome", aShow);

  if (aShow)
  {
    var height = screen.height/2;
    var width = screen.availWidth/2;

    window.resizeTo(width, height);
    window.moveTo(0, 0);

    // r.permissive = true;
  }
    else 
  {
    // set window to full screen
    // setTimeout(resizeWindow, 1000);

    r.permissive = false;
  }
}
  catch (e) { /*alert(e);*/ }
}

function createUserPluginsDir ()
{
  try
  {
    var du = new DirUtils;
    var d = new Dir(du.getPrefsDir());
  
    d.append("plugins");

    // jslibPrint("prefs dir", d.path);
    d.create();
  }
    catch (e) { jslibPrint("ERROR", e); }
}

