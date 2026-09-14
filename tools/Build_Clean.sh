#!/bin/sh

# Remove Description-fetched trees, then build/ and dist/
if [ -f "${0%/*}/../Description.xml" ] ; then
   # shellcheck source=ResolveDescription.sh
   . "${0%/*}/ResolveDescription.sh" || exit 1
   if command -v python3 >/dev/null 2>&1 ; then
      _PY=python3
   elif command -v python >/dev/null 2>&1 ; then
      _PY=python
   else
      echo "[ERROR] Python 3 required for Description clean" >&2
      exit 1
   fi
   $_PY "${0%/*}/Dependencies.py" -d "${0%/*}/../Description.xml" clean || exit 1
fi

if [ -d "${0%/*}/../build" ] ; then
    echo "Deleting ${0%/*}/../build"
    rm -rf "${0%/*}/../build"
fi
if [ -d "${0%/*}/../dist" ] ; then
    echo "Deleting ${0%/*}/../dist"
    rm -rf "${0%/*}/../dist"
fi
