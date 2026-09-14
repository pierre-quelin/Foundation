#!/bin/sh

# Resolve + fetch components from Description.xml
if [ -f "${0%/*}/../Description.xml" ] ; then
   # shellcheck source=ResolveDescription.sh
   . "${0%/*}/ResolveDescription.sh" || exit 1

   if command -v python3 >/dev/null 2>&1 ; then
      _PY=python3
   elif command -v python >/dev/null 2>&1 ; then
      _PY=python
   else
      echo "[ERROR] Python 3 required for Description fetch" >&2
      exit 1
   fi
   $_PY "${0%/*}/Dependencies.py" -d "${0%/*}/../Description.xml" fetch || exit 1
fi
