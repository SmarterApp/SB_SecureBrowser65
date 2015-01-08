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

var gUrlbarListArray = new Array();

function checkForEmptyList()
{
  var urlbar = document.getElementById("kioskurlbar");
  var numurls = gUrlbarListArray.length;
  if (numurls == 0)  {
    uribarDropDown(gKioskBundle.getString("urlbar.empty"));
  }
}

function uriListAccess() 
{
  // Reads the value of the menulist item clicked on
  // and calls loadPage with the uri.
  var temp = document.getElementById("kioskurlbar");
  var site = temp.selectedItem.value
  loadPage(site);
}

function listGotFocus() 
{
  var tempList = document.getElementById("kioskurlbar");
  //var tempout = tempList.getAttribute("focused");
  //dump("LIST GOT FOCUS **** :" + tempout + "***\n");
  if (tempList.getAttribute("focused")) {
    uriListInputField();
  }
}

// **** Loads the uri entered into the uribar inputField ****
function uriListInputField() 
{
  var temp = document.getElementById("kioskurlbar");
  var site = temp.inputField.value;

  if (site != "") {
    if (validateProtocol(site.substring(0, site.indexOf(":")+1)))  {
      loadPage(site);
      // Add to urlbar history
      uribarDropDown(site);
    }
  }
}

function enableUriInputField() 
{
  var urlbar = document.getElementById("kioskurlbar");
  urlbar.setAttribute("disabled", false);
  urlbar.setAttribute("editable", true);
}

function disableUriInputField() 
{
  var urlbar = document.getElementById("kioskurlbar");
  urlbar.setAttribute("disabled", true);
  urlbar.setAttribute("editable", false);
}

// *** appends a uri to the dropdown list *****
function uribarDropDown(loadedUrl) 
{
  var urlbar = document.getElementById("kioskurlbar");
  var numurls = gUrlbarListArray.length;

  // remove empty placeholder if necessary
  if (numurls == 1 && gUrlbarListArray[0] == gKioskBundle.getString("urlbar.empty"))
    var removedItem = urlbar.removeItemAt(0);

  for (var i = 0; i <=  numurls; i++) {
    if (gUrlbarListArray[i] == loadedUrl) { 
      return; 
    }
  } 
  var newItem = urlbar.appendItem(loadedUrl, loadedUrl);
  if (loadedUrl == gKioskBundle.getString("urlbar.empty"))  {
    newItem.setAttribute("disabled", true);
  }
  gUrlbarListArray.push(loadedUrl);
  //dump("** --" + gUrlbarListArray.length + "--*** report\n");
}

// **** Dump the contents of the uribar dropdowm *******
function uribarListDump() 
{
  var uribarDropDownCount = gUrlbarListArray.length;
  var ed = document.getElementById("kioskurlbar");
  var item;
  while ( uribarDropDownCount >= 1 ) {
    item = ed.removeItemAt(0);
    uribarDropDownCount--;
  }
  gUrlbarListArray.length = 0;
  urlbarResetDefaultLabel();
}

function urlbarResetDefaultLabel() 
{
  var ed = document.getElementById("kioskurlbar");
  ed.inputField.value = ed.getAttribute("valueDefault");
}
