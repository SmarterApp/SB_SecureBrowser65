#!/bin/bash

# Installs shortcut to the browser on the desktop

# First, lets figure out where the browser is actually installed
# This cumbersome method handles cases where the script is run
# by clicking, by going to terminal, by using a sym-link to the script
# etc.

# SCRIPT_PATH="${BASH_SOURCE[0]}";

SCRIPT_PATH="$0";

if([ -h "${SCRIPT_PATH}" ]) then
   while([ -h "${SCRIPT_PATH}" ]) do SCRIPT_PATH=`readlink "${SCRIPT_PATH}"`; done
fi
pushd . > /dev/null
cd `dirname ${SCRIPT_PATH}` > /dev/null
SCRIPT_PATH=`pwd`
popd > /dev/null

# Now we know where we are installed. Lets create the shortcut.
INSTALLDIR=${SCRIPT_PATH};
DESKTOP=~/Desktop;

SHORTCUT=%APP_NAME%.desktop;

touch ${DESKTOP}/${SHORTCUT};
chmod a+rx ${DESKTOP}/${SHORTCUT};

echo "[Desktop Entry]"                            > ${DESKTOP}/${SHORTCUT};
echo "Encoding=UTF-8"                            >> ${DESKTOP}/${SHORTCUT};
echo "Version=6.0"                               >> ${DESKTOP}/${SHORTCUT};
echo "Type=Application"                          >> ${DESKTOP}/${SHORTCUT};
echo "Terminal=false"                            >> ${DESKTOP}/${SHORTCUT};
echo "Exec=$INSTALLDIR/%APP_NAME%.sh"     >> ${DESKTOP}/${SHORTCUT};
echo "Icon=$INSTALLDIR/kiosk.png"                >> ${DESKTOP}/${SHORTCUT};
echo "Name=%APP_NAME%"                 >> ${DESKTOP}/${SHORTCUT};

# If SELINUX is running, we need to set the security context for 
# one of our libs - otherwise, the browser won't launch.
if [ -e /selinux/enforce ]; then
   chcon -t texrel_shlib_t ${INSTALLDIR}/libxul.so   
fi

# install SOX

which yum > /dev/null;

if [ $? -eq 0  ]; then 
  echo sudo yum install sox;
  echo;

  sudo yum install sox;

  if [ $? -ne 0 ]; then
    echo installation of SOX failed ...
  fi

  exit 0;
fi

which apt-get > /dev/null;

if [ $? -eq 0  ]; then 
  echo sudo apt-get install sox;
  echo;

  sudo apt-get install sox;

  if [ $? -ne 0 ]; then
    echo installation of SOX failed ...
  fi

  exit 0;
fi

exit 0;

