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

var wObject;

// main string bundle
var gKioskBundle;

function warningOnLoad ()
{
try
{
  // Set up the string bundle
  gKioskBundle = document.getElementById("bundle_bmakiosk");

  // set the window title
  wObject = window.arguments[0];
  document.title = wObject.title;

  var messageText = wObject.message;
  var descriptionNode = document.getElementById("message1");
  var text = document.createTextNode(messageText);
  descriptionNode.appendChild(text);

  // start the countdown timer
  opener.startCountdown(window);

  // Set the button text
  document.documentElement.getButton("accept").label = gKioskBundle.getString("continue.button");

  // getAttention();
}
  catch (e) { alert(e); }
}

function onAccept ()
{
  wObject.reset = false;
  window.close();
}

