# kiosk-client.mk
#
# Automate checkout of the base firefox code and the kiosk client code.


# AIR mercurial repo for src files
HG_SB_SRC_URL = https://bugz.airast.org/kiln/Code/TDS-Student-Labs/Secure-Browser/SecureBrowserSrc

# set this for newer branches
TARGET_REV=FIREFOX_10_0_12esr_RELEASE

JSLIB_XPI_URL = https://www.mozdevgroup.com/dropbox/jslib/jslib_current.xpi
JAVA_WIN32_URL = https://www.mozdevgroup.com/dropbox/air/java-6.0_29.zip
JAVA_LINUX_URL1 = https://www.mozdevgroup.com/dropbox/air/jre-1.4.tar.bz2
JAVA_LINUX_URL2 = https://www.mozdevgroup.com/dropbox/air/jre-1.5.tar.bz2
JAVA_LINUX_URL3 = https://www.mozdevgroup.com/dropbox/air/jre-1.6.0_31.tar.bz2

JAVA_OSX_JEP_URL = https://www.mozdevgroup.com/dropbox/air/JEP.zip

MDG_REDIST_URL=https://www.mozdevgroup.com/dropbox/redist/microsoft/system

OS_ARCH = $(shell uname -s)

thisdir = $(notdir $(shell pwd))


all: checkout

first-checkout: checkout

first-checkout-prod:: prodCheckOut

checkout:: mercurial-checkout kiosk-checkout jslib-checkout java-checkout applypatch mozconfig hgignore 
prodCheckOut:: mercurial-checkout kiosk-checkout-prodBuild jslib-checkout applypatch mozconfig

mercurial-checkout:
	@if [ ! -d mozilla ]; then \
	  hg clone -r $(TARGET_REV)  http://hg.mozilla.org/releases/mozilla-esr10/ mozilla/; \
	fi

kiosk-checkout-prodBuild: 
	cd mozilla; cp -r ./../../src kiosk;
	
kiosk-checkout:
	@if [ ! -d mozilla/kiosk ]; then \
	  hg clone $(HG_SB_SRC_URL) mozilla/kiosk/; \
	fi

applypatch:
	@cd mozilla; patch -p1 < ../kiosk-core-changes.patch;

cleanpatch:
	@cd mozilla; hg update -C;

repatch: cleanpatch applypatch

jslib-checkout: 
	wget --no-check-certificate -N $(JSLIB_XPI_URL) -P mozilla/kiosk/jslib;

win32-dll-checkout:
	mkdir -p mozilla/kiosk/redist/;
	echo "Fetching redist MS dll's ...";
	wget --no-check-certificate -N -P mozilla/kiosk/redist/ $(MDG_REDIST_URL)/msvcirt.dll;
	wait;
	wget --no-check-certificate -N -P mozilla/kiosk/redist/ $(MDG_REDIST_URL)/msvcp71.dll;
	wait;
	wget --no-check-certificate -N -P mozilla/kiosk/redist/ $(MDG_REDIST_URL)/msvcr71.dll;
	wait;
	wget --no-check-certificate -N -P mozilla/kiosk/redist/ $(MDG_REDIST_URL)/msvcrt.dll;
	wait;	


java-jep-checkout: 
	system=`uname -s 2>/dev/null`;   \
	if [ "$$system" == "Darwin" ]; then \
	  wget --no-check-certificate -N $(JAVA_OSX_JEP_URL) \
	wait; \
	unzip -o JEP.zip -d mozilla/plugin/oji/JEP/; \
	fi;

java-checkout: 
	system=`uname -s 2>/dev/null`;   \
	echo $$system ;                  \
	echo $$system | grep WIN;        \
	if [ $$? -ne 0 ]; then           \
	  echo $$system | grep MING;     \
	fi;                              \
	if [ $$? -eq 0 ]; then \
	  wget --no-check-certificate -N $(JAVA_WIN32_URL) -P mozilla/kiosk/java; \
	else \
	  wget --no-check-certificate -N $(JAVA_LINUX_URL3) -P mozilla/kiosk/plugins; \
	fi;

mozconfig:
  ifeq ($(OS_ARCH),Linux)
	echo '. $$topsrcdir/kiosk/config/mozconfig.'$(OS_ARCH) > mozilla/.mozconfig;
  else
    ifeq ($(OS_ARCH),Darwin)
	echo '. $$topsrcdir/kiosk/config/mozconfig.'$(OS_ARCH).universal > mozilla/.mozconfig;
    else
	echo '. $$topsrcdir/kiosk/config/mozconfig.WINNT' > mozilla/.mozconfig;
    endif
  endif

hgignore:
	@echo kiosk > mozilla/.hgignore

help:
	@echo build targets: checkout mercurial-checkout kiosk-checkout kiosk-checkout-prodBuild jslib-checkout mozconfig applypatch repatch cleanpatch java-jep-checkout java-checkout win32-dll-checkout hgignore

