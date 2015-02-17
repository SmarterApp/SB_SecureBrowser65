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

#### Centos  ######
function linuxBuild()
{
setupSource

#check out Java runtime environment
make -f kiosk-client.mk java-checkout

#Simon Kwame: ASCM Addition - Add unavailable files from local build
mkdir -p ~/Desktop
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
mv ~/Desktop/*.tar.bz2  ./../../Release/
echo "... build moved to release dir ...."

}

#### Centos end ####


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
else
    { 
		echo "###this build systems is not supported####"
		exit 1 ;
	}
fi
