#!/bin/sh

# source nss env variables
if [ -f ~/.nss ]; then
  . ~/.nss
fi

SIGN_TOOL_PATH=`which signtool`;

xpi_dir=$inst_dir/xpi;
signed_dir=$xpi_dir/signed;
signed_tmp=/tmp/signed;
nickname=$NICKNAME;

if [ ! $SIGN_TOOL_PATH ] && [ ! $NSS_BIN ]; then
  echo "please set NSS_BIN env variable to system path to nss binaries and libraries ...";
exit 1;
elif [ $SIGN_TOOL_PATH ]; then 
  NSS_BIN=`echo $SIGN_TOOL_PATH | sed -e's:/\?signtool::g'`;
fi

if [ ! -f $NSS_BIN/libnss3.so ]; then
  echo "Please set env variable NSS_LIBS to the path where NSS libraries are installed ...";
exit 1;
else
  NSS_LIBS=$NSS_BIN;
fi

if [ ! $NSS_DB_PATH ]; then
  echo "Please set env variable NSS_DB_PATH to where the db containing your cert is located ...";
exit 1;
fi

if [ ! $PASSWORD ]; then
  echo "Please set env variable PASSWORD to your db password ...";
exit 1;
fi

export LD_LIBRARY_PATH=$NSS_LIBS;

mkdir -p $signed_tmp;
mkdir -p $signed_dir;
cd $signed_tmp;

for f in `ls $xpi_dir/*.xpi`
  do
    echo xpi: $f;
		rm -rf *;
    wait;
    unzip $f;
    wait;
		pkg_name=`echo $f | sed -e 's:'$xpi_dir'/::g' -e's:.xpi::g'`;
		pkg_name=$pkg_name"_signed.xpi";
		echo $pkg_name;
    $NSS_BIN/signtool -d $NSS_DB_PATH \
    -k "$nickname" -p $PASSWORD .;
    zip $pkg_name META-INF/zigbert.rsa
    zip -r -D $pkg_name * -x META-INF/zigbert.rsa
    mv $pkg_name $signed_dir/;
	done

du -h $signed_dir/*.xpi;
rm -rf $signed_tmp;

exit 0;
