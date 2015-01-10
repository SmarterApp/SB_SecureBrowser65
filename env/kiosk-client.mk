# kiosk-client.mk
#
# Automate checkout of the base firefox code and the kiosk client code.


# AIR mercurial repo for src files
HG_SB_SRC_URL = https://bitbucket.org/sbacoss/securebrowser65/src

# set this for newer branches
TARGET_REV=FIREFOX_10_0_12esr_RELEASE


OS_ARCH = $(shell uname -s)

thisdir = $(notdir $(shell pwd))


all: checkout

first-checkout: checkout

first-checkout-prod:: prodCheckOut

checkout:: mercurial-checkout kiosk-checkout jslib-checkout java-checkout applypatch mozconfig hgignore 
prodCheckOut:: mercurial-checkout kiosk-checkout-prodBuild jslib-checkout applypatch mozconfig

mercurial-checkout:

if [ ! -d "mozilla" ]; then \
	  if [ ! -d "../../mozilla" ]; then \
	    echo "Cloning mozilla source code from mozilla.org ..."; \
	    hg clone -r $(TARGET_REV)  http://hg.mozilla.org/releases/mozilla-esr10/ mozilla/; \
	  else \
            echo "Cloning from local mozilla source ..."; \
	    hg clone ../../mozilla mozilla/; \
	  fi \
	else \
	  echo "Mozilla source previously cloned. Reverting to checkout state..."; \
	  cd mozilla; hg update -C; \
	  echo "Reverting mozilla changes done.";\
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

