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

Portions related to homepage handling copied and modified from
pref-navigator.js and is copyright of respective authors in that 
file.

Original Author: Brian King <brian@mozdevgroup.com>
Contributor(s): MozdevGroup Inc.
*/

// UI globals
var gKioskStartupHomepage;
var gDefaultPageButton;
var gAttractCheckbox;
var gRedirectCheckbox;
var gKioskAttractHomepage;
var gKioskRedirectPage;
var gBrowserCheckbox;
var gModeRadio;
var gCacheCheckbox;
var gMemCacheCheckbox;
var gCookiesCheckbox;
var gTabsCheckbox;
var gUIRadio;
var gCustomizeDeck;
var gFilterCheck;
var gFilterFileField;
var gFilterFileButton;
var gFilterFileEdit;
var gJSAllCheck;
var gFileProtoCheck;
var gFTPProtoCheck;
var gAboutProtoCheck;
// var gJSFilterFileField;
// var gJSFilterFileButton;
var gAUPCheck;
var gAUPFileField;
var gAUPFileButton;
var gAUPFilterFileField;
var gAUPFilterFileButton;
var gResetMinutes;
var gWarningSeconds;
var gWarningCheck;
var gResetEnableCheck;
var gPasswordField;
var gPwdRadio;
var gPrintRadio;
var gPrintPageCheck;
var gSaveBtn;
var gSaveFilepicker;
var gSaveLoc;
var gResetBtn;
var gAddOns;
var gFileUpload;
var gUserAgentTb; 

// String bundles
var gSettingsBundle;
var gDefaultsBundle;
var gBrandBundle;

var gDefaultPage;
var gLoginCount;

const G_DEFAULT_KIOSK_CHROME_URL = "chrome://bmakiosk/content/";
const G_DEFAULT_MOZILLA_CHROME_URL = "chrome://browser/content/browser.xul";

// For homepage handling
const nsIPrefService         = jslibI.nsIPrefService;
const nsIPrefLocalizedString = jslibI.nsIPrefLocalizedString;
const nsISupportsString      = jslibI.nsISupportsString;
const PREFSERVICE_CONTRACTID = "@mozilla.org/preferences-service;1";

var gPrefService = null, gPref = null;
const G_LOGIN_HOST = "bmakiosk.admin";
const G_LOGIN_USER = "admin";

var gGlobalPrefsFile    = null;
var gGlobalAddPrefsFile = null;

var gPS = jslibGetService("@mozilla.org/embedcomp/prompt-service;1", "nsIPromptService");

include(jslib_file);
include(jslib_dirutils);

function onLoad()
{
  // define UI globals
  gKioskStartupHomepage = document.getElementById("kioskStartupHomepage");
  gDefaultPageButton = document.getElementById("browserUseDefault");
  gAttractCheckbox = document.getElementById("attractEnabled");
  gRedirectCheckbox = document.getElementById("redirectEnabled");
  gKioskAttractHomepage = document.getElementById("kioskAttractHomepage");
  gKioskRedirectPage = document.getElementById("kioskRedirectPage");
  // gBrowserCheckbox = document.getElementById("defaultBrowser");
  gModeRadio = document.getElementById("kioskmoderadio");
  gCacheCheckbox = document.getElementById("cacheEnabled");
  gMemCacheCheckbox = document.getElementById("memcacheEnabled");
  gCookiesCheckbox = document.getElementById("cookiesEnabled");
  gTabsCheckbox = document.getElementById("tabsEnabled");
  gFilterCheck = document.getElementById("filtersEnabled");
  gFilterFileField = document.getElementById("allowed.file");
  gFilterFileButton = document.getElementById("allowed.button");
  gFilterFileEdit = document.getElementById("filter.edit");
  // gJSFilterFileField = document.getElementById("allowedjs.file");
  // gJSFilterFileButton = document.getElementById("allowedjs.button");
  gJSAllCheck = document.getElementById("jsallEnabled");
  gFileProtoCheck = document.getElementById("fileProtoEnabled");
  gFTPProtoCheck = document.getElementById("ftpProtoEnabled");
  gAboutProtoCheck = document.getElementById("aboutProtoEnabled");
  gAUPFileField = document.getElementById("aup.file");
  gAUPFileButton = document.getElementById("aupfile.button");
  gAUPFilterFileField = document.getElementById("aup.exempt");
  gAUPFilterFileButton = document.getElementById("aupexempt.button");
  gAUPCheck = document.getElementById("aupEnabled");
  gResetMinutes = document.getElementById("resetInterval");
  gWarningCheck = document.getElementById("warningEnabled");
  gResetEnableCheck = document.getElementById("enable-session-reset");
  gWarningSeconds = document.getElementById("warningSeconds");
  gUIRadio = document.getElementById("showuiradio");
  gCustomizeDeck = document.getElementById("customiseDeck");
  gPasswordField = document.getElementById("newPassword");
  gPwdRadio = document.getElementById("readpwdradio");
  gPrintRadio = document.getElementById("printSettingsradio");
  gPrintPageCheck = document.getElementById("enable.print.page");
  gSaveBtn = document.getElementById("save.txt");
  gSaveFilepicker = document.getElementById("save.filepicker");
  gSaveLoc = document.getElementById("save.location");
  gResetBtn = document.getElementById("reset.txt");
  gAddOns = document.getElementById("enable.addons");
  gFileUpload = document.getElementById("enable.fileupload");
  gUserAgentTb = document.getElementById("useragent.txt");

  // Set up the string bundles
  gSettingsBundle = document.getElementById("settingsDefaults");
  gDefaultsBundle = document.getElementById("bmakioskDefaults");
  gBrandBundle = document.getElementById("brandBundle");

  gPrefService = jslibGetService(PREFSERVICE_CONTRACTID, "nsIPrefService");
  gPref = gPrefService.getBranch(null);


  // Wait for the window to load before prompting, 
  // so we have a window to close!
  if (!opener)
  {
    gLoginCount = 0;
    setTimeout(login, 500);
  }
    else
  {
    fillPwdRadio();
    var p = window.arguments[0];
    if (p) gPasswordField.value = p;
  }

  gDefaultPage = gDefaultsBundle.getString("defaultHomepage");

  // Browser tab
  startupHomepage();
  startupAttractpage();
  startupRedirectPage();
  // setDefaultBrowserCheckbox();
  fillModeRadio();
  // Filters tab
  setFilterFiles();
  // Sessions tab
  setSessionTimers();
  toggleSessionBox();
  enableCache();
  // Customize tab
  setTabsCheckbox();
  readToolbarSettings();
  fillUIRadio();
  fillSave();
  fillReset();
  setPrinting();
  fillEdit();
  setAddons();
  setFileUpload();
  readUserAgent();

  addRegistryInfo();

  document.getElementById("prefSession").value = false;

  // not necessary anymore
  // initGlobalPrefsFile();
}

function login ()
{
  var gPS = jslibGetService("@mozilla.org/embedcomp/prompt-service;1", "nsIPromptService");

  pwd = { value: "" };

  if (!gPS.promptPassword(window,
                         gSettingsBundle.getString('login.title'),
                         gSettingsBundle.getString('login.message'),
                         pwd,
                         null,
                         {value: null}))
    return exitAdmin(false);  // dialog cancelled

  var userTyped = pwd.value;

  if (!validatePassword(userTyped)) 
  {
    gLoginCount++;

    if (gLoginCount <= 1) 
    {
      gPS.alert(window, gSettingsBundle.getString('login.title'), gSettingsBundle.getString('login.tryagain'));
      setTimeout(login, 100);
    } 
      else 
    {
      exitAdmin(true, gLoginCount);
    }
  }
    else 
  {
    fillPwdRadio();
    gPasswordField.value = userTyped;
  }

  return JS_LIB_OK;
}

function exitAdmin (failed, numTimes)
{
  // failed = true ; user repeatedly entered wrong password
  // failed = false ; user cancelled prompt
  var msg;
  if (failed)
    msg = gSettingsBundle.getFormattedString('login.failed', [numTimes]);
  else
    msg = gSettingsBundle.getString('login.cancelled');

  // gPS.alert(window, gSettingsBundle.getString('login.title'), msg);

  goQuitApp();
}

function fillPwdRadio()
{
  if (savedInRegistry())
    gPwdRadio.selectedItem = document.getElementById('pwd.reg');
  else
    gPwdRadio.selectedItem = document.getElementById('pwd.prefs');

  togglePasswordLocation();
}

function togglePasswordLocation ()
{
    gPasswordField.removeAttribute("disabled");
}

function savePassword()
{
  try
  {
    var saveWhere = gPwdRadio.value;
    // first, save the where pref
    gPref.setBoolPref("bmakiosk.admin.savedinreg", (saveWhere == "reg"));

    var newPass = gPasswordField.value;
    if (newPass == "") return;

    // now, save the pwd, but only if for prefs
    // registry key can not be written via public API in Mozilla

    var p = md5(btoa(newPass));

    if (saveWhere == "prefs") 
    {
      gPref.setCharPref("bmakiosk.admin.login", p);
    }
      else
    {
      const nsIWindowsRegKey = jslibI.nsIWindowsRegKey;
      var key = jslibCreateInstance("@mozilla.org/windows-registry-key;1", "nsIWindowsRegKey");
      key.create(nsIWindowsRegKey.ROOT_KEY_CURRENT_USER, "Software\\OpenKiosk", nsIWindowsRegKey.ACCESS_ALL);

      // jslibPrintMsg("WRITING", p);

      key.writeStringValue("KioskAdmin", p);
      key.close();

    }
  }
    catch (e) { jslibPrint("ERROR", "savePassword", e); }

  return;
}

function setPrinting () 
{
  var useDialog = gPref.getBoolPref("bmakiosk.print.dialog");

  if (useDialog)
    gPrintRadio.selectedItem = document.getElementById('print.dialog');
  else
    gPrintRadio.selectedItem = document.getElementById('print.silent');

  gPrintPageCheck.checked = gPref.getBoolPref("bmakiosk.printfrompage.allow");
}

function savePrinting ()
{
  var useDialog = (gPrintRadio.value == "dialog");
  gPref.setBoolPref("bmakiosk.print.dialog", useDialog);

  gPref.setBoolPref("bmakiosk.printfrompage.allow", gPrintPageCheck.checked);
}

function onAccept()
{
  try
  {
    saveHomepage();
    saveAttractpage();
    saveRedirectPage();
    // saveDefaultBrowser();
    setModePref();
    saveTabsPref();
    saveFilterInfo();
    setUIPref();
    saveSessionTimers();
    saveCachePref();
    saveMemCachePref();
    saveCookiesPref();
    saveToolbarSettings();
    savePassword();
    saveSave();
    resetSave();
    saveGlobalPrefs();
    savePrinting();
    saveEdit();
    enableAddons();
    enableFileUpload();
    setUserAgent();

    goQuitApp();
    toggleBrowserToolbar();
  }
    catch (e) { jslibPrint("ERROR", "onAccept", e); }
}

// Restore the default Kiosk settings in the UI - bug 282
function onRestore()
{
  // Browser tab
  gKioskStartupHomepage.disabled = false;
  gKioskStartupHomepage.value = gDefaultPage;
  gAttractCheckbox.checked = false;
  gRedirectCheckbox.checked = false;
  gKioskAttractHomepage.value = "";
  gKioskRedirectPage.value = "";
  // gBrowserCheckbox.checked = false;
  gModeRadio.selectedItem = document.getElementById('mode.full');

  // Filters tab
  gFilterCheck.checked = false;
  gFilterFileField.value = gSettingsBundle.getString('filter.nofile');
  gFilterFileField.disabled = true;
  // gJSFilterFileField.value = gSettingsBundle.getString('filter.nofile');
  // gJSFilterFileField.disabled = false;
  gJSAllCheck.disabled = false;
  gJSAllCheck.checked = false;
  gFileProtoCheck.checked = false;
  gFTPProtoCheck.checked = false;
  gAboutProtoCheck.checked = false;

  // Sessions tab
  gResetMinutes.value = gDefaultsBundle.getString("defaultResetMins");
  gWarningCheck.checked = true;
  gWarningSeconds.value = gDefaultsBundle.getString("defaultWarningSecs");
  gCacheCheckbox.checked = false;
  gMemCacheCheckbox.checked = false;
  gCookiesCheckbox.checked = false;
  gAUPFileField.value = gSettingsBundle.getString('filter.nofile');
  gAUPFileField.disabled = true;
  gAUPFilterFileField.value = gSettingsBundle.getString('filter.nofile');
  gAUPFilterFileField.disabled = true;
  gAUPCheck.disabled = false;
  gAUPCheck.checked = false;

  // Customize tab
  gTabsCheckbox.checked = false;
  gUIRadio.selectedItem = document.getElementById('ui.show');
  gCustomizeDeck.selectedIndex = 0;
  readToolbarSettings();
  gSaveBtn.value = "";
  gSaveFilepicker.checked = false;
  gSaveLoc.disabled = false;
  gSaveLoc.value = "";
  gResetBtn.value = gPref.getCharPref("bmakiosk.reset.buttontext");

  // Admin tab
  gPasswordField.value = "admin";
  gPwdRadio.selectedItem = document.getElementById('pwd.prefs');
}

function fillUIRadio()
{
  var showUI = getUIPref();
  if (showUI)
    gUIRadio.selectedItem = document.getElementById('ui.show');
  else
    gUIRadio.selectedItem = document.getElementById('ui.hide');
  toggleUIList(showUI);
}

function toggleUIList(show)
{
  // disabling the listbox, so this hack use a deck and and replaces with some text
  // true - show, false - hide
  if (show == true) {
    gCustomizeDeck.selectedIndex = 0;
  }
  else {
    gCustomizeDeck.selectedIndex = 1;
  }
}

function getUIPref()
{
  var show;
  try
  {
    show = gPref.getBoolPref("bmakiosk.ui.show");
  }
  catch (ex) {
    // default
    show = true;
  }
  return show;
}

function setUIPref()
{
  var showval = gUIRadio.value;
  gPref.setBoolPref("bmakiosk.ui.show", (showval == "show"));
}

/* Home Page Handling */

function startupHomepage()
{
  var startpage = getKioskHomePage();
  gKioskStartupHomepage.value = startpage;
  updateHomePageButtons();
}

function saveHomepage ()
{
  var savepage = gKioskStartupHomepage.value;
  jslibDebug("Setting homepage : "+savepage+"\n");
  gPref.setCharPref("bmakiosk.startup.homepage", btoa(savepage));
}

function getKioskHomePage ()
{
  var homepage;
  try {
    homepage = atob(gPref.getCharPref("bmakiosk.startup.homepage"));
  }
  catch (ex) {
    // default
    homepage = gDefaultPage;
  }
  return homepage;
}

function setHomePageToDefaultPage()
{
  gKioskStartupHomepage.value = gDefaultPage;
  updateHomePageButtons();
}

function homepageInputHandler()
{
  updateHomePageButtons();
}

function updateHomePageButtons()
{
  var homepage = gKioskStartupHomepage.value;
  // disable the "default page" button if the default page
  // is already the homepage
  gDefaultPageButton.disabled = (homepage == gDefaultPage);
}

/* End Home Page Handling */

/* Attract Screen Handling */

function startupAttractpage()
{
  var attractOn;
  try
  {
    attractOn = gPref.getBoolPref("bmakiosk.attract.enabled");
    if (attractOn) {
      gAttractCheckbox.checked = true;
    }
    else {
      gAttractCheckbox.checked = false;
    }
  }
  catch (ex) {
    gAttractCheckbox.checked = false;
  }
  var startpage = getAttractPage();
  gKioskAttractHomepage.value = startpage;
  toggleAttract(gAttractCheckbox);
}

function saveAttractpage()
{
  gPref.setBoolPref("bmakiosk.attract.enabled", gAttractCheckbox.checked);
  var savepage = gKioskAttractHomepage.value;
  gPref.setCharPref("bmakiosk.attract.page", savepage);
}

function getAttractPage()
{
  var homepage;
  try {
    homepage = gPref.getCharPref("bmakiosk.attract.page");
  }
  catch (ex) {
    // default
    homepage = "about:blank";
  }
  return homepage;
}

function toggleAttract(enable)
{
  var attractOn = enable.checked;
  gKioskAttractHomepage.disabled = !attractOn;
}

function getRedirectPage()
{
  var homepage;
  try {
    homepage = gPref.getCharPref("bmakiosk.redirect.page");
  }
  catch (e) {
    homepage = "";
  }
  return homepage;
}

function startupRedirectPage()
{
  var redirectOn;
  try
  {
    redirectOn = gPref.getBoolPref("bmakiosk.redirect.enabled");
    if (redirectOn) {
      gRedirectCheckbox.checked = true;
    }
    else {
      gRedirectCheckbox.checked = false;
    }
  }
  catch (e) {
    gRedirectCheckbox.checked = false;
  }
  var startpage = getRedirectPage();
  gKioskRedirectPage.value = startpage;
  toggleRedirectPage(gRedirectCheckbox);
}

function saveRedirectPage()
{
  gPref.setBoolPref("bmakiosk.redirect.enabled", gRedirectCheckbox.checked);
  var savepage = gKioskRedirectPage.value;
  gPref.setCharPref("bmakiosk.redirect.page", savepage);
}

function toggleRedirectPage(enable)
{
  var redirectOn = enable.checked;
  gKioskRedirectPage.disabled = !redirectOn;
}

/* End Attract Screen Handling */

/* Default Browser handling */

function setDefaultBrowserCheckbox()
{
  return;

  var bCheckbox = gBrowserCheckbox;
  var chromeURL;
  try
  {
    chromeURL = gPref.getCharPref("browser.chromeURL");
    if (chromeURL != G_DEFAULT_KIOSK_CHROME_URL) {
      bCheckbox.checked = false;
    }
    else {
      bCheckbox.checked = true;
    }
  }
  catch (ex) {
    bCheckbox.checked = false;
  }
}

function saveDefaultBrowser()
{
  var bCheckbox = gBrowserCheckbox;
  var chromeURL;

  if (bCheckbox.checked)
    chromeURL = G_DEFAULT_KIOSK_CHROME_URL;
  else
    chromeURL = G_DEFAULT_MOZILLA_CHROME_URL;

  gPref.setCharPref("browser.chromeURL", chromeURL);
}

/* Mode radiobox */

function fillModeRadio()
{
  // full screen or titlebar mode?
  var mode;
  try { mode = gPref.getCharPref("bmakiosk.browser.mode"); }
  catch (ex) { mode = "full"; }
  if (mode == "title")
    gModeRadio.selectedItem = document.getElementById('mode.title');
  else
    gModeRadio.selectedItem = document.getElementById('mode.full');
}

function setModePref()
{
  var mode = gModeRadio.value;
  gPref.setCharPref("bmakiosk.browser.mode", mode);
}

/* Tabs handling */

function setTabsCheckbox()
{
  var tabsOn;
  try
  {
    tabsOn = gPref.getBoolPref("bmakiosk.tabs.enabled");
    if (tabsOn) {
      gTabsCheckbox.checked = true;
    }
    else {
      gTabsCheckbox.checked = false;
    }
  }
  catch (ex) {
    gTabsCheckbox.checked = false;
  }
}

function saveTabsPref()
{
  gPref.setBoolPref("bmakiosk.tabs.enabled", gTabsCheckbox.checked);
}

/* Filter enabling and file picking */

function toggleFilters(enable)
{
/******
  var contentFilterOn = enable.checked;
  gFilterFileField.disabled = gFilterFileButton.disabled = !contentFilterOn;
  gJSFilterFileField.disabled = gJSFilterFileButton.disabled = gJSAllCheck.disabled = contentFilterOn;
******/
}

function toggleAUPFilter()
{
  var AUPFilterOn = gAUPCheck.checked;
  gAUPFileField.disabled = gAUPFilterFileField.disabled = gAUPFileButton.disabled = gAUPFilterFileButton.disabled = !AUPFilterOn;
}

function setFilterFiles()
{
  var nofile = gSettingsBundle.getString('filter.nofile');
  // Filter enabled?
  var filterOn;
  try { filterOn = gPref.getBoolPref("bmakiosk.filter.enabled"); }
  catch (ex) { filterOn = false; }
  gFilterCheck.checked = filterOn;
  toggleFilters(gFilterCheck);

  var filterFile;
  try
  {
    filterFile = gPref.getCharPref("bmakiosk.filter.filepath");
    gFilterFileField.value = filterFile;
  }
    catch (e) { gFilterFileField.value = nofile; }

/********
  // now JS filter
  try
  {
    filterFile = gPref.getCharPref("bmakiosk.jsfilter.filepath");
    gJSFilterFileField.value = filterFile;
  }
    catch (e) { gJSFilterFileField.value = nofile; }
********/

  var jsallOn;
  try { jsallOn = gPref.getBoolPref("bmakiosk.jsall.enabled"); }
  catch (ex) { jsallOn = false; }
  gJSAllCheck.checked = jsallOn;

  var fileProtoOn;
  try { fileProtoOn = gPref.getBoolPref("bmakiosk.protocols.file"); }
  catch (e) { fileProtoOn = false; }
  gFileProtoCheck.checked = fileProtoOn;

  var ftpProtoOn;
  try { ftpProtoOn = gPref.getBoolPref("bmakiosk.protocols.ftp"); }
  catch (e) { ftpProtoOn = false; }
  gFTPProtoCheck.checked = ftpProtoOn;

  var aboutProtoOn;
  try { aboutProtoOn = gPref.getBoolPref("bmakiosk.protocols.about"); }
  catch (e) { aboutProtoOn = false; }
  gAboutProtoCheck.checked = aboutProtoOn;


  // now AUP filters
  try
  {
    filterFile = gPref.getCharPref("bmakiosk.aup.filepath");
    gAUPFileField.value = filterFile;
  }
  catch (ex) {
    gAUPFileField.value = nofile;
  }
  try
  {
    filterFile = gPref.getCharPref("bmakiosk.aup.filterpath");
    gAUPFilterFileField.value = filterFile;
  }
  catch (ex) {
    gAUPFilterFileField.value = nofile;
  }
  var aupOn;
  try { aupOn = gPref.getBoolPref("bmakiosk.aup.enabled"); }
  catch (ex) { aupOn = false; }
  gAUPCheck.checked = aupOn;
  toggleAUPFilter();
}

function saveFilterInfo()
{
  gPref.setBoolPref("bmakiosk.filter.enabled", gFilterCheck.checked);
  gPref.setCharPref("bmakiosk.filter.filepath", gFilterFileField.value);
  // gPref.setCharPref("bmakiosk.jsfilter.filepath", gJSFilterFileField.value);
  gPref.setBoolPref("bmakiosk.jsall.enabled", gJSAllCheck.checked);

  // set global javascript pref
  gPref.setBoolPref("javascript.enabled", gJSAllCheck.checked);

  gPref.setBoolPref("bmakiosk.protocols.file", gFileProtoCheck.checked);
  gPref.setBoolPref("bmakiosk.protocols.ftp", gFTPProtoCheck.checked);
  gPref.setBoolPref("bmakiosk.protocols.about", gAboutProtoCheck.checked);

  gPref.setCharPref("bmakiosk.aup.filepath", gAUPFileField.value);
  gPref.setCharPref("bmakiosk.aup.filterpath", gAUPFilterFileField.value);
  gPref.setBoolPref("bmakiosk.aup.enabled", gAUPCheck.checked);
}

// All files filepicker (with Text and HTML)
function selectFilterFileAll(btn)
{
  var nsIFilePicker = Components.interfaces.nsIFilePicker;
  var fp = jslibCreateInstance("@mozilla.org/filepicker;1", "nsIFilePicker");

  var title = gSettingsBundle.getString('fp.title');
  fp.init(window, title, nsIFilePicker.modeOpen);
  fp.appendFilters(nsIFilePicker.filterAll | nsIFilePicker.filterHTML | nsIFilePicker.filterText);

  // Get current filter file folder, if there is one.
  // We can set that to be the initial folder so that users 
  // can maintain their signatures better.
  //var sigFolder = GetSigFolder();
  //if (sigFolder)
  //   fp.displayDirectory = sigFolder;

  var ret = fp.show();

  if (ret == nsIFilePicker.returnOK) document.getElementById(btn.getAttribute("name")).value = fp.file.path;
}

function selectFilterFile(btn)
{
  var nsIFilePicker = Components.interfaces.nsIFilePicker;
  var fp = jslibCreateInstance("@mozilla.org/filepicker;1", "nsIFilePicker");

  var title = gSettingsBundle.getString('fp.title');
  var mode = btn.label == "Save" ? nsIFilePicker.modeSave : nsIFilePicker.modeOpen;

  fp.init(window, title, mode);
  fp.appendFilters(nsIFilePicker.filterText);

  if (mode == nsIFilePicker.modeSave) fp.defaultString = "Allowed.txt";

  // Get current filter file folder, if there is one.
  // We can set that to be the initial folder so that users 
  // can maintain their signatures better.
  //var sigFolder = GetSigFolder();
  //if (sigFolder)
  //   fp.displayDirectory = sigFolder;

  var ret = fp.show();

  var p = fp.file.path;
  if (ret == nsIFilePicker.returnReplace) 
  {
    document.getElementById(btn.getAttribute("name")).value = p;
    btn.label = "Choose Another File";
    writeChangesToFile(p);
  }

  if (ret == nsIFilePicker.returnOK)
  {
    document.getElementById(btn.getAttribute("name")).value = p;
    fillEdit(p);
  }
}

function selectSaveDirectory(btn)
{
  var nsIFilePicker = Components.interfaces.nsIFilePicker;
  var fp = jslibCreateInstance("@mozilla.org/filepicker;1", "nsIFilePicker");

  var title = gSettingsBundle.getString('fpdir.title');
  fp.init(window, title, nsIFilePicker.modeGetFolder);

  var ret = fp.show();
  if (ret == nsIFilePicker.returnOK) {
    document.getElementById(btn.getAttribute("name")).value = fp.file.path;
  }
}

function fillReset ()
{
  var resetBtnText;

  try
  {
    resetBtnText = gPref.getCharPref("bmakiosk.reset.buttontext");
  }
    catch (e) { resetBtnText = ""; }

  gResetBtn.value = resetBtnText;
}

function onEditChangeButtonLabel ()
{
  var nofile = gSettingsBundle.getString('filter.nofile');

  if (gFilterFileField.value == "" || gFilterFileField.value == nofile) gFilterFileButton.label = "Save";
}

function fillEdit (aFilePath)
{
try
{
  var rv;
  var nofile = gSettingsBundle.getString('filter.nofile');

  if (!aFilePath && !gFilterFileEdit.value)
  {
    include(jslib_chromefile);
    var cf = new ChromeFile("chrome://bmakiosk/content/allowed-sample.txt");
    cf.open();
    rv = cf.read();
    cf.close();
  }

  if (gFilterFileField.value != nofile)
  {
    var path = aFilePath || gFilterFileField.value;

    var f = new File(path);

    if (f.exists())
    {
      f.open("r");
      rv = f.read();
      f.close();
    }
      else
    {
      if (aFilePath) 
      {
        writeChangesToFile(aFilePath);
        return;
      }
      gFilterFileField.value = nofile;
    }
  }

  gFilterFileEdit.value = rv;

  if (gFilterFileField.value == "") gFilterFileField.value = nofile;
}
  catch (e) { jslibPrintMsg("ERROR", "fillEdit", e); }
}

function writeChangesToFile (aFilePath)
{
  f = new File(aFilePath);
  f.open("w");
  f.write(gFilterFileEdit.value);
  f.close();
}

function saveEdit ()
{
  var path = gFilterFileField.value;

  var f = new File(path);

  if (f.exists())
  {
    f.open("w");
    f.write(gFilterFileEdit.value);
    f.close();
  }
}

function fillSave()
{
  var saveBtnText, saveLoc, saveFilepicker;

  try
  {
    saveBtnText = gPref.getCharPref("bmakiosk.save.buttontext");
  }
    catch (e) { saveBtnText = ""; }

  try
  {
    // saveLoc = gPref.getCharPref("bmakiosk.save.location");
    saveLoc = gPref.getCharPref("browser.download.dir");
  }
    catch (e) { saveLoc = ""; }

  gSaveBtn.value = saveBtnText;
  gSaveLoc.value = saveLoc;
  
  try
  {
    saveFilepicker = gPref.getBoolPref("bmakiosk.save.usefilepicker");
  }
    catch (e) { saveFilepicker = false; }

  gSaveFilepicker.checked = saveFilepicker; 
  toggleSavePath(gSaveFilepicker);
}

function toggleSavePath(fpcheck)
{
  document.getElementById("saveloc.button").disabled = gSaveLoc.disabled = fpcheck.checked;
}

function enableAddons()
{
  gPref.setBoolPref("bmakiosk.xpinstall.enabled", gAddOns.checked); 
  gPref.setBoolPref("xpinstall.enabled", gAddOns.checked); 
}

function enableFileUpload()
{
  gPref.setBoolPref("bmakiosk.fileupload.enabled", gFileUpload.checked); 
}

/* Reset button text */
function resetSave()
{
  gPref.setCharPref("bmakiosk.reset.buttontext", gResetBtn.value);
}

/* Save button text and save location */
function saveSave()
{
  gPref.setCharPref("bmakiosk.save.buttontext", gSaveBtn.value); 
  gPref.setBoolPref("bmakiosk.save.usefilepicker", gSaveFilepicker.checked); 
  // gPref.setCharPref("bmakiosk.save.location", gSaveLoc.value);  
  gPref.setCharPref("browser.download.dir", gSaveLoc.value);  
}

/* Session timers */
function setSessionTimers()
{
  var a, b, c;

  // Reset timer
  try
  {
    a = gPref.getIntPref("bmakiosk.reset.timer");
    gResetMinutes.value = a;
  }
  catch (ex) {
    gResetMinutes.value = gDefaultsBundle.getString("defaultResetMins");
  }

  // Warning enabled?
  try
  {
    b = gPref.getBoolPref("bmakiosk.reset.warningenabled");
  }
  catch (ex) {
    b = true;
  }

  // Warning coundown seconds
  try
  {
    c = gPref.getIntPref("bmakiosk.reset.warningtimer");
    gWarningSeconds.value = c;
  }
  catch (ex) {
    gWarningSeconds.value = gDefaultsBundle.getString("defaultWarningSecs");
  }

  gWarningCheck.checked = b;
  toggleSecondsTextbox(gWarningCheck)
}

function saveSessionTimers()
{
  // need to factor in error checking for non-numeric and blank entries
  gPref.setIntPref("bmakiosk.reset.timer", gResetMinutes.value);
  gPref.setBoolPref("bmakiosk.reset.warningenabled", gWarningCheck.checked);
  gPref.setIntPref("bmakiosk.reset.warningtimer", gWarningSeconds.value);
}

function toggleSecondsTextbox(warning)
{
  gWarningSeconds.disabled = !warning.checked;
}

function toggleSessionBox (aEl)
{
  var checked;

  if (aEl) 
    checked = aEl.checked;
  else
  {
    checked = gPref.getBoolPref("bmakiosk.reset.timer.on");
    gResetEnableCheck.checked = checked;
  }
      
  var el = document.getElementById("session-reset-gb");

  if (!checked)
    el.setAttribute("disabled", true);
  else
    el.removeAttribute("disabled");

  if (aEl)
  {
    gPref.setBoolPref("bmakiosk.reset.timer.on", checked);

    if (opener && opener.startSessionListener) 
    {
      if (checked) opener.startSessionListener();
      else opener.clearSession();
    }
  }
}

function enableCache()
{
  var c, d, e;
  try
  {
    c = gPref.getBoolPref("bmakiosk.cache.diskenabled");
    d = gPref.getBoolPref("bmakiosk.cache.memenabled");
    e = gPref.getBoolPref("bmakiosk.cookies.enabled");
  }
    catch (e) { c = false; }

  gCacheCheckbox.checked = c;
  gMemCacheCheckbox.checked = d;
  gCookiesCheckbox.checked = e;
}

function saveCachePref()
{
  gPref.setBoolPref("bmakiosk.cache.diskenabled", gCacheCheckbox.checked);
}

function saveMemCachePref()
{
  gPref.setBoolPref("bmakiosk.cache.memenabled", gMemCacheCheckbox.checked);
}

function saveCookiesPref()
{
  gPref.setBoolPref("bmakiosk.cookies.enabled", gCookiesCheckbox.checked);
}

/* New win with sample filter file */
function goSampleFilters(evt)
{
  var href = evt.target.getAttribute("href");
  var title = evt.target.getAttribute("name");
  var prevW = window.openDialog("chrome://bmakiosk/content/filterExample.xul", "_blank", "centerscreen,resizable=yes", title, href);
}

// If we are in the generic version, tell where the registry key is
function addRegistryInfo()
{
  var brand = gBrandBundle.getString("brandname");
  if (brand == "Open") {
    var regItem = document.createElement("label");
    var regText = "Registry key : 'HKLM/SOFTWARE/Open Kiosk' - value 'KioskAdmin'";
    regItem.setAttribute("value", regText);
    regItem.setAttribute("style", "font-weight: bold;");
    document.getElementById("passwordBox").insertBefore(regItem, document.getElementById("readwhereEntry"));
  }
}

function readToolbarSettings()
{
  var buttonsArray = [["unified-back-forward-button",    "backforward.label"],
                      ["reload-button",                  "reload.label"],
                      ["stop-button",                    "stop.label"],
                      ["home-button",                    "home.label"],
                      ["urlbar-container",               "urlbar.label"],
                      ["print-button",                   "print.label"],
                      ["zoom-control",                   "zoom.label"],
                      ["save-container",                 "save.label"],
                      ["reset-container",                "logout.label"], 
                      ["navigator-throbber",             "throbber.label"],
                      /*["status-bar",                     "statusbar.label"]*/];

  var chosen = document.getElementById("prefButtons").value;
  var list = document.getElementById("bmaOverlaysList");

  for each (item in buttonsArray)
  {
    var val = gSettingsBundle.getString(item[1]);
    var id  = item[0]; 
    var checkbox = list.appendItem(val, id);
    checkbox.setAttribute("type", "checkbox");

    checkbox.id = id;
    checkbox.value = val;

    var checked = false;
    if (item[0] == 'status-bar')
      checked = document.getElementById("prefStatusbar").value;
    else      
      checked = chosen.indexOf(id) != -1;

    checkbox.setAttribute("checked", checked);
  }
}

function saveToolbarSettings()
{
  var statusbar = document.getElementById("prefStatusbar");
  var list = document.getElementById("bmaOverlaysList");
  var len = list.getRowCount();
  var chosen = [], item;

  for (var i = 0; i < len; i++)
  {
    item = list.getItemAtIndex(i);

    if (item.id == 'status-bar')
    {
      statusbar.id = item.checked;
      statusbar.value = (item.getAttribute("checked") == "true");
    }
      else if (item.getAttribute("checked") == "true")
      chosen.push(item.id);
  }

  document.getElementById("prefButtons").value = chosen.join(',');
}

function setEnableTabbedBrowsing (aEl)
{
  try
  {
    var num = 3;

    if (!aEl.checked) num = 1;
    
    gPref.setIntPref("browser.link.open_newwindow", num);
    gPref.setIntPref("browser.link.open_external", num);
  }
    catch (e) { jslibError(e); }
}

// this is for when kiosk is 
// installed globally and as admin
// so settings can be set a single time --pete
function saveGlobalPrefs ()
{
  if (!gGlobalPrefsFile) return;

  try
  {
    // alert(gGlobalAddPrefsFile.leafName);

    gPrefService.savePrefFile(gGlobalAddPrefsFile.nsIFile);

    var b = gPrefService.getBranch("bmakiosk.");

    b.deleteBranch("");

    gGlobalAddPrefsFile.open();
    var c = gGlobalAddPrefsFile.read();
    gGlobalAddPrefsFile.close();

    c = c.replace(/user_pref/g, "pref");

    gGlobalPrefsFile.open("a");
    gGlobalPrefsFile.write(c); 
    gGlobalPrefsFile.close();

    // resetBranch(in string aStartingAt);

  }
    catch (e) { jslibPrintError(e); }
}

function initGlobalPrefsFile ()
{
  var du = new DirUtils;

  var f = new File(du.getCurProcDir());

  f.append("extensions");
  f.append("bmakiosk@brooklynmuseum.org");
  f.append("defaults");
  f.append("preferences");

  var d = f.clone();

  d.append("bmakiosk.js");

  d.create();
  gGlobalAddPrefsFile = d;

  f.append("prefs.js");

  if (f.exists()) gGlobalPrefsFile = f;
}

function editPopupSettings ()
{

  var bundlePreferences = document.getElementById("bundle_preferences");
  var params = { blockVisible   : false,
                 sessionVisible : false,
                 allowVisible   : true,
                 prefilledHost  : null,
                 permissionType : "popup",
                 windowTitle    : "Allowed Sites",
                 introText      : "Pop-ups" };

  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                      .getService(Components.interfaces.nsIWindowMediator);

  var existingWindow = wm.getMostRecentWindow("Browser:Permissions");

  if (existingWindow)
  {
    existingWindow.initWithParams(params);
    existingWindow.focus();
  }
  else
    window.openDialog("chrome://browser/content/preferences/permissions.xul",
                      "_blank", "resizable,dialog=no,centerscreen", params);
}

/**
 * Displays fine-grained, per-site preferences for cookies.
 */
function showCookieExceptions ()
{
  var params = { blockVisible   : true,
                 sessionVisible : true,
                 allowVisible   : true,
                 prefilledHost  : "",
                 permissionType : "cookie",
                 windowTitle    : "Exceptions",
                 introText      : "Cookies" };

  window.openDialog( "chrome://browser/content/preferences/permissions.xul", "_blank", "resizable,dialog=no,centerscreen",params);
}

function toggleBrowserToolbar ()
{
  if (!opener || !opener.readToolbarSettings) return;

  opener.readToolbarSettings();
}

function setAddons ()
{
  gAddOns.checked = gPref.getBoolPref("bmakiosk.xpinstall.enabled");
}

function setFileUpload ()
{
  gFileUpload.checked = gPref.getBoolPref("bmakiosk.fileupload.enabled");
}

function readUserAgent ()
{
  gUserAgentTb.value = navigator.userAgent;
}

function setUserAgent ()
{
  if (!gUserAgentTb.value)
    resetUserAgent();
  else
    gPref.setCharPref("general.useragent.override", gUserAgentTb.value);
}

function resetUserAgent ()
{
  gPref.clearUserPref("general.useragent.override");
  readUserAgent();
}


