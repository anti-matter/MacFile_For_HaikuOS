#!/bin/sh
#
# This script builds all the MacFile binaries and moves them to the distribution directory.
#
# If you tried to install by opening this script from the tracker and it opened in a text editor
# instead of running, then try the following:
#
#	1. Open a terminal window
#	2. In the terminal, navigate to the directory where this script is located
#	3. Type in the following command and hit return
#
#			chmod guoa+x build_macfile.sh
#
#	4. Close the terminal window
#	5. Double click on build-macfile.sh from the tracker again
#

cd $(dirname "$0")

X86_RELEASE_FOLDER=MacFile_x86_Release
X64_RELEASE_FOLDER=MacFile_x86_64_Release
X86_RELEASE_ARCHIVE=$X86_RELEASE_FOLDER.zip
X64_RELEASE_ARCHIVE=$X64_RELEASE_FOLDER.zip

function clean_release {
	if [ -e "objects.x86-cc2-release" ]; then
		rm -r objects.x86-cc2-release
	fi
	
	if [ -e "objects.x86_64-cc13-release" ]; then
		rm -r objects.x86_64-cc13-release
	fi
}

echo -n "Build for release? (y/n): "
read buildrelease

if [ "$buildrelease" = "y" ]
then
	echo Building for release...
	
	cd afpserver
	clean_release;
	
	make

	cd ../afp_config
	clean_release;
	
	make

	cd ..
	
	#Create the installation zip that contains the applications
	if [ -e distribution/install.zip ]; then
		rm distribution/install.zip
	fi
	
	#Depending on what our platform...
	if [ -e "afpserver/objects.x86-cc2-release" ]
	then
		cd afpserver/objects.x86-cc2-release/
		zip ../../distribution/install.zip afp_server
		cd ../..
		
		cd afp_config/objects.x86-cc2-release/
		zip ../../distribution/install.zip MacFile
		cd ../..
		
		#Now create the release archive
		if [ -e $X86_RELEASE_ARCHIVE ]; then
			rm $X86_RELEASE_ARCHIVE
		fi
		
		mkdir $X86_RELEASE_FOLDER
		cp distribution/* $X86_RELEASE_FOLDER
		
		zip -r $X86_RELEASE_ARCHIVE $X86_RELEASE_FOLDER/
		
		rm -r $X86_RELEASE_FOLDER
	else
		cd afpserver/objects.x86_64-cc13-release/
		zip ../../distribution/install.zip afp_server
		cd ../..
		
		cd afp_config/objects.x86_64-cc13-release/
		zip ../../distribution/install.zip MacFile
		cd ../..
		
		cd deps/openssl/lib
		zip ../../../distribution/install.zip libcrypto111v.so
		cd ../../..
		
		cd deps/openssl/lib
		zip ../../../distribution/install.zip libssl111v.so
		cd ../../..

		#Now create the release archive
		if [ -e $X64_RELEASE_ARCHIVE ]; then
			rm $X64_RELEASE_ARCHIVE
		fi
		
		mkdir $X64_RELEASE_FOLDER
		cp distribution/* $X64_RELEASE_FOLDER
		
		zip -r $X64_RELEASE_ARCHIVE $X64_RELEASE_FOLDER/
		
		rm -r $X64_RELEASE_FOLDER
	fi
		
else
	echo "ERROR: We don't package for debug builds!"
fi

echo ----------------------------------------------------------------
echo "Done! Archive should now be available in the distribution folder"
echo ----------------------------------------------------------------
							 
