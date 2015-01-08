#!/bin/sh

sys=`uname -s`;

if [ "$sys" = "Linux" ]; then
  return 0;
fi

of=~/kiosk_out.sh;

/bin/rm -f $of;

# read
one=`/usr/bin/defaults read com.apple.dock wvous-tl-corner`
two=`/usr/bin/defaults read com.apple.dock wvous-tr-corner`
three=`/usr/bin/defaults read com.apple.dock wvous-bl-corner`
four=`/usr/bin/defaults read com.apple.dock wvous-br-corner`

scl=`/usr/bin/defaults read com.apple.screencapture location`;

/bin/mkdir -p ~/.sb;

echo "#!/bin/sh" > $of;
echo  >> $of;
echo  >> $of;

if [ $scl ]; then
  echo /usr/bin/defaults write com.apple.screencapture location $scl >> $of;
else
  echo /usr/bin/defaults delete com.apple.screencapture >> $of;
fi

echo  >> $of;
echo /usr/bin/killall -HUP SystemUIServer >> $of;
echo /bin/rm -rf  ~/.sb >> $of;
echo  >> $of;

chmod 755 $of;

/usr/bin/defaults write  com.apple.screencapture location ~/.sb;
/usr/bin/wait;
/usr/bin/killall -HUP SystemUIServer;

# no longer used
slp=0;

tsf=~/.sb_stamp;

if [ -f $tsf ]; then
  s=`cat $tsf`;
  ct=`expr $s + 15`;
  d=`date "+%s"`;

  if [ $d -gt $ct ]; then
   slp=1;
  fi
fi

date "+%s" > ~/.sb_stamp;

if [ $one == 1 ]; then
  if [ $two == 1 ]; then
    if [ $three == 1 ]; then
      if [ $four == 1 ]; then
        # turn off VoiceOver dialog
        # /usr/bin/defaults write ~/Library/Preferences/com.apple.VoiceOverTraining doNotShowSplashScreen -boolean true
        # /usr/bin/killall -HUP Dock
        # /usr/bin/wait;
        # give the Dock time to reappear
        # /bin/sleep 8;
        exit 0;
      fi
    fi
  fi
fi

# the reason the below lines are commented is
# restoring the settings below will result in 
# the Dock appearing when we launch the SecureBrowser
# every time because we have to restart the dock
# by not restoring the expose settings, we no longer need to restart the dock
# --pete

# echo /usr/bin/defaults write com.apple.dock wvous-tl-corner -int $one   >> $of;
# echo /usr/bin/defaults write com.apple.dock wvous-tr-corner -int $two   >> $of;
# echo /usr/bin/defaults write com.apple.dock wvous-bl-corner -int $three >> $of;
# echo /usr/bin/defaults write com.apple.dock wvous-br-corner -int $four  >> $of;

# echo  >> $of;
# echo  >> $of;
# echo /usr/bin/killall -HUP Dock >> $of;
# echo /bin/sleep 2 >> $of;
# echo /usr/bin/killall -HUP Dock >> $of;

echo  >> $of;
echo  >> $of;
echo  exit 0 >> $of;

# turn off active corners --pete
/usr/bin/defaults write com.apple.dock wvous-tl-corner -int 1
/usr/bin/defaults write com.apple.dock wvous-tr-corner -int 1
/usr/bin/defaults write com.apple.dock wvous-bl-corner -int 1
/usr/bin/defaults write com.apple.dock wvous-br-corner -int 1

# turn off VoiceOver dialog
/usr/bin/defaults write ~/Library/Preferences/com.apple.VoiceOverTraining doNotShowSplashScreen -boolean true

/usr/bin/killall -HUP Dock

i=0;

while ((i<=100)); do

  d=`/bin/ps ax | /usr/bin/grep Dock.app | /usr/bin/sed /grep/d`;

  if [ "$d" ]; then
    break;
  fi
  let i=$i+1;
done

exit 0;

