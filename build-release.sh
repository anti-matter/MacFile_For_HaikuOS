#!/bin/bash
#
# build-release.sh — single-command release build for MacFile.
#
# Builds the AFP server, the MacFile configuration app, and the
# MacFileInstaller GUI app, then assembles:
#
#	distribution/install.zip            (payload: afp_server, MacFile, OpenSSL libs)
#	distribution/MacFile_<arch>_Release.zip
#	                                     (MacFileInstaller, install-macfile.sh,
#	                                       install.zip, ReadMe!)
#
# Requires the Haiku build environment (BUILDHOME set), make, and zip.
#
set -euo pipefail

cd "$(dirname "$0")"
REPO_ROOT="$(pwd)"

X86=objects.x86-cc2-release
X64=objects.x86_64-cc13-release

fail() {
	echo "ERROR: $1" >&2
	exit 1
}

#
# 1. Clean and build all three components.
#
for dir in afpserver afp_config installer; do
	rm -rf "$dir/$X86" "$dir/$X64"
	( cd "$dir" && make )
done

#
# 2. Detect the architecture from the objects directories we just built.
#
if [ -d "afpserver/$X86" ]; then
	ARCH=x86
	OBJECTS=$X86
elif [ -d "afpserver/$X64" ]; then
	ARCH=x86_64
	OBJECTS=$X64
else
	fail "release build produced no objects directory (x86 or x86_64)."
fi

#
# 3. Verify all three binaries exist before packaging.
#
SERVER_BIN="afpserver/$OBJECTS/afp_server"
CONFIG_BIN="afp_config/$OBJECTS/MacFile"
INSTALLER_BIN="installer/$OBJECTS/MacFileInstaller"

[ -f "$SERVER_BIN" ] || fail "missing $SERVER_BIN"
[ -f "$CONFIG_BIN" ] || fail "missing $CONFIG_BIN"
[ -f "$INSTALLER_BIN" ] || fail "missing $INSTALLER_BIN"

#
# 4. Build distribution/install.zip (the payload the installer unpacks).
#
rm -f distribution/install.zip
( cd "afpserver/$OBJECTS" && zip ../../distribution/install.zip afp_server )
( cd "afp_config/$OBJECTS" && zip ../../distribution/install.zip MacFile )

if [ "$ARCH" = "x86_64" ]; then
	for lib in libcrypto111v.so libssl111v.so; do
		[ -f "deps/openssl/lib/$lib" ] || fail "missing deps/openssl/lib/$lib"
		( cd deps/openssl/lib && zip ../../../distribution/install.zip "$lib" )
	done
fi

[ -f distribution/install.zip ] || fail "failed to create distribution/install.zip"

#
# 5. Stage the release folder and zip it.
#
RELEASE_FOLDER="MacFile_${ARCH}_Release"
RELEASE_ARCHIVE="distribution/${RELEASE_FOLDER}.zip"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

mkdir "$STAGE/$RELEASE_FOLDER"
cp "$INSTALLER_BIN" "$STAGE/$RELEASE_FOLDER/MacFileInstaller"
cp distribution/install-macfile.sh "$STAGE/$RELEASE_FOLDER/"
chmod +x "$STAGE/$RELEASE_FOLDER/install-macfile.sh"
cp distribution/install.zip "$STAGE/$RELEASE_FOLDER/"
cp "distribution/ReadMe!" "$STAGE/$RELEASE_FOLDER/"

rm -f "$RELEASE_ARCHIVE"
( cd "$STAGE" && zip -r "$REPO_ROOT/$RELEASE_ARCHIVE" "$RELEASE_FOLDER" )

[ -f "$RELEASE_ARCHIVE" ] || fail "failed to create $RELEASE_ARCHIVE"

echo "----------------------------------------------------------------"
echo "Done! Release archive: $RELEASE_ARCHIVE"
echo "----------------------------------------------------------------"
