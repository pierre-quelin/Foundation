#!/bin/sh
# Resolve variant from Description.xml into the current shell (source this file).
#   . tools/ResolveDescription.sh
# Honors existing BUILD_TARGET; no-op if Description.xml is missing.

_ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
_DESC="$_ROOT/Description.xml"
if [ ! -f "$_DESC" ] ; then
   return 0 2>/dev/null || exit 0
fi

if command -v python3 >/dev/null 2>&1 ; then
   _PY=python3
elif command -v python >/dev/null 2>&1 ; then
   _PY=python
else
   echo "[ERROR] Python 3 required to resolve Description.xml" >&2
   return 1 2>/dev/null || exit 1
fi

_RESOLVE=$($_PY "$_ROOT/tools/Dependencies.py" -d "$_DESC" resolve --shell sh) || {
   return 1 2>/dev/null || exit 1
}
eval "$_RESOLVE"
echo "[INFO] Description resolve: BUILD_TARGET=$BUILD_TARGET VARIANT_ID=$VARIANT_ID ENV=$VARIANT_ENV ENV_VERSION=$ENV_VERSION"
return 0 2>/dev/null || exit 0
