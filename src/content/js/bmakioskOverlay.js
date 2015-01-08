const G_DEFAULT_KIOSK_CHROME_URL = "chrome://bmakiosk/content/go.xul";
const G_PREFS_FILENAME = "bmakioskprefs.js";
var gKioskPrefService = null, gKioskPref = null;
var gKioskPrefsFile;

window.addEventListener("load", checkDefaultBrowser, false);

/*********** Launch App ***************/
function launchApp (mode, url) 
{
  var modeArg;

  if (!mode) mode = getMode();

  if (url == null) url = "none";

  modeArg = "mode="+mode;

  window.openDialog(G_DEFAULT_KIOSK_CHROME_URL, "", 
                    "chrome,dialog=no", modeArg+" "+url);
}

function launchAbout () 
{
  const CHROME  = "chrome://bmakiosk/content/aboutDialog.xul";
  window.openDialog(CHROME, "_blank", "chrome,dialog,dependent=no,resize=no,centerscreen");
}

function launchAdmin () 
{
  window.openDialog("chrome://bmakiosk/content/settings.xul", "_blank",
                          "chrome,dialog=no,resizable=no,centerscreen");
}

function setupKioskPrefs ()
{
  // Set up kiosk prefs file
  var NSIFILE = Components.interfaces.nsIFile;
  var dirLocator = Components.classes["@mozilla.org/file/directory_service;1"]
          .getService(Components.interfaces.nsIProperties);

  var prefsDir = dirLocator.get("PrfDef", NSIFILE).path;

  gKioskPrefsFile = Components.classes["@mozilla.org/file/local;1"]
                           .createInstance(Components.interfaces.nsILocalFile);

  gKioskPrefsFile.initWithPath(prefsDir);

  gKioskPrefsFile.append(G_PREFS_FILENAME);

  if (!gKioskPrefsFile.exists())  
  {
    gKioskPref = null;
    return;
  }

  // prefs
  gKioskPrefService = 
    jslibGetService("@mozilla.org/preferences-service;1", "nsIPrefService");

  gKioskPrefService.readUserPrefs(gKioskPrefsFile);
  // XX don't use getDefaultBranch

  gKioskPref = gKioskPrefService.getBranch(null);
}

// Bug 306 - This is a listener that checks the Kiosks dedicated prefs file.
// It checks for the pref -- browser.chromeURL -- to see if the Kiosk should be launched
// or Mozilla as normal. We can't use normal mozilla prefs, because we don't want the prefs.js
// version of this pref. We want the version in bmakioskprefs.js - Brian
function checkDefaultBrowser()
{
  setupKioskPrefs();
  if (gKioskPref == null)
    return;

  var browserURL;
  var launchKiosk = false;
  try
  {
    browserURL = gKioskPref.getCharPref("browser.chromeURL");
    if (browserURL == G_DEFAULT_KIOSK_CHROME_URL)
      launchKiosk = true;
    else
      launchKiosk = false;
  } catch(ex) {}

  window.removeEventListener("load", checkDefaultBrowser, false);

  if (launchKiosk)  {
    var browserMode = getMode();

    // Bug 520 - Make sure its not the default homepage passed in when desktop shortcut is used
    var page;
    var defaultPage = "";
    var argPage = "";

    // ugly hack to revert back to Mozilla browser prefs, 
    // then back again to Kiosk prefs when we get what we need.
    resetPrefs();
    var mozPages = getMozillaHomePages();
    setupKioskPrefs();

    for (i=0;i<mozPages.length;i++) {
      defaultPage += mozPages[i];
    }
    if (window.arguments[0]) {
      argPage = window.arguments[0];
      argPage = argPage.replace(/\n/, "");
      if (argPage != defaultPage)
        page = window.arguments[0];
      else
        page = null;
    }
    else  {
      page = null;
    }

    var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
    var windowManagerInterface = windowManager.QueryInterface(Components.interfaces.nsIWindowMediator);
    var topWindow = windowManagerInterface.getMostRecentWindow("bmakiosk:browser");
    
    if (topWindow)  
    {
      // alert("We have top window");
      topWindow.focus();
      var theBrowser = topWindow.document.getElementById("content");
      var tabAdded = theBrowser.addTab(page);
      theBrowser.selectedTab = tabAdded;
    }
    else  {
      launchApp(browserMode, page);
    }
    window.close();
  }
  else {
    resetPrefs();
  }
}

// If we don't reset the open prefs and read in the default ones
// then all browser prefs in a normal Mozilla browser session will be 
// written to the wrong file (bmakioskprefs.js).
// This also ensures that bma prefs are not written to prefs.js in reverse situation
function resetPrefs()
{
  //gKioskPrefService.savePrefFile(gKioskPrefsFile);
  gKioskPrefService.resetUserPrefs();
  gKioskPrefService.readUserPrefs(null);
  gKioskPref = gKioskPrefService.getBranch(null);
}

function getMode()
{
  var browserMode;
  try {
    browserMode = gKioskPref.getCharPref("bmakiosk.browser.mode");
  }
  catch(ex) {
    browserMode = "full" // default
  }
  return browserMode;
}

function getMozillaHomePages()
{
  var URIs = [];
  try {
    URIs[0] = pref.getComplexValue("browser.startup.homepage",
                                   Components.interfaces.nsIPrefLocalizedString).data;
    var count = pref.getIntPref("browser.startup.homepage.count");
    for (var i = 1; i < count; ++i) {
      URIs[i] = pref.getComplexValue("browser.startup.homepage."+i,
                                     Components.interfaces.nsIPrefLocalizedString).data;
    }
  } catch(e) {
  }

  return URIs;
}
