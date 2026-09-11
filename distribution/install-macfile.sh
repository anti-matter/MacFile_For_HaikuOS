#!/bin/sh
#
# This script installs the MacFile application and associated files to your Haiku system.
#
# If you tried to install by opening this script from the tracker and it opened in a text editor
# instead of running, then try the following:
#
#	1. Open a terminal window
#	2. In the terminal, navigate to the directory where this script is located
#	3. Type in the following command and hit return
#
#			chmod guoa+x install-macfile.sh
#
#	4. Close the terminal window
#	5. Double click on install-macfile.sh from the tracker again
#
# NOTE: This script performs a "clean install", so if your installation becomes corrupted,
#       you can re-run this script to get everything back to normal (note that your settings
#       are preserved).
#
# Usage:
#
#	install-macfile.sh              Interactive mode (GUI alerts; the original behavior).
#	install-macfile.sh install      Non-interactive install, machine-readable output.
#	install-macfile.sh upgrade      Non-interactive upgrade: replace an existing
#	                                install, preserving your configuration.
#	install-macfile.sh uninstall    Non-interactive uninstall, machine-readable output.
#	install-macfile.sh status       Report whether MacFile is currently installed.
#	install-macfile.sh help         Print this usage text.
#
# Non-interactive mode emits a line-based protocol on stdout so a GUI frontend
# (the MacFileInstaller app) can drive a progress bar without scraping prompts:
#
#	PROGRESS <0-100> <label>
#	INFO <message>
#	ERROR <message>
#	DONE success
#	DONE failure
#
# The exit code is 0 on success and non-zero on failure.
#

cd $(dirname "$0")

LINK_PREF_DIRECTORY=/boot/home/config/non-packaged/data/deskbar/menu/Preferences
LINK_APPL_DIRECTORY=/boot/home/config/non-packaged/data/deskbar/menu/Applications
BIN_DIRECTORY=/boot/home/config/non-packaged/apps
LIB_DIRECTORY=/boot/home/config/non-packaged/lib
ADDON_DIRECTORY=/boot/home/config/non-packaged/add-ons/Tracker
LAUNCH_DIRECTORY=/boot/home/config/settings/boot/launch

ARCHIVEDIR=`dirname "$0"`
ARCHIVE="install.zip"

# PROTOCOL=1 when running as a non-interactive backend (install/uninstall/status),
# so that the machine-readable lines below are emitted. Interactive mode (PROTOCOL=0)
# keeps the original alert-based flow and stays quiet on stdout.
PROTOCOL=0

emit_progress()
{
	if [ "$PROTOCOL" = "1" ]; then
		echo "PROGRESS $1 $2"
	fi
}

emit_info()
{
	if [ "$PROTOCOL" = "1" ]; then
		echo "INFO $1"
	fi
}

emit_error()
{
	if [ "$PROTOCOL" = "1" ]; then
		echo "ERROR $1"
	fi
}

# fail <message>
# Report a failure through the correct channel and exit non-zero. In protocol mode
# this emits an ERROR line plus a terminal "DONE failure" line; in interactive mode
# it raises a stop alert. Both paths exit non-zero so a caller never sees a failure
# reported as success.
fail()
{
	msg="$1"
	if [ "$PROTOCOL" = "1" ]; then
		echo "ERROR $msg"
		echo "DONE failure"
	else
		echo "$msg"
		alert --stop "$msg" "Quit"
	fi
	exit 1
}

start()
{
	if [ -f "$1" ]; then
		"$1" "$2" &
	else
		echo There is no "$1"
	fi
}

# waitForServer
# Bounded liveness check for afp_server. A bare `waitfor afp_server` blocks until the
# application is running, which would hang a GUI caller if the server fails to start.
# Instead we poll the process list for up to ~10 seconds and report success/failure.
waitForServer()
{
	tries=0
	while [ $tries -lt 10 ]; do
		if ps 2>/dev/null | grep -v grep | grep -q "afp_server"; then
			return 0
		fi
		sleep 1
		tries=$((tries + 1))
	done
	return 1
}

function installFiles {
	emit_progress 10 "Installing application binaries..."
	#install file system and utilities
	unzip -o "$ARCHIVEDIR/$ARCHIVE" -d $BIN_DIRECTORY || fail "Failed to extract \"$ARCHIVE\" into $BIN_DIRECTORY."

	emit_progress 25 "Placing OpenSSL libraries..."
	mv -f $BIN_DIRECTORY/libcrypto111v.so $LIB_DIRECTORY/libcrypto.so.1.1
	mv -f $BIN_DIRECTORY/libssl111v.so $LIB_DIRECTORY/libssl.so.1.1

	#
	#CreateAfpShare is a tracker add-on (a shared library the tracker loads
	#and calls process_refs() on), not an application, so it goes in the
	#add-ons directory, not the apps directory.
	#
	emit_progress 35 "Installing the 'Share with Macs (AppleShare)' add-on..."
	mkdir -p $ADDON_DIRECTORY
	mv -f "$BIN_DIRECTORY/Share with Macs (AppleShare)" "$ADDON_DIRECTORY/Share with Macs (AppleShare)"

	emit_progress 40 "Verifying installed files..."
	#make sure the files got installed properly
	if [ ! -e "$BIN_DIRECTORY/MacFile" ]; then
		fail "Installation failed, configuration app failed to install!!"
	fi

	if [ ! -e "$BIN_DIRECTORY/afp_server" ]; then
		fail "Installation failed, afp_server failed to install!!"
	fi

	if [ ! -e "$ADDON_DIRECTORY/Share with Macs (AppleShare)" ]; then
		fail "Installation failed, the CreateAfpShare add-on failed to install!!"
	fi

	emit_progress 55 "Configuring menu links and startup..."
	mkdir -p $LINK_PREF_DIRECTORY
	mkdir -p $LINK_APPL_DIRECTORY

	#creat links in the right places for easy access and launching at startup
	ln -s  -f $BIN_DIRECTORY/MacFile $LINK_PREF_DIRECTORY/MacFile
	ln -s  -f $BIN_DIRECTORY/afp_server $LINK_APPL_DIRECTORY/afp_server
	ln -s  -f $BIN_DIRECTORY/afp_server $LAUNCH_DIRECTORY/afp_server

	emit_progress 70 "Installation of files complete."
}

function removeFiles {
	emit_progress 20 "Removing installed files..."
	#remove old afp_server application
	rm -fv $BIN_DIRECTORY/afp_server
	rm -fv $BIN_DIRECTORY/MacFile
	rm -fv "$ADDON_DIRECTORY/Share with Macs (AppleShare)"
	rm -fv $LIB_DIRECTORY/libcrypto.so.1.1
	rm -fv $LIB_DIRECTORY/libssl.so.1.1

	rm -fv $LAUNCH_DIRECTORY/afp_server

	rm -fv $LINK_APPL_DIRECTORY/afp_server
	rm -fv $LINK_PREF_DIRECTORY/MacFile

	emit_progress 40 "Removal of files complete."
}

function uninstall {
	#Shutdown the current MacFile (afp_server) process
	emit_info "Shutting down the MacFile server..."
	quit application/x-vnd.afp_server

	removeFiles

	emit_progress 60 "Uninstall complete."
}

# ---------------------------------------------------------------------------
# Non-interactive backend entry points (driven by the MacFileInstaller GUI).
# ---------------------------------------------------------------------------

# verifyPayload
# Confirm the payload archive actually contains every file the install needs,
# so we never tear down a working installation only to discover the
# replacement payload is missing or corrupt. Fails (leaving the existing
# installation untouched) if the archive cannot be read or a required entry is
# absent. The OpenSSL libraries are only present in x86_64 payloads, so they
# are not required here -- this mirrors the post-extraction checks in
# installFiles, which verify the same three core components.
verifyPayload()
{
	listing=$(unzip -l "$ARCHIVEDIR/$ARCHIVE" 2>/dev/null)
	if [ -z "$listing" ]; then
		fail "The payload archive \"$ARCHIVE\" could not be read. It may be corrupt. $OPERATION aborted."
	fi

	for name in "afp_server" "MacFile" "Share with Macs (AppleShare)"; do
		if ! printf '%s\n' "$listing" | grep -Fq "$name"; then
			fail "The payload archive \"$ARCHIVE\" is missing \"$name\". $OPERATION aborted."
		fi
	done
}

# install_sequence <operation-word>
# The shared install/upgrade sequence. <operation-word> ("Installation" or
# "Upgrade") is used in the bookend progress labels. A fresh install and an
# upgrade run the same steps: verify the payload, stop the server, remove the
# installed program components (the user's configuration in ~/.settings/ is
# never touched), install the components from the current payload, and restart
# the server.
install_sequence()
{
	OPERATION="$1"

	#Make sure the zip file with the app bits is present and complete before
	#we modify the existing installation.
	if [ ! -e "$ARCHIVEDIR/$ARCHIVE" ]; then
		fail "The file \"$ARCHIVE\" that contains the application binaries is missing. $OPERATION aborted."
	fi
	verifyPayload

	#We must shut down afp_server before replacing the installed binaries.
	emit_info "Shutting down the MacFile server..."
	quit application/x-vnd.afp_server

	#Remove the installed program components, then install the components from
	#the current payload. removeFiles only removes program files -- the user's
	#configuration in ~/.settings/ is never touched, so an upgrade leaves the
	#server configured exactly as it was.
	removeFiles
	installFiles

	echo "PROGRESS 80 Starting the MacFile server"
	#start the server
	start $BIN_DIRECTORY/afp_server
	if ! waitForServer; then
		fail "MacFile was installed, but afp_server did not start. Check that it is installed properly."
	fi
	echo "INFO afp_server has been started"

	echo "PROGRESS 100 $OPERATION complete"
	echo "DONE success"
	exit 0
}

cmd_install()
{
	PROTOCOL=1
	echo "PROGRESS 5 Preparing installation"
	install_sequence "Installation"
}

cmd_upgrade()
{
	PROTOCOL=1
	echo "PROGRESS 5 Preparing upgrade"
	install_sequence "Upgrade"
}

cmd_uninstall()
{
	PROTOCOL=1
	echo "PROGRESS 5 Preparing uninstall"

	#Make sure the zip file with the app bits is present (it is the source of truth
	#for what an installation looks like, and keeps the two modes symmetric).
	if [ ! -e "$ARCHIVEDIR/$ARCHIVE" ]; then
		fail "The file \"$ARCHIVE\" is missing; cannot verify the installation to remove."
	fi

	uninstall

	echo "PROGRESS 100 Uninstall complete"
	echo "DONE success"
	exit 0
}

cmd_status()
{
	PROTOCOL=1
	if [ -e "$BIN_DIRECTORY/afp_server" ]; then
		echo "STATUS installed"
	else
		echo "STATUS not_installed"
	fi
	exit 0
}

cmd_help()
{
	cat <<'EOF'
MacFile for Haiku installer

Usage:
	install-macfile.sh              Interactive install/uninstall (GUI alerts).
	install-macfile.sh install      Non-interactive install (machine-readable output).
	install-macfile.sh upgrade      Non-interactive upgrade (replace an existing
	                                install, preserving your configuration).
	install-macfile.sh uninstall    Non-interactive uninstall (machine-readable output).
	install-macfile.sh status       Report whether MacFile is currently installed.
	install-macfile.sh help         Show this help.

Non-interactive mode emits PROGRESS / INFO / ERROR / DONE lines on stdout and
exits 0 on success, non-zero on failure.
EOF
	exit 0
}

# ---------------------------------------------------------------------------
# Subcommand dispatch.
# ---------------------------------------------------------------------------

case "$1" in
	install)
		cmd_install
		;;
	upgrade)
		cmd_upgrade
		;;
	uninstall)
		cmd_uninstall
		;;
	status)
		cmd_status
		;;
	help|-h|--help)
		cmd_help
		;;
	"")
		# Interactive mode (the original behavior).
		:
		;;
	*)
		echo "Unknown command: $1"
		echo "Try 'install-macfile.sh help' for usage."
		exit 1
		;;
esac

# ---------------------------------------------------------------------------
# Interactive flow (only reached when no subcommand was given).
# ---------------------------------------------------------------------------

#Make sure the zip file with the app bits is present
if [ ! -e "$ARCHIVEDIR/$ARCHIVE" ]; then
	msg="The file \"$ARCHIVE\" that contains the application binaries is missing. Installation aborted."
	alert --stop "$msg" "Quit"
	exit 1
fi

#We must shut down afp_server before installing the new one.
msg="If you already have MacFile installed, the current server will be shut down and connected users will be disconnected. Do you wish to continue?"
result=$(alert --stop "$msg" "Uninstall" "Cancel" "Install")
if [ "$result" = "Cancel" ]; then
	exit 0
fi

#If the user wants to uninstall, make sure, then do it.
if [ "$result" = "Uninstall" ]; then
	result=$(alert --stop "Are you sure you want to completely remove MacFile Server for Haiku?" "Cancel" "Remove")
	if [ "$result" = "Remove" ]; then
		#Shutdown the current MacFile (afp_server) process
		echo shutting down the MacFile server...
		quit application/x-vnd.afp_server

		removeFiles

		(alert --info "The MacFile Server for Haiku has been removed. Click OK to exit." "OK")
		exit 0
	fi

	(alert --info "Removal of the MacFile Server has been cancelled leaving your installation untouched. Click OK to exit." "OK")
	exit 0
fi

#Shutdown the current MacFile (afp_server) process
echo shutting down the MacFile server...
quit application/x-vnd.afp_server

removeFiles
installFiles

#start the server
start $BIN_DIRECTORY/afp_server
if ! waitForServer; then
	fail "MacFile was installed, but afp_server did not start. Check that it is installed properly."
fi
echo afp_server has been started

result=$(alert --info $'MacFile has been installed and is now running. Would you like to open the configuration tool?' "No" "Yes")
if [ "$result" = "Yes" ]; then
	start $BIN_DIRECTORY/MacFile
fi

result=$(alert --info $'MacFile has been successfully installed.\nPlease read the release notes for further information.' "Open Release Notes" "Done")
if [ "$result" = "Open Release Notes" ] ; then
	open "$ARCHIVEDIR/ReadMe!"
fi
