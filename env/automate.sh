#!/bin/bash

echo "############################################################################"

echo "
       @@@@@@   @@@@@@@      @@@@@@@   @@@  @@@  @@@  @@@       @@@@@@@
      @@@@@@@   @@@@@@@@     @@@@@@@@  @@@  @@@  @@@  @@@       @@@@@@@@
      !@@       @@!  @@@     @@!  @@@  @@!  @@@  @@!  @@!       @@!  @@@
      !@!       !@   @!@     !@   @!@  !@!  @!@  !@!  !@!       !@!  @!@
      !!@@!!    @!@!@!@      @!@!@!@   @!@  !@!  !!@  @!!       @!@  !@!
       !!@!!!   !!!@!!!!     !!!@!!!!  !@!  !!!  !!!  !!!       !@!  !!!
           !:!  !!:  !!!     !!:  !!!  !!:  !!!  !!:  !!:       !!:  !!!
          !:!   :!:  !:!     :!:  !:!  :!:  !:!  :!:   :!:      :!:  !:!
      :::: ::   :: ::::      :: ::::   ::::: ::  ::   :: ::::   :::: ::
      :: : :    :: : ::      :: : ::    : :  :   :    : :: : :  :: :  :
                                                                                 "
echo "###########################################################################"


#### Common functions #######################################################
function usage()
{
 echo "ERROR:parameter you gave is not valid"
 echo "valid paramaters are -b/--branding for branding option"
 echo "ex: autmate.sh -b OaksSecureBrowser"
}

function cleanUp()
 {
		echo ".......Cleaning build directories ......"
		rm -rf mozilla
		rm -rf Release
		rm -rf opt-*
 }

function checkOS()
 {
		system=`uname -s 2>/dev/null`
		echo $system | grep -i NT > /dev/null 2>&1;
		rv=$?;
		if [ "$system" != "Darwin" ]; then
			if [ $rv -eq 0  ]; then
			echo "win32";
			fi
		fi
		if [ "$system" = "Linux" ]; then
			echo "Linux";
		fi
		if [ "$system" = "Darwin" ]; then
			echo "OSX";
		fi
 }

function setupSource()
{
	if [ ! -d $project-src ]; then
		make -f $codename-client.mk first-checkout-prod
		wait;
	else
		cd $project;
		make -f $codename-client.mk repatch;
		wait;
	fi

}

function setBranding()
{
# check if branding is need
	if [ $brand_flag == 2 ]
        then {
             echo "applying  $brandName branding "

                if [ -e $branding_app_config_file ]
                then
                {
                        cat /dev/null > $branding_app_config_file
                        echo $brandName > $branding_app_config_file
                 }
                else
                {
                        echo " ERROR: Brand config file $branding_app_config_file, doesnt exit"
                        exit 1
                }
                fi

	}
  
	elif [ $brand_flag == 0  ]
	then
        echo " running default branding build"
	
       else {
	echo " branding went wrong, check build script and see why you are here" 
        exit 1
	}   
       fi  
}

function makeConfigure()
{
	#check you must be in Mozilla dir before you run
	make -f client.mk configure || { echo "...Make Error: failed to configure build..." ; exit 1; }

}

function makeBuild()
{
	#check you must be in Mozilla dir before you run
	make -f client.mk build || { echo "...Make Error: failed to run build..." ; exit 1; }

}
 
 
#### OS Specific functions ##################################################

#### Windows #####
function windowsBuild()
{
	setupSource
	cd ./mozilla/$codename
	echo "OS is Windows";
# commented out after talking to Pete
#	mkdir -p redist/microsoft/system;
#	echo "Fetching redist MS dll's ...";
#	wget --no-check-certificate -N -P redist/microsoft/system/ $MDG_REDIST_URL/msvcirt.dll;
#	wait;
#	wget --no-check-certificate -N -P redist/microsoft/system/ $MDG_REDIST_URL/msvcp71.dll;
#	wait;
#	wget --no-check-certificate -N -P redist/microsoft/system/ $MDG_REDIST_URL/msvcr71.dll;
#	wait;
#	wget --no-check-certificate -N -P redist/microsoft/system/ $MDG_REDIST_URL/msvcrt.dll;
#	wait;
# cd to build root
	cd ./../../
	
# check out Java runtime environment
	make -f kiosk-client.mk java-checkout
	
# check out Windows dlls
	make -f kiosk-client.mk win32-dll-checkout

# setup branding
setBranding
	
# cd to mozilla dir for build prep
cd ./mozilla

# Configure 
makeConfigure

#build after Configure
makeBuild


# go to distro directory
cd ./../opt*
cd $codename
echo " .... entering `pwd` directory..."

# run distro
make distro || { echo " Make Error: distro failed, please verify build ...."; exit 1; }

# Next Steps: automate MSI In classpath, this done at system provisioning
make msi || { echo " MSI Build failed, please verify build ...."; exit 1; }

# Move to release directory
echo "Move build"
echo "...create release directory ..."
mkdir ./../../Release
echo ".............................."
echo ".............................."
echo " ......move artifact........."
echo " ............................."
echo "..............................."
mv ~/Desktop/*.msi  ./../../Release/
echo "....build moved to release dir ...."
echo "...................................."
echo "....................................."

}

#### Windows end ###


#### Centos  ######
function linuxBuild()
{
setupSource

#check out Java runtime environment
make -f kiosk-client.mk java-checkout

#Simon Kwame: ASCM Addition - Add unavailable files from local build
cp ./fileshere/jslib_current.xpi ./mozilla/kiosk/jslib/
cp ./fileshere/jre-1.6.0_31.tar.bz2 ./mozilla/kiosk/plugins/

# setup branding
setBranding

# goto Mozilla dir to prep build
cd ./mozilla
# Configure 
makeConfigure

#build after Configure
makeBuild

# go to distro directory
cd ./../opt*
cd $codename
echo " .... entering `pwd` directory..."

# run distro
make distro || { echo " Make Error: distro failed, please verify build ...."; exit 1; }


echo "Move build"
echo "...create release directory..."
mkdir ./../../Release
mkdir -p ~/Desktop
mv ~/Desktop/*.tar.bz2  ./../../Release/
echo "... build moved to release dir ...."

}

#### Centos end ####

#### Mac #######
function macBuild()
{
setupSource

# setup branding
setBranding

# goto Mozilla dir to prep build
cd ./mozilla

# Configure 
makeConfigure


#build after Configure
makeBuild


# go to s.b. build directory
cd ./../opt*

# after checking with Balaji
cd  ./i386/$codename
#cd  ./i386
make || { echo " Make Error: 386 make build failed at first run"; exit 1;   }
make || { echo " Make Error: 386 make build failed at second run" ; exit 1; }

# after checking with Balaji
cd ./../../x86_64/$codename
#cd ./../x86_64
make || { echo " Make Error: x86_64 make build failed at first run"; exit 1;  }
make || { echo " Make Error: x86_64  make build failed at second run"; exit 1; }

cd ./../../../mozilla

# commented out after checking with Balaji
#cd ./../../mozilla

#build before distro
makeBuild

# build distro
cd ./../opt*
cd  ./i386/$codename
make distro || { echo " Make Error: distro failed, please verify build ...."; exit 1; }

echo "Move build"
echo "...create release directory..."
mkdir ./../../../Release
mkdir -p ~/Desktop
mv ~/Desktop/*.dmg  ./../../../Release/
echo "... build moved to release dir ...."

}

#### Mac end #####



#### Build Steps: run time ###################################################

# name of the project
project=air;
if [ "$2" == "3.1" ]; then
  hg_version="-1.9.1"
fi
# codename of app
codename=kiosk;

# branding details
brand_flag=0
brandName=
branding_dir=./../src/branding
branding_app_config_file=./mozilla/$codename/config/appname.txt

#check parameters
if [ $# -gt 0 ]; then {
    echo "running paramaterized build"
	while [ "$1" != "" ]; do
    		case $1 in
        		-b | --branding )           shift
                               			 brandName=$1
						 brand_flag=1
                               			 ;;
       			 -h | --help )           usage
                        		         exit
                               			 ;;
     			 * )                     usage
                        		         exit 1
   		 esac
   		 shift
	done
    echo "brand name given as build parameter is: $brandName "
    echo "validating given brand name"

        if [ -d $branding_dir/$brandName ]; then
        {
        brand_flag=2
        echo " $brandName is detected for build as valid option"
        }
        else {
        echo "$brandName is not avalid brand option"
        echo " Valid Brand options are ...."
        ls -l $branding_dir |egrep '^d'| awk '{print $9}' 
		exit 1
        }
        fi
echo
}
# user has not given any params
else
    echo "no paramaters for this build, running .. default build.."
fi


# clean checkout
cleanUp
echo 
echo "-.-.-.- CLEAN UP DONE .-.-.-.-.-"
echo

#check system
OS=`checkOS`
echo 
echo ".....$OS is detected operating systems ...."
echo

if [ "$OS" = "Linux" ]
then
	{
		echo " running $OS build "
		linuxBuild
		exit 0 ;
	}
elif [ "$OS" = "win32" ]
then
	{
		echo " running $OS build "
		windowsBuild
		exit 0 ;
	}
elif [ "$OS" = "OSX" ]
then
	{
		echo " running $OS build "
		macBuild
		exit 0 ;
	}
else
    { 
		echo "###this build systems is not supported####"
		exit 1 ;
	}
fi
