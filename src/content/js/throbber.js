/*
The contents of this file are subject to the Mozilla Public
License Version 1.1 (the "License"); you may not use this file
except in compliance with the License. You may obtain a copy of
the License at http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS
IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
implied. See the License for the specific language governing
rights and limitations under the License.

The Original Code copyright (C) Jim Massey.
The Initial Developer of the Original Code is Jim Massey

Original Author: Jim Massey <massey@stlouis-shopper.com>
*/

// **** Reads and sets the value for the throbber element ****
 
//window.addEventListener("unload", meter_on, true);
//window.addEventListener("load", meter_off, true);

// XX Note that if you ever use this , it badly breaks the kiosk in some circumstances
// e.g. on startup it goes into a load loop on the homepage, if it is not in the filter list
// NEEDS TO BE FIXED, BUT THE SPEC DOESN"T ASK FOR A THROBBER so we are leaving out.
function meter_on(event) 
{
  //dump("****New Meter ON\n");
  try {
    allowedListFilter(event);
  } catch (e) {
    //alert("***in throober.js White List Failed because: " + e);
  }

  try {
    var throbber_active = document.getElementById("bmakioskDefaults").getString("throbberActive");
    //dump(throbber_active + "****New Meter IMAGE\n");
    if (document.getElementById("progress") != null) {
      meter = document.getElementById("progress");
      meter.setAttribute("mode", "undetermined");
    }
    throbber = document.getElementById("throbber");
    throbber.setAttribute("src", throbber_active);
    //alert("****New Meter ON has finnished!\n");
  } catch (e) {
    //alert("meter_on failed because: " + e);
  }
}

function meter_off() 
{
  //dump("****New Meter OFF\n");
  try {
    var throbber_image = document.getElementById("bmakioskDefaults").getString("throbberInactive");
    if (document.getElementById("progress") != null) {
      meter = document.getElementById("progress");
      meter.setAttribute("mode", "normal");
    }
    throbber = document.getElementById("throbber");
    throbber.setAttribute("src", throbber_image);
 } catch (e) {
   //alert("meter_off failed because");
 }
}
