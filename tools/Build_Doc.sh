#!/bin/sh

cd "${0%/*}/.." || exit 1

if ! command -v doxygen >/dev/null 2>&1 ; then
   echo "ERROR: doxygen not found on PATH. Install Doxygen and retry."
   exit 1
fi

echo "Generating documentation with Doxygen..."
doxygen Doxyfile
if [ $? -ne 0 ] ; then
   echo "ERROR: Doxygen failed."
   exit 1
fi

if [ -f doc/html/index.html ] ; then
   echo "Documentation: doc/html/index.html"
else
   echo "WARNING: doc/html/index.html not found."
fi

exit 0
