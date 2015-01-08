#!/bin/sh


#  MOZ_APP_NAME=__KIOSK_APP_NAME__
#  MOZ_APP_DISPLAYNAME=__KIOSK_DISPLAY_NAME__

export KIOSK_APP_NAME=$1;
export KIOSK_DISPLAY_NAME=$2;
export SRC_DIR=$3;


echo KIOSK_APP_NAME     : $KIOSK_APP_NAME;
echo KIOSK_DISPLAY_NAME : $KIOSK_DISPLAY_NAME;
echo SRC_DIR            : $SRC_DIR;

echo;

configure_file=$SRC_DIR/configure.in;
of=/tmp/.configure_file_00001;

# always work with the original file
if [ -f $configure_file.orig ]; then
  cp $configure_file.orig $configure_file;
fi

# backup original file
if [ ! -f $configure_file.orig ]; then
  cp $configure_file{,.orig};
fi

# replace strings below w/ new name
#
#  MOZ_APP_NAME=AIRSecureBrowser
#  MOZ_APP_DISPLAYNAME="AIR Secure Browser"
#


sed -e's:  MOZ_APP_NAME=AIRSecureBrowser:  MOZ_APP_NAME='$KIOSK_APP_NAME':g'                         \
    -e's:  MOZ_APP_DISPLAYNAME="AIR Secure Browser":  MOZ_APP_DISPLAYNAME='"$KIOSK_DISPLAY_NAME"':g' \
    $configure_file > $of;

mv $of $configure_file;

echo;
echo;

echo "configure.in updated to reflect new names ...";
echo "Now run autoconf2.13 to generate new configure file and then build from scratch";

exit 0;

