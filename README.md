# Welcome to the Secure Browser 6.5 Project
The [SmarterApp](http://smarterapp.org) Secure Browser 6.5 project builds upon the Mozilla Firefox source code and creates a secure browser that is used to deliver student assessments. The Secure Browser implements security features such as not permitting multiple tabs, browsing arbitrary URLs and enforcing a white list of applications.

## License ##
This project is licensed under the [Mozilla open source license v.2.0](https://www.mozilla.org/MPL/2.0/).

## Getting Involved ##
We would be happy to receive feedback on its capabilities, problems, or future enhancements:

* For general questions or discussions, please use the [Forum](http://forum.opentestsystem.org/viewforum.php?f=17).
* Use the **Issues** link to file bugs or enhancement requests.
* Feel free to **Fork** this project and develop your changes!

## Configuration ###

This version of the Secure Browser is available for the Linux platform. Each build follows a similar process, as described below. From a high-level perspective, building a custom secure browser involves:

* Cloning the Mozilla Firefox source (FIREFOX_10_0_12esr_RELEASE)
* Cloning this SecureBrowser project
* Applying a few customizations such as the target URL
* Running the automated build script

### Build Process

On any **CentOS 5.3 - 5.11** machine:

#### 1. Install basic required packages

* `yum update`
* `yum install gcc make zip unzip sshpass autoconf213 pkgconfig automake alsa-lib alsa-lib-devel curl openssl-devel openssl wget libXt libXt-devel wireless-tools-devel mesa-libGL mesa-libGL-devel ncurses-devel zlib-devel python-setuptools python-devel gtk2-* gtk2-devel curl-devel -y`
 
#### 2. Install Mercurial
`sudo easy_install Mercurial`
 
#### 3. Install yasm
* `wget http://www.tortall.net/projects/yasm/releases/yasm-1.1.0.tar.gz`
* `tar -xvzf yasm-1.1.0.tar.gz`
* `cd yasm-1.1.0`
* `./configure`
* `make`
* `make install`
 
#### 4. Install libIDL main and devel:

* `wget ftp://ftp.muug.mb.ca/mirror/centos/5.11/os/x86_64/CentOS/libIDL-0.8.7-1.fc6.i386.rpm`
* `rpm -ivh libIDL-0.8.7-1.fc6.i386.rpm`
* `wget ftp://ftp.muug.mb.ca/mirror/centos/5.11/os/x86_64/CentOS/libIDL-devel-0.8.7-1.fc6.i386.rpm`
* `rpm -ivh libIDL-devel-0.8.7-1.fc6.i386.rpm`

#### 5. Install dev tools:
`yum groupinstall "Development Tools"`

#### 6. Install libnotify:
`yum install libnotify* -y`

#### 7. Create a user and password

* switch to that user
* clone the Secure Browser 6.5 repo (e.g. `hg clone https://BBUSERNAME@bitbucket.org/sbacoss/securebrowser65_release`)

#### 8. Generate a v4 GUID
Get a [v4 GUID](https://www.uuidgenerator.net/version4) and place into `/src/branding/SBACSecureBrowser/uuid.txt` (only for a single custom build, not for every version)

#### 9. Add target URL
Obtain the URL of the TDS Student home page for the browser, uuencode it, and insert into `/src/branding/SBACSecureBrowser/pref/kiosk-branding.js` as defined within the file. The current default build will point the browser to http://browser.smarterbalanced.org/landing.

#### 10. Execute the Build
From within the env/ directory, run `/bin/bash automate.sh`