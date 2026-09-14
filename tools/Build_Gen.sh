#!/bin/sh

# Prefer Description.xml variant (respects pre-set BUILD_TARGET)
if [ -f "${0%/*}/../Description.xml" ] ; then
   # shellcheck source=ResolveDescription.sh
   . "${0%/*}/ResolveDescription.sh" || exit 1
elif [ -z "$BUILD_TARGET" ] ; then
   export BUILD_TARGET=linux-x86_64-trixie
   echo "unspecified BUILD_TARGET using [$BUILD_TARGET]"
fi

if [ -z "$BUILD_TARGET" ] ; then
   echo "[ERROR] BUILD_TARGET not set after Description resolve" >&2
   exit 1
fi
export BUILD_TARGET

TARGET=foundation

if [ ! -d ${0%/*}/../build ] ; then
{
   echo "mkdir ${0%/*}/../build"
   mkdir ${0%/*}/../build
}
fi
if [ ! -d ${0%/*}/../dist ] ; then
{
   echo "mkdir ${0%/*}/../dist"
   mkdir ${0%/*}/../dist
}
fi

cd ${0%/*}/../build

if [ ! -f CMakeCache.txt ] ; then
    cmake -G "Unix Makefiles" ..
    if [ $? -ne 0 ]; then
    exit 1;
    fi
else
    cmake ..
    if [ $? -ne 0 ]; then
    exit 1;
    fi
fi

cmake --build . --target "$TARGET" -- -j`nproc`
if [ $? -ne 0 ]; then
   exit 1;
fi

cmake --build . --target foundation_utests -- -j`nproc`
if [ $? -ne 0 ]; then
   exit 1;
fi

cmake --install .
if [ $? -ne 0 ]; then
   exit 1;
fi
