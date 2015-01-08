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
Copyright (C) 2004-2007 Mozdev Group, Inc.  All
Rights Reserved.

Original Author: Jim Massey <massey@stlouis-shopper.com>
Contributor(s): Brian King <brian@mozdevgroup.com>
                Daniel Brooks <daniel@mozdevgroup.com>
*/

var allowed = [];
var gWhiteList;
var gJSWhiteList;
var gAUPWhiteList;

const iosvc = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);


// for the port I disabled the call to this function
// because FF seems to handle all various calls
// to new windows correctly, this func was
// primarily needed for Mozilla lgeacy kiosk --pete
function checkLinkForTarget (event)
{
  try
  {
    var t = event.target;
    var tag = t.tagName.toLowerCase();

    if (!t.parentNode || !t.parentNode.tagName) return true;

    var tagParent = t.parentNode.tagName.toLowerCase();

    var forms = _content.document.getElementsByTagName("form");

    for (var i = 0; i < forms.length; i++)
      forms[i].removeAttribute("target");

    // handle case where link is wrapped around another tag, e.g img
    var linkNode;
    if (tag == "a" || tag == "form")
      linkNode = t;
    else if (tagParent == "a" || tagParent == "form")
      linkNode = t.parentNode;
    else
      return true;

    var protocol = linkNode.protocol;
    if (protocol)
    {
      // if not a valid protocol, bail
      if (!validateProtocol(protocol))
        return false;
    }

    if (linkNode.tagName.toLowerCase() == 'form' &&
        linkNode.hasAttribute("action"))
    {
      // comments below fix bug 2172 --pete
      // event.preventDefault();
      linkNode.target = null;
      // linkNode.submit();

      return true;
    }

    var url = linkNode.href;

    // handleAUP(url);

    if (whiteListCheck(url, "url"))
    {
      // Ok, stop the event dead in it's tracks,
      // so a new window is not launched --pete
      event.preventDefault();

      if (linkNode.hasAttribute("target"))
        loadPageInTab(url);
      else
        loadPage(url);

      return true;
    }

    if (!permission.granted)
    {
      var msg = gKioskBundle.getFormattedString('notinwhitelist', [url]);
      statusMessage(msg);

      return false;
    }

  } 
    catch (e) { jslibPrintMsg("ERROR:checkLinkForTarget", e); }

    return true;
}

// This function is used by both content and JS filtering
// type arg is optional, and only passed in for js requests - "js"
function whiteListCheck (requestUrl, type)
{
  // jslibPrintCaller("requestUrl", requestUrl);

  // always return true for home and attract pages
  if (URLIsHomeOrAttract(requestUrl)) return true;

  try
  {
    // tidy up the request URL for compatibility
    var gURIFixup = jslibGetService("@mozilla.org/docshell/urifixup;1", "nsIURIFixup");

    var fixedRequest = gURIFixup.createFixupURI(requestUrl, jslibI.nsIURIFixup.FIXUP_FLAGS_MAKE_ALTERNATE_URI);

    // debugAlert(fixedRequest.host);

    // Let's see if filtering is turned on
    // If off, all content goes through but JS needs to be filtered
    var whiteList;
    var filterOn = document.getElementById("prefURLFiltering").value;
    var jsOn = document.getElementById("prefJSFiltering").value;
    var aupOn = document.getElementById("prefAUPFiltering").value;

    /********************************
    jslibPrintMsg("aupOn", aupOn);
    jslibPrintMsg("filterOn", filterOn);
    jslibPrintMsg("jsOn", jsOn);
    jslibPrintMsg("type", type);
    ********************************/

    // Forking for AUP/JS/Content filter request
    if (type == "aup")
    {
      // no checking necessary if the feature is turned off 
      if (!aupOn)  return true;

      whiteList = gAUPWhiteList;
    }
      else if (type == "js")
    {
      // all JS requests go through when content filtering is enabled
      if (filterOn) return true; 

      // all JS content allowed 
      if (jsOn) return true;

      whiteList = gJSWhiteList;
    }
      else if (type == "url")
    {
      if (!filterOn) return true;

      whiteList = gWhiteList;
    }
      else
    {
      if (!filterOn && !aupOn) return true;

      whiteList = gWhiteList;
    }

    if (fixedRequest.scheme == "javascript") fixedRequest = gBrowser.currentURI;

    toggleGlobalJS(jsOn);

    for each ([uri, mode, jsenabled] in whiteList)
    {
      var enableJS = (jsenabled == "JAVASCRIPT");

      // jslibPrint("uri", uri.spec, "mode", mode);

      if (mode == 'STRICT')
      {
        if (uri.equals(fixedRequest)) 
        {
          toggleGlobalJS(enableJS);

          return true;
        }
      }
        else if (mode == 'ALL')
      {
        if (wildcardMatch(fixedRequest, uri) && pathMatch(fixedRequest, uri))
        {
          // jslibPrint("uri", uri.spec, "mode", mode, "type", type, "fixedRequest", fixedRequest.spec, "enableJS", enableJS, "["+jsenabled+"]");

          toggleGlobalJS(enableJS);
          
          return true;
        }
      }
    }
  } 
    catch (e) { debugAlert("ERROR", e); /*jslibPrintMsg("ERROR:whiteListCheck", e);*/ }

  return false;
}

function URLIsHomeOrAttract (aURL)
{
  var urifixup = Components.classes["@mozilla.org/docshell/urifixup;1"].getService(Components.interfaces.nsIURIFixup);
  var uri = urifixup.createFixupURI(aURL, 0);

  var homepage = atob(document.getElementById("prefHomePage").value) || gDefaultsBundle.getString("defaultHomepage");
  var attractpage = document.getElementById("prefAttractPage").value || "about:blank";

  homepage = urifixup.createFixupURI(homepage, 0);
  attractpage = urifixup.createFixupURI(attractpage, 0);

  return uri.equals(homepage) || uri.equals(attractpage);
}

function wildcardMatch (aRequest, aFilter)
{
  var rv = false;
  try
  {
    // jslibPrintMsg("aRequest.spec", aRequest.spec);
    // jslibPrintMsg("aFilter", aFilter.host);
    // jslibPrintMsg("aRequest.host", aRequest.host);

    if (!aRequest.host)
    {
      // jslibPrintMsg("NO HOST!", aRequest.scheme);

      if (aRequest.scheme == "file") return false;
    }

    var re = aFilter.host.replace(/\./g, '\\.').replace(/\*/g, '.*') + '$'
    rv = new RegExp(re).test(aRequest.host);
  }
    catch (e) {}

  return rv;
}

function pathMatch(aRequest, aFilter)
{
  aRequest = aRequest.path;
  aFilter = aFilter.path;

  var filterLen;
  // Its not enough to check domains for regular URLs
  // e.g allowed[www.mozilla.org/projects/firefox/, ALL];
  filterLen = aFilter.length;
  reqLen = aRequest.len;

  // clean up filter root
  if (aFilter.charAt(filterLen-1) != "/")
  {
    aFilter += "/";
    filterLen = aFilter.length;
  }

  // There is method in the following madness!! - Brian
  choppedReq = aRequest.substring(0, filterLen);
  delimitChar = choppedReq.charAt(filterLen-1);

  if (delimitChar != "")
  {
    if (delimitChar == "/" && choppedReq == aFilter)
      return true;
  }
  else
  {
    choppedReq += "/";
  }

  if (choppedReq == aFilter)
    return true;

  return false;
}

function parseWhitelist (type)
{
  var wlistFile = getWhitelistFile(type);

  if (wlistFile == null) return;

  var fis = jslibCreateInstance("@mozilla.org/network/file-input-stream;1", "nsIFileInputStream");

  fis.init(wlistFile, -1, -1, false);

  var lis = fis.QueryInterface(Components.interfaces.nsILineInputStream);
  var line = { value: "" };
  var more = false;
  var all = [];
  var strict = [];
  var whitelist = [];

  do
  {
    more = lis.readLine(line);

    var text = line.value;

    if (text && !/#/.test(text)) 
    {
      if (/STRICT/.test(text))
        strict.push(parseLine(text));
      else
        all.push(parseLine(text));
    }
  }
    while (more);

  fis.close();

  whitelist = strict.concat(all);

  if (type == "js")
    gJSWhiteList = whitelist;
  else if (type == "aup")
    gAUPWhiteList = whitelist;
  else
    gWhiteList = whitelist;
}

// Example formats for line :
// allowed[www.brooklynmuseum.org/programs/, ALL, JAVASCRIPT];
// allowed[www.mozilla.org/projects/firefox/, STRICT, NOJAVASCRIPT];
function parseLine (line)
{
  var rv = [];

  if (!line) return rv;

  try
  {
    var match = /\[([^\\]*)\]/.exec(line);

    if (!match) return rv;

    let [url, mode, jsenabled] = match[1].split(',');

    var uri = gURIFixup.createFixupURI(url, jslibI.nsIURIFixup.FIXUP_FLAGS_MAKE_ALTERNATE_URI);

    if (!jsenabled) jsenabled = "JAVASCRIPT";

    // rv = [uri, mode.replace(' ', ''), stripWhiteSpace(jsenabled)];

    rv = [uri, stripWhiteSpace(mode), stripWhiteSpace(jsenabled)];
  } 
    catch (e) { jslibPrintMsg("ERROR:parseLine", e); }
 
  return rv;
}

function getWhitelistFile (type)
{
  try
  {
    var filterPref = document.getElementById('pref'+ (type || 'URL').toUpperCase() +'FilterFile').value;

    if (filterPref && filterPref != "[No file specified]")
    {
      var filterFile = jslibCreateInstance("@mozilla.org/file/local;1", "nsILocalFile");
      filterFile.initWithPath(filterPref);

      if (filterFile.exists()) return filterFile;
    }
  }
    catch (e) { jslibError(e); }

  return null;
}

function getFirstEntry(list)
{
  if (list) return list[0][0].spec.replace('*.', '');

  return "about:blank";
}

function dumpWhitelist(list)
{
  if (!list) dump("whitelist empty\n");

  for each ([uri, mode] in list)
  {
    dump([uri.spec, mode].toSource() +"\n");
  }
}

function validateProtocol (aProtocol)
{
  var scheme = aProtocol;
  var rv = true;

  if (scheme == "wyciwyg" || scheme == "wyciwyg:") return true;
  if (scheme == "javascript" || scheme == "javascript:") return true;

  var filter = gDefaultsBundle.getString("uriAllowedSchemes");

  if (gPref.getBoolPref("protocols.file")) filter += "|file:";

  if (gPref.getBoolPref("protocols.ftp")) filter += "|ftp:";

  if (gPref.getBoolPref("protocols.about")) filter += "|about:";

  if ((filter.search(scheme)) == -1) rv = false;

  // jslibDebug("Leaving validateProtocol with value of "+rv);

  return rv;
}

// See bug 356 - a whitelist for JS when filters are turned off
// If the request coming through is in the list, return true
function JSwhiteListCheck(requestUrl)
{
  // Let's run it through the same filter as content, with 1 extra argument
  return whiteListCheck(requestUrl, "js");
}
