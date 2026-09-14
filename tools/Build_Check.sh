#!/bin/sh

# CMake path system vs custom
if [ -d /opt/cmake ] ; then
    CMAKE=/opt/cmake/bin/cmake
else
    CMAKE=cmake
fi

# Description.xml check (validate + recurse)
if [ -f "${0%/*}/../Description.xml" ] ; then
   # shellcheck source=ResolveDescription.sh
   . "${0%/*}/ResolveDescription.sh" || exit 1
   if command -v python3 >/dev/null 2>&1 ; then
      _PY=python3
   elif command -v python >/dev/null 2>&1 ; then
      _PY=python
   else
      echo "[ERROR] Python 3 required for Description check" >&2
      exit 1
   fi
   $_PY "${0%/*}/Dependencies.py" -d "${0%/*}/../Description.xml" check || exit 1
fi

# Construction de l'application
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

${CMAKE} -G "Unix Makefiles" ..
if [ $? -ne 0 ]; then
   exit 1;
fi

cd ../${0%/*}

cppcheck -j`nproc` --std=c++17 ../src \
   -I../build/include \
   --enable=all --suppress=missingIncludeSystem --inconclusive --xml --xml-version=2 2> ../dist/cppcheck-result.xml
if [ $? -ne 0 ]; then
   exit 1;
fi
