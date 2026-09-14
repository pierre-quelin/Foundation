#!/bin/sh

${0%/*}/Build_Gen.sh
if [ $? -ne 0 ] ; then
   exit 1
fi

cd ${0%/*}/../build || exit 1

if [ ! -f CMakeCache.txt ] ; then
   echo "ERROR: build directory not configured. Run Build.sh gen first."
   exit 1
fi

cmake --build . --target foundation_utests
if [ $? -ne 0 ] ; then
   exit 1
fi

CTEST_OUTPUT_ON_FAILURE=1 ctest --output-on-failure
exit $?
