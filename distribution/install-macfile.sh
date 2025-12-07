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

cd $(dirname "$0")

LINK_PREF_DIRECTORY=/boot/home/config/non-packaged/data/deskbar/menu/Preferences
LINK_APPL_DIRECTORY=/boot/home/config/non-packaged/data/deskbar/menu/Applications
BIN_DIRECTORY=/boot/home/config/non-packaged/apps
LIB_DIRECTORY=/boot/home/config/non-packaged/lib
LAUNCH_DIRECTORY=/boot/home/config/settings/boot/launch

ARCHIVEDIR=`dirname "$0"`
ARCHIVE="install.zip"

start()
{ 
	if [ -f $1 ]; then 
	        $1 $2 & 
	else 
	        echo There is no $1 
	fi 
}

function installFiles {
	#install file system and utilities
	unzip -o "$ARCHIVEDIR/$ARCHIVE" -d $BIN_DIRECTORY
	mv -f $BIN_DIRECTORY/libcrypto111v.so $LIB_DIRECTORY/libcrypto.so.1.1
	mv -f $BIN_DIRECTORY/libssl111v.so $LIB_DIRECTORY/libssl.so.1.1

	#make sure the files got installed properly
	if [ ! -e "$BIN_DIRECTORY/MacFile" ]; then
		msg="Installation failed, configuration app failed to install!!"
		alert --stop "$msg" "Quit"
		exit 0
	fi

	if [ ! -e "$BIN_DIRECTORY/afp_server" ]; then
		msg="Installation failed, afp_server failed to install!!"
		alert --stop "$msg" "Quit"
		exit 0
	fi
	
	mkdir -p $LINK_PREF_DIRECTORY
	mkdir -p $LINK_APPL_DIRECTORY

	#creat links in the right places for easy access and launching at startup
	ln -s  -f $BIN_DIRECTORY/MacFile $LINK_PREF_DIRECTORY/MacFile
	ln -s  -f $BIN_DIRECTORY/afp_server $LINK_APPL_DIRECTORY/afp_server
	ln -s  -f $BIN_DIRECTORY/afp_server $LAUNCH_DIRECTORY/afp_server
}

function removeFiles {
	#remove old afp_server application
	rm -fv $BIN_DIRECTORY/afp_server
	rm -fv $BIN_DIRECTORY/MacFile
	rm -fv $LIB_DIRECTORY/libcrypto.so.1.1
	rm -fv $LIB_DIRECTORY/libssl.so.1.1
	
	rm -fv $LAUNCH_DIRECTORY/afp_server
	
	rm -fv $LINK_APPL_DIRECTORY/afp_server
	rm -fv $LINK_PREF_DIRECTORY/MacFile
}

function uninstall {
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
}

#Make sure the zip file with the app bits is present
if [ ! -e "$ARCHIVEDIR/$ARCHIVE" ]; then
	msg="The file \"$ARCHIVE\" that contains the application binaries is missing. Installation aborted."
	alert --stop "$msg" "Quit"
	exit 0
fi

#We must shut down afp_server before installing the new one.
msg="If you already have MacFile installed, the current server will be shut down and connected users will be disconnected. Do you wish to continue?"
result=$(alert --stop "$msg" "Uninstall" "Cancel" "Install")
if [ "$result" = "Cancel" ]; then
	exit 0
fi

#If the user wants to uninstall, make sure, then do it.
if [ "$result" = "Uninstall" ]; then
	uninstall
fi

#Shutdown the current MacFile (afp_server) process
echo shutting down the MacFile server...
quit application/x-vnd.afp_server

removeFiles
installFiles

#start the server
start $BIN_DIRECTORY/afp_server
waitfor afp_server
echo afp_server has been started

result=$(alert --info $'MacFile has been installed and is now running. Would you like to open the configuration tool?' "No" "Yes")
if [ "$result" = "Yes" ]; then
	start $BIN_DIRECTORY/MacFile
fi

result=$(alert --info $'MacFile has been successfully installed.\nPlease read the release notes for further information.' "Open Release Notes" "Done")
if [ "$result" = "Open Release Notes" ] ; then	
	open "$ARCHIVEDIR/ReadMe!"
fi
