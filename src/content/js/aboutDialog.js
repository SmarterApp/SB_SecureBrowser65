// Globals
var gKioskBundle;

function onloadAbout ()
{
  // Set up the string bundle
  gKioskBundle = document.getElementById("bundle_bmakiosk");

  document.getElementById("kioskUserAgent").value = navigator.userAgent;
}

function goLink (evt)
{
  var url = evt.target.getAttribute("url");

  if (url)
  {
    if (opener) opener.loadURI(url);

    self.close();
  }
}

function putVersion ()
{
  var ver = getChromePackageValue("packageVersion", "bmakiosk");

  if (ver) 
  {
    getEl("kioskVersion").value = gKioskBundle.getString('about.version');
    getEl("kioskVersionNum").value = ver;
  } 
    else 
  {
    getEl("kioskVersion").value = gKioskBundle.getString('about.noVersion');
  }
}

// Get some extension information from chrome
// e.g. authorURL, name, packageVersion ...
function getChromePackageValue (property, packageName)
{
  var cValue;
  try 
  {
    var rdfSvc = jslibGetService("@mozilla.org/rdf/rdf-service;1", "nsIRDFService");
    var rdfDS = rdfSvc.GetDataSource("rdf:chrome");
    var resSelf = rdfSvc.GetResource("urn:mozilla:package:"+packageName);
    var resProp = rdfSvc.GetResource("http://www.mozilla.org/rdf/chrome#"+property);
    var resTarget = rdfDS.GetTarget(resSelf, resProp, true);
    var literal = resTarget.QueryInterface(Components.interfaces.nsIRDFLiteral);
    cValue = literal.Value;
  } 
    catch (e) 
  {
    cValue = null;
  }

  return cValue;
} 

function getEl (aEl) { return document.getElementById(aEl); }

