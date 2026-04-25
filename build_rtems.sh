#!/bin/bash
#RTEMS development
export RTEMS_VERSION=6
export RTEMS_RELEASE=6.1
export RTEMS_CPU=powerpc
export RTEMS_BSP=beatnik
export RTEMS_ARCH=${RTEMS_CPU}-rtems${RTEMS_VERSION}
SCRIPT=$(readlink -f $0)
export RTEMS_ROOT=`dirname $SCRIPT`/rtems/${RTEMS_RELEASE}

#install rsb
wget https://ftp.rtems.org/pub/rtems/releases/6/6.1/sources/rtems-source-builder-6.1.tar.xz
tar Jxf rtems-source-builder-6.1.tar.xz

#build the tools
cd rtems-source-builder-6.1/rtems
../source-builder/sb-set-builder \
  --prefix=${RTEMS_ROOT} \
  6/rtems-${RTEMS_CPU}

# Build kernel

#!/bin/bash
#RTEMS development
export RTEMS_VERSION=6
export RTEMS_RELEASE=6.1
export RTEMS_CPU=powerpc
export RTEMS_BSP=beatnik
export RTEMS_ARCH=${RTEMS_CPU}-rtems${RTEMS_VERSION}
SCRIPT=$(readlink -f $0)
export RTEMS_ROOT=`dirname $SCRIPT`/rtems/${RTEMS_RELEASE}

export PATH=${RTEMS_ROOT}/bin:${PATH}
echo ${PATH}

git clone https://gitlab.rtems.org/rtems/rtos/rtems.git kernel
cd kernel
git checkout origin/6
./waf bspdefaults --rtems-bsps=${RTEMS_CPU}/${RTEMS_BSP} > config.ini
sed -i \
-e "s|RTEMS_POSIX_API = False|RTEMS_POSIX_API = True|" \
-e "s|BUILD_TESTS = False|BUILD_TESTS = True|" \
config.ini

./waf configure --prefix=${RTEMS_ROOT}
./waf
./waf install