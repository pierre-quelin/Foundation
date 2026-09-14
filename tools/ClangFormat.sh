#!/usr/bin/env bash
# Apply or check clang-format using a clang-format binary available on PATH.
# Same rules as Foundation; Sample keeps a copy of .clang-format at repo root.
#
# Usage (from repo root or tools/) :
#   tools/ClangFormat.sh              dry-run on src/ (+ tests/ if present)
#   tools/ClangFormat.sh -i           write changes on default trees
#   tools/ClangFormat.sh -i path/to/file.cpp
#   tools/ClangFormat.sh path/dir     dry-run on that path (file or directory)
#
# Env : CLANG_FORMAT = full path to clang-format binary (optional override)

set +u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

CF="${CLANG_FORMAT:-}"
if [ -n "$CF" ] && [ -f "$CF" ]; then
    :
else
    CF=""
fi

if [ -z "$CF" ]; then
    # Prefer command on PATH first.
    if command -v clang-format >/dev/null 2>&1; then
        CF="$(command -v clang-format)"
    else
        # Also try common versioned executable names.
        for candidate in clang-format-19 clang-format-20 clang-format-21 clang-format-22 clang-format-23 clang-format; do
            if command -v "$candidate" >/dev/null 2>&1; then
                CF="$(command -v "$candidate")"
                break
            fi
        done
    fi
fi

if [ -z "$CF" ]; then
    echo "[ERROR] clang-format not found."
    echo "        Install: clang-format (or clang-format-X) and ensure it is on PATH"
    echo "        Or set CLANG_FORMAT=/path/to/clang-format"
    exit 1
fi

echo "[INFO] using \"$CF\""
"$CF" --version

MODE="dry"
TARGETS=()

usage() {
    echo "Usage: $(basename "$0") [-i|--apply] [path ...]"
    echo "  default paths: src/ and tests/ (if present)"
    echo "  default mode: dry-run (-Werror); -i writes files"
    exit 0
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        -i|--apply)
            MODE="write"
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            TARGETS+=("$1")
            shift
            ;;
    esac
done

if [ ${#TARGETS[@]} -eq 0 ]; then
    if [ -d "src" ]; then
        TARGETS+=("src")
    fi
    if [ -d "tests" ]; then
        TARGETS+=("tests")
    fi
fi

if [ ${#TARGETS[@]} -eq 0 ]; then
    echo "[ERROR] no targets (pass a path or run from a tree with src/)"
    exit 1
fi

FAILED=0
COUNT=0

one_file() {
    local file="$1"
    COUNT=$((COUNT + 1))

    if [ "$MODE" = "write" ]; then
        "$CF" -style=file -i "$file"
        if [ $? -ne 0 ]; then
            echo "[ERROR] format failed: $file"
            FAILED=1
        fi
    else
        "$CF" -style=file --dry-run -Werror "$file" >/dev/null 2>&1
        if [ $? -ne 0 ]; then
            echo "[DIFF] $file"
            FAILED=1
        fi
    fi
}

scan_target() {
    local target="$1"
    if [ -d "$target" ]; then
        while IFS= read -r -d '' file; do
            one_file "$file"
        done < <(find "$target" -type f \( -name '*.cpp' -o -name '*.cxx' -o -name '*.cc' -o -name '*.h' -o -name '*.hpp' -o -name '*.hxx' \) -print0)
    elif [ -f "$target" ]; then
        one_file "$target"
    else
        echo "[WARN] skip missing $target"
    fi
}

for target in "${TARGETS[@]}"; do
    scan_target "$target"
done

echo "[INFO] files checked/updated: $COUNT"

if [ "$FAILED" = "1" ]; then
    exit 1
fi

exit 0
