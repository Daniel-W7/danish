#!/bin/sh
# Run this to generate all the initial makefiles, etc.

case $1 in
	po|pot|linguas) po/zzpo.sh $1 ; exit $? ;;
	release|-release|--release)
		pkg="$(grep -m 1 AC_INIT configure.ac | cut -f 2 -d '[' | cut -f 1 -d ']')"
		ver="$(grep -m 1 AC_INIT configure.ac | cut -f 3 -d '[' | cut -f 1 -d ']')"
		ver=$(echo $ver)
		dir=${pkg}-${ver}
		rm -rf ../${dir}
		mkdir -p ../${dir}
		cp -rf $(pwd)/* ../${dir}
		( cd ../${dir} ; ./autogen.sh )
		cd ..
		tar -Jcf ${dir}.tar.xz ${dir}
		exit $?
		;;
	"") ok=1 ;;
	*)
		echo "Unrecognized param: $1"
		exit 1 ;;
esac

#===========================================================================
#                      autogen.sh
#===========================================================================

srcdir=`dirname $0`
#-z 字符串:字符串长度伪则为真,
test -z "$srcdir" && srcdir=.
cd $srcdir

test -z "$AUTOMAKE"   && AUTOMAKE=automake
test -z "$ACLOCAL"    && ACLOCAL=aclocal
test -z "$AUTOCONF"   && AUTOCONF=autoconf
test -z "$AUTOHEADER" && AUTOHEADER=autoheader
test -z "$LIBTOOLIZE" && LIBTOOLIZE=$(which libtoolize glibtoolize 2>/dev/null | head -1)
test -z "$LIBTOOLIZE" && LIBTOOLIZE=libtoolize #paranoid precaution

# pre-create some dirs / files,创建一些必要的文件和文件夹
# 定义编译目录
auxdir='.'
#grep -q,不显示任何信息
#如果configure.ac里面有配置的话，则使用configure.ac里面的配置
if grep -q "AC_CONFIG_AUX_DIR" configure.ac ; then
	auxdir="$(grep AC_CONFIG_AUX_DIR configure.ac)"
fi
mkdir -p ${auxdir}
#创建config，rpath文件
touch ${auxdir}/config.rpath
#判断m4dir是否存在，不存在的话就创建一个出来
m4dir="$(grep AC_CONFIG_MACRO_DIR configure.ac | cut -f 2 -d '[' | cut -f 1 -d ']')"
#test -n 字符串:字符串长度不伪则为真
if test -n "$m4dir" ; then
	mkdir -p ${m4dir}
fi

# Get all m4 macros(宏) required for configure，获取所有configure需要的m4宏
if grep -q LT_INIT configure.ac \
	|| grep -q AM_PROG_LIBTOOL configure.ac \
	|| grep -q AC_PROG_LIBTOOL configure.ac ; then
	echo "- Running $LIBTOOLIZE --copy --force"
	$LIBTOOLIZE --copy --force || exit 1
fi

echo "- Running $ACLOCAL"
$ACLOCAL || exit 1

echo "- Running $AUTOHEADER --force"
# Generate config.h.in
$AUTOHEADER --force || exit 1

# Generate Makefile.in's
echo "- Running $AUTOMAKE --add-missing --copy --force"
$AUTOMAKE --add-missing --copy --force || exit 1

# generate configure
echo "- Running $AUTOCONF --force"
$AUTOCONF --force || exit 1

#删除m4文件夹和m4配置文件
if test "$(ls $m4dir)" = "" ; then
	rmdir $m4dir
fi
rm -rf autom4te.cache
