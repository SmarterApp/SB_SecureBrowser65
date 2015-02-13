rem this file will create profile settings
rem  profile will be loaded as msvc env.
rem Teamcity working dir and shell script name to run are writen to the profile as params.
@ECHO OFF
SET curDir=%cd%
SET homdir=%HOMEDRIVE%%HOMEPATH%
cd %homdir%
if  not exist .bash_profile goto createnew
del .bash_profile
:createnew
set a=%1%
echo %a%
set a=%a:\=/%
echo %a%
set a=%a::=%
echo cd /%a% > .bash_profile
echo sh %2 %3 %4>> .bash_profile
echo  if [ $? -eq 1 ]; then >> .bash_profile
echo  exit 1 >> .bash_profile
echo  else >> .bash_profile
echo  exit 0 >> .bash_profile
echo  fi >> .bash_profile
cd %curDir%