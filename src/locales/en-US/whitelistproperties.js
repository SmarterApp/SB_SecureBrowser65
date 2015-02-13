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
Contributor(s): MozdevGroup Inc.
*/

// **** kiosk whitelistproperties

var baseUriForFilter = "about:"; //this should match the default.uri

//*** this catches _requested_ popups if false and if the uri is
//*** in the allowed list opens in current window instead.
//*** The popup blocker should be setup also in the mozilla prefs to 
//*** catch most popups. 
//*** Value can be true or false
var allowPopupWindows = "false";
