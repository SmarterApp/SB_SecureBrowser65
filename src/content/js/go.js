// globals
const G_DEFAULT_LAUNCH_FILE = "bmakiosk.xul";
var gFile, gMode, gUrlToLoad;
var gBanner = false;
var gAdmin = false;
var gAbout = false;
var gWinArgs;

function launchKiosk()
{
  try
  {
  gWinArgs = new Object;

  // handle commandline args
  if (!testArgs())  {
    dump("GENERIC KIOSK\n");
    if (loadInCurrentWin()) setTimeout(closeWin, 1000);
    else genericLaunch();
    return;
  }

  var argv = window.arguments.toString().split(' ');
  for (var i=0; i<argv.length; i++) parseArgs(argv[i]);

  // admin and about are exclusive args, and can not be used with others

  if (gAdmin)  
  {
    window.openDialog("chrome://bmakiosk/content/settings.xul", "_blank", "chrome,dialog=no,resizable=no,centerscreen");
    closeWin();
    return;
  }

  if (gAbout)  
  {
    window.openDialog("chrome://bmakiosk/content/aboutDialog.xul", "_blank", "chrome,dialog,dependent=no,centerscreen");
    closeWin();
    return;
  }

  // check for values, set defaults if not found
  if (!gFile)
    gFile = G_DEFAULT_LAUNCH_FILE;
  if (gUrlToLoad) {
    gWinArgs.url = gUrlToLoad;
  }

  var url = "chrome://bmakiosk/content/"+gFile;

  if (!gMode)
    gWinArgs.full = null;
  else if (gMode && gMode != "full")
    gWinArgs.full = false;
  else
    gWinArgs.full = true;  // default
    
  window.openDialog(url, "", "all,chrome,dialog=no", gWinArgs);

  // now close this window
  closeWin(); 

  } catch (e) { alert(e); }
}

function genericLaunch ()
{
  gWinArgs.full = true;
  window.openDialog("chrome://bmakiosk/content/"+G_DEFAULT_LAUNCH_FILE, "", "all,chrome,dialog=no", gWinArgs);
  closeWin();
}

function testArgs ()
{
  if (window.arguments) 
    return true;

  return false;
}

function parseArgs (pArg)
{
  var arg, argvalue;
  if (/=/.test(pArg)) {
    var argArray = pArg.split("=");
    arg = argArray[0];
    argvalue = argArray[1];
  }
  else {
    arg = pArg;
  }

  switch (arg)
  {
    case "file":
      dump("file arg passed with value of"+argvalue+"\n");
      gFile = argvalue;
      break;
    case "mode":
      dump("mode arg passed with value of"+argvalue+"\n");
      gMode = argvalue;
      break;
    case "admin":
      dump("admin arg passed\n");
      gAdmin = true;
      break;
    case "about":
      dump("about arg passed\n");
      gAbout = true;
      break;
    case "banner":
      dump("banner arg passed\n");
      gBanner = true; // currently not used
      break;
    default:  // someones simply trying to pass in a URL
      gUrlToLoad = arg;
      break;
  }
}

function loadInCurrentWin ()
{
  var wm = Components.classes['@mozilla.org/appshell/window-mediator;1']
             .getService(Components.interfaces.nsIWindowMediator);

  var kWindows = wm.getEnumerator("bmakiosk:browser");

  var rv = false;
  while (kWindows.hasMoreElements())
  {
    // Update the home button tooltip.
    var win = kWindows.getNext();
    if (win) 
    {
      // alert(win.location);
      win.focus();
      var theBrowser = win.document.getElementById("content");
      var url = win.getKioskHomePage();
      if (win.useTabs())
      {
        var tabAdded = theBrowser.addTab(url);
        theBrowser.selectedTab = tabAdded;
      } else {
        win.loadPage(url);
      }
      rv = true;
    }
  }

  return rv;
}

function closeWin ()
{
  window.close();
}
