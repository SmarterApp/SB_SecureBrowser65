#!/bin/sh

##### -*- Mode: shellscript; tab-width: 2;
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is Mozdev Group, Inc. code.
# The Initial Developer of the Original Code is Pete Collins.
#
# Portions created by Mozdev Group, Inc. are
# Copyright (C) 2005 Mozdev Group, Inc..  
# All Rights Reserved.
#
# Original Author: Pete Collins <pete@mozdevgroup.com>
# Contributor(s): __________
#
#####


export MOZILLA_FIVE_HOME=.
export  LD_LIBRARY_PATH=.:./plugins
export     LIBRARY_PATH=.:./components
export       SHLIB_PATH=.
export          LIBPATH=.
export       ADDON_PATH=.
export      MOZ_PROGRAM=./xpcshell
export      MOZ_TOOLKIT=
export        moz_debug=0
export      moz_debugger=

top_srcdir=`pwd`;
moz_dist=$MOZ_DIST;
if [ XPCSHELL ]; then
  xpc_bin=$XPCSHELL;
else
  xpc_bin=$MOZ_DIST/xpcshell;
fi

grep \.js jar.mn | cut -f2 -d"(" | sed -e's:)::g' > js_files.mn;


if [ -f $top_srcdir/js_files.mn ]; then

  if [ -x $xpc_bin ]; then

    if [ "$OS_ARCH" = "WINNT" ]; then
      PATH=$PATH:$moz_dist;
      for f in `cat $top_srcdir/js_files.mn`
        do
          if [ -f $top_srcdir/$f ]; then
            xpcshell -w -s $f;
          else
            echo File: [$top_srcdir/$f] not found;
          fi
        done
        echo;
        echo js files tested:
        cat $top_srcdir/js_files.mn;
        rm -f $top_srcdir/js_files.mn;
    else
      cd $moz_dist;
      for f in `cat $top_srcdir/js_files.mn`
        do
          if [ -f $top_srcdir/$f ]; then
          ./run-mozilla.sh  ./xpcshell -w -s $top_srcdir/$f;
          else
            echo File: [$top_srcdir/$f] not found;
          fi
        done
        echo;
        echo js files tested:
        cat $top_srcdir/js_files.mn;
        rm -f $top_srcdir/js_files.mn;
    fi
  fi
fi

cd $top_srcdir;

exit;

