#!/bin/bash
#
# bump-version.sh — maintain the MacFile version numbers.
#
# afpserver/Resource.rdef is the single source of truth for the version.
# This script propagates its major/middle/minor to every other place the
# version is recorded:
#
#   afp_config/Resource.rdef                 app_version resource
#   installer/Resource.rdef                  app_version resource
#   afpserver/afp_sources/commands.h         AFP_SERVER_VERSION macro
#
# Usage:
#   ./bump-version.sh          sync derived files from the source of truth
#   ./bump-version.sh --bump   increment the third digit (minor) of the
#                              source of truth first, then sync
#
# build-release.sh runs "./bump-version.sh --bump", so every release build
# automatically increments the third digit. major and middle are never
# changed automatically: to ship a major or middle update, edit
# afpserver/Resource.rdef by hand and re-run this script (without --bump)
# to push the new values to the other files.
#
# Nothing is committed; review and commit the touched files when the result
# is what you want. On success the script prints only the new version
# (x.y.z) on stdout; informational messages go to stderr.
#
set -euo pipefail

cd "$(dirname "$0")"

SRC_RDEF="afpserver/Resource.rdef"
DERIVED_RDEFS="afp_config/Resource.rdef installer/Resource.rdef"
COMMANDS_H="afpserver/afp_sources/commands.h"

fail() {
	echo "ERROR: $1" >&2
	exit 1
}

case "${1:-}" in
	"")      bump=0 ;;
	--bump)  bump=1 ;;
	*)       fail "usage: $0 [--bump]" ;;
esac

for f in "$SRC_RDEF" $DERIVED_RDEFS "$COMMANDS_H"; do
	[ -f "$f" ] || fail "missing $f — run this script from the repository root"
done

# Print the numeric value of an app_version field ($2 = major|middle|minor).
rdef_get() {
	sed -E -n "s|^[[:space:]]*$2[[:space:]]*=[[:space:]]*([0-9]+).*|\1|p" "$1" | head -n 1
}

major="$(rdef_get "$SRC_RDEF" major)"
middle="$(rdef_get "$SRC_RDEF" middle)"
minor="$(rdef_get "$SRC_RDEF" minor)"

{ [ -n "$major" ] && [ -n "$middle" ] && [ -n "$minor" ]; } \
	|| fail "could not read major/middle/minor from $SRC_RDEF"

if [ "$bump" = 1 ]; then
	minor=$((minor + 1))
fi

# The AFP_SERVER_VERSION macro packs each component into one byte.
for n in "$major" "$middle" "$minor"; do
	[ "$n" -le 255 ] || fail "version component $n out of range (must be 0-255)"
done

# Verify an rdef file carries the expected version after rewriting.
rdef_check() {
	{ [ "$(rdef_get "$1" major)" = "$major" ] \
		&& [ "$(rdef_get "$1" middle)" = "$middle" ] \
		&& [ "$(rdef_get "$1" minor)" = "$minor" ]; } \
		|| fail "failed to update version in $1"
}

# Rewrite major/middle/minor in an rdef file (canonical rdef formatting).
rdef_set() {
	sed -E \
		-e "s|^[[:space:]]*major[[:space:]]*=.*|\tmajor  = $major,|" \
		-e "s|^[[:space:]]*middle[[:space:]]*=.*|\tmiddle = $middle,|" \
		-e "s|^[[:space:]]*minor[[:space:]]*=.*|\tminor  = $minor,|" \
		"$1" > "$1.tmp" || { rm -f "$1.tmp"; fail "failed to rewrite $1"; }
	mv "$1.tmp" "$1"
	rdef_check "$1"
}

if [ "$bump" = 1 ]; then
	rdef_set "$SRC_RDEF"
	echo "bumped $SRC_RDEF" >&2
fi

for f in $DERIVED_RDEFS; do
	rdef_set "$f"
done

# Rewrite the AFP_SERVER_VERSION macro, e.g. 2.1.1 -> 0x02010100. The macro
# name is anchored to the start of the #define line so that
# CMD_AFP_SERVER_VERSION further down the file is left untouched.
hex="$(printf '0x%02X%02X%02X00' "$major" "$middle" "$minor")"
version="$major.$middle.$minor"

grep -Eq "^#[[:space:]]*define[[:space:]]+AFP_SERVER_VERSION" "$COMMANDS_H" \
	|| fail "AFP_SERVER_VERSION define not found in $COMMANDS_H"

sed -E \
	"s|^#[[:space:]]*define[[:space:]]+AFP_SERVER_VERSION.*|#define AFP_SERVER_VERSION\t\t$hex\t// version $version|" \
	"$COMMANDS_H" > "$COMMANDS_H.tmp" \
	|| { rm -f "$COMMANDS_H.tmp"; fail "failed to rewrite $COMMANDS_H"; }
mv "$COMMANDS_H.tmp" "$COMMANDS_H"

grep -qF "$hex" "$COMMANDS_H" || fail "failed to update version in $COMMANDS_H"

echo "synced $DERIVED_RDEFS and $COMMANDS_H" >&2
echo "$version"
