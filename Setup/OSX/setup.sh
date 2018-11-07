#!/bin/bash

cd "$( dirname "${BASH_SOURCE[0]}" )"

ScriptDir="$PWD"

set -e

if [ -e ../../Scripts/Detect.sh ]; then
	source ../../Scripts/Detect.sh
elif [[ "$MToolPath" != "" ]]; then
	MToolExecutable="$MToolPath/MTool"
else
	MToolExecutable="$ScriptDir/MTool"
fi

if [[ "$MalterlibCompiledFiles" != "" ]]; then
	DependenciesDirectory="$MalterlibCompiledFiles/Dependencies"
else
	DependenciesDirectory=
	if [ -d /CompiledFiles ]; then
		DependenciesDirectory="/CompiledFiles/Dependencies"
	else
		DependenciesDirectory="$HOME/.CompiledFiles/Dependencies"
	fi
fi

DependenciesVersion=1
DependenciesFile="$DependenciesDirectory/MalterlibDependencies.ver"

Setting_Plugin_Malterlib=true
Setting_Plugin_NavigationFixes=true
Setting_Plugin_CustomizeAnnotations=true
Setting_Plugin_HideDistractions=true
Setting_Plugin_P4Checkout=false
Setting_SyntaxHighlight=true

VersionLessThanEqual() {
    [  "$1" = "`printf "$1\n$2" | sort -V | head -n1`" ]
}

VersionLessThan() {
    [ "$1" = "$2" ] && return 1 || VersionLessThanEqual $1 $2
}

SignXcode()
{
	XcodeLocation=$1

	echo Signing Xcode: $XcodeLocation

	if [ ! -e "$XcodeLocation" ] ; then
		echo "No Xcode found at $XcodeLocation"
		return 0
	fi

	XcodeVersion=`defaults read "$XcodeLocation/Contents/version.plist" CFBundleShortVersionString`
	if VersionLessThan $XcodeVersion 8.0 ; then
		echo "Skipping unsigning because only 8.0 or later needs it $XcodeLocation"
		return 0
	fi

	if [ ! -e "$XcodeLocation/Contents/unsigned" ] ; then
		pwd
		pushd "$TMPDIR"
			echo Signing Xcode
			sudo cp -f "$XcodeLocation/Contents/MacOS/Xcode" .
			sudo codesign -s - -f --timestamp=none "Xcode"
			sudo cp -f Xcode "$XcodeLocation/Contents/MacOS/"

			echo Signing xcodebuild
			sudo cp -f "$XcodeLocation/Contents/Developer/usr/bin/xcodebuild" .
			sudo codesign -s - -f --timestamp=none "xcodebuild"
			sudo cp -f xcodebuild "$XcodeLocation/Contents/Developer/usr/bin/"
		popd

		echo Removivg xattrs
		sudo xattr -rc "$XcodeLocation"
		echo Adding Xcode with spctl
		sudo spctl --add --label "Xcode" "$XcodeLocation"
		sudo touch "$XcodeLocation/Contents/unsigned"
	fi

	if ! "$XcodeLocation/Contents/Developer/usr/bin/xcodebuild" -version ; then
		echo If this fails you need to reboot computer and run script again
	fi
}

UpdatePorts()
{
	echo Updating ports

	if ! which port ; then
		Cannot find MacPorts, please install from https://www.macports.org/install.php
	fi

	sudo "$MToolExecutable" AddPortSource "Source=file://$ScriptDir/MacPorts/"

	sudo port -N selfupdate

	# To build clang
	sudo port -N install cmake

	if [ "`sudo port outdated`" != "No installed ports are outdated." ]; then
		sudo port -N upgrade outdated
	fi

	# To run linux tools
	sudo port -N -s install i386-elf-binutils x86_64-elf-binutils

	# For meteor manager
	sudo port -N uninstall installed and npm installed and npm-devel installed and nodejs installed and nodejs-devel installed and nodejs6 installed and npm3 || true

	sudo port -N install nodejs8 npm5

	# Bring in dependencies for our custom build of doxygen
	sudo port -N install cmake perl5 python27 flex bison libpng libiconv graphviz

	sudo port -N install ninja

	if ! which go ; then
		sudo port -N install go
	fi

	if ! which brew > /dev/null ; then
		/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
	fi

	brew update
	brew upgrade
	brew install ruby
	gem install -n /usr/local/bin rubygems-update
	gem install -n /usr/local/bin xcpretty
	update_rubygems --silent
	gem update -n /usr/local/bin --system
}

UpdateXCTool()
{
	if [ ! -e "$DependenciesDirectory/xctool_versiontag" ] ; then
		if [ -e "$DependenciesDirectory/xctool" ] ; then
			echo Wrong version, removing old xctool
			rm -rf "$DependenciesDirectory/xctool"
		fi
	fi

	if [ ! -e "$DependenciesDirectory/xctool" ] ; then
		mkdir -p "$DependenciesDirectory"
		pushd "$DependenciesDirectory" > /dev/null
		git clone https://github.com/facebook/xctool.git
		popd > /dev/null
	else
		pushd "$DependenciesDirectory/XCTool" > /dev/null
		git pull
		popd > /dev/null
	fi 

	echo 1 > "$DependenciesDirectory/xctool_versiontag"

	pushd "$DependenciesDirectory/xctool" > /dev/null
	./xctool.sh -version
	popd > /dev/null
}

UpdateXcodePlugins()
{
	XcodeVersion=`defaults read "$2/Contents/version.plist" CFBundleShortVersionString`
	XcodeVersionSplit=( ${XcodeVersion//./ } )
	XcodeVersionCompact=${XcodeVersionSplit[0]}${XcodeVersionSplit[1]}
	if VersionLessThan $XcodeVersionCompact 92 ; then
		echo "Too old ($XcodeVersionCompact), plugins not supported"
		return 0
	fi

	RepositoryDirectory="$DependenciesDirectory/XcodePatches/$1"

	if [ ! -e "$RepositoryDirectory/MalterlibXcodePatches" ] ; then
		mkdir -p "$RepositoryDirectory"
		pushd "$RepositoryDirectory" > /dev/null
		git clone https://github.com/Malterlib/MalterlibXcodePatches.git
		popd > /dev/null
	else
		pushd "$RepositoryDirectory/MalterlibXcodePatches" > /dev/null
		git fetch
		popd > /dev/null
	fi

	pushd "$RepositoryDirectory/MalterlibXcodePatches" > /dev/null
	XcodeBranch=xcode${XcodeVersionCompact}
	git checkout -f -B $XcodeBranch origin/$XcodeBranch
	git clean -fd
	popd > /dev/null

	echo 1 > "$RepositoryDirectory/xcodeplugins_versiontag"

	pushd "$RepositoryDirectory/MalterlibXcodePatches/Plugins" > /dev/null

	"$2/Contents/Developer/usr/bin/xcodebuild" -quiet -workspace "XcodePlugins.xcworkspace" -scheme "Release All" clean 

	if $Setting_Plugin_Malterlib; then
		echo Installing Malterlib plugin
		"$2/Contents/Developer/usr/bin/xcodebuild" -quiet -workspace "XcodePlugins.xcworkspace" -scheme "Release Plugin_Malterlib"
	fi
	if $Setting_Plugin_NavigationFixes; then
		echo Installing NavigationFixes plugin
		"$2/Contents/Developer/usr/bin/xcodebuild" -quiet -workspace "XcodePlugins.xcworkspace" -scheme "Release Plugin_NavigationFixes"
	fi
	if $Setting_Plugin_CustomizeAnnotations; then
		echo Installing CustomizeAnnotations plugin
		"$2/Contents/Developer/usr/bin/xcodebuild" -quiet -workspace "XcodePlugins.xcworkspace" -scheme "Release Plugin_CustomizeAnnotations"
	fi
	if $Setting_Plugin_HideDistractions; then
		echo Installing HideDistractions plugin
		"$2/Contents/Developer/usr/bin/xcodebuild" -quiet -workspace "XcodePlugins.xcworkspace" -scheme "Release Plugin_HideDistractions"
	fi
	if $Setting_Plugin_P4Checkout; then
		echo Installing P4Checkout plugin
		"$2/Contents/Developer/usr/bin/xcodebuild" -quiet -workspace "XcodePlugins.xcworkspace" -scheme "Release Plugin_P4Checkout"
	fi
	if $Setting_Plugin_HideDistractions; then
		echo Installing HideDistractions plugin
		"$2/Contents/Developer/usr/bin/xcodebuild" -quiet -workspace "XcodePlugins.xcworkspace" -scheme "Release Plugin_HideDistractions"
	fi

	popd > /dev/null

	if $Setting_SyntaxHighlight; then
		echo Installing Malterlib systax highligting
		pushd "$RepositoryDirectory/MalterlibXcodePatches/SyntaxColoring" > /dev/null
		./install.sh "$XcodeLocation"
		popd > /dev/null
	fi
}

UpdateXcode()
{
	XcodeLocation=$1

	if [ ! -e "$XcodeLocation" ] ; then
		echo "No Xcode found at $XcodeLocation"
		return 0
	fi

	XcodeVersion=`$XcodeLocation/Contents/Developer/usr/bin/xcodebuild -version | grep Xcode | awk -F ' ' {'print $2'} | awk -F . {'print $1 "." $2'}`
	echo Xcode version: $XcodeVersion

	UpdateXcodePlugins $XcodeVersion "$XcodeLocation"
}

function UpdateAllXcode()
{
	for File in /Applications/Xcode*.app; do
		UpdateXcode "$File"
	done
}

function UnsignAllXcode()
{
	for File in /Applications/Xcode*.app; do
		SignXcode "$File"
	done
}

function InstallMeteor()
{
	if ! which meteor ; then
		curl https://install.meteor.com/ | sh
	fi
}

AskSettings=true

if [ "$1" == "--all" ] ; then
	shift
	Setting_Plugin_Malterlib=true
	Setting_Plugin_NavigationFixes=true
	Setting_Plugin_CustomizeAnnotations=true
	Setting_Plugin_HideDistractions=true
	Setting_Plugin_P4Checkout=true
	Setting_SyntaxHighlight=true
	AskSettings=false
elif [ "$1" == "--default" ] ; then
	shift
	AskSettings=false
elif [ "$1" == "--none" ] ; then
	shift
	AskSettings=false
	Setting_Plugin_Malterlib=false
	Setting_Plugin_NavigationFixes=false
	Setting_Plugin_CustomizeAnnotations=false
	Setting_Plugin_HideDistractions=false
	Setting_Plugin_P4Checkout=false
	Setting_SyntaxHighlight=false
fi

function AskForSetting()
{
	Default=$1
	echo >&2
	echo "$2" >&2
	echo >&2
	if $Default;then
		Prompt=$'[\e[38;5;46mY\e[39m/n]'
	else
		Prompt=$'[y/\e[38;5;46mN\e[39m]'
	fi

	read -p "$3 $Prompt " Answer
	if [[ "$Answer" == "" ]]; then
		echo $Default
		return
	fi
	if [[ "$Answer" == "y" || "$Answer" == "Y" ]]; then
		echo true
		return
	fi 
	echo false
	return
}

function Divider()
{
	echo 
	echo -------------------------------------------------------------------------------------------------------------------------------------
}

function DoInstall()
{
	if $AskSettings; then
		
		Divider
		echo 
		echo To install default plugins and highlighting specify --default to setup
		echo To install all plugins and highlighting specify --all to setup
		echo To install no plugins or highlighting specify --none to setup
		Divider

		Setting_Plugin_Malterlib=$(AskForSetting $Setting_Plugin_Malterlib $'The \e[38;5;39mMalterlib\e[39m plugin (https://github.com/Malterlib/MalterlibXcodePatches/blob/xcode92/Plugins/Plugin_Malterlib/README.md):\n   * Allow Xcode to automatically reload without prompts after build system generation\n   * Run multiple executables with ⌘-G\n   * Configure extra debug settings with Ctrl+O\n   * Disables buggy build queue throttling' $'Install \e[38;5;39mMalterlb\e[39m Xcode plugin?')

		Divider

		Setting_Plugin_NavigationFixes=$(AskForSetting $Setting_Plugin_NavigationFixes $'The \e[38;5;39mNavigationFixes\e[39m plugin (https://github.com/Malterlib/MalterlibXcodePatches/blob/xcode92/Plugins/Plugin_NavigationFixes/README.md):\n   * Navigate the Xcode Navigators with ⌘-N / ⌘-Shift-N\n   * Visual Studio like keyboard navigation for whole word movements\n   * Disable unhelpful automatic format when pasting\n   * Navigate to file:line from execution output by double-clicking' $'Install \e[38;5;39mNavigationFixes\e[39m Xcode plugin?')

		Divider
		
		Setting_SyntaxHighlight=$(AskForSetting $Setting_SyntaxHighlight $'\e[38;5;39mMalterlib Syntax Highlighting\e[39m (http://docs.malterlib.org/p__malterlib__core__code_standard__coloring.html):\n   * Use syntax highlighting taking advantage of the Malterlib code naming standard' $'Install \e[38;5;39mMalterlb Syntax Highlighting\e[39m for Xcode?')

		Divider
		
		Setting_Plugin_CustomizeAnnotations=$(AskForSetting $Setting_Plugin_CustomizeAnnotations $'The \e[38;5;39mCustomizeAnnotations\e[39m plugin (https://github.com/Malterlib/MalterlibXcodePatches/blob/xcode92/Plugins/Plugin_CustomizeAnnotations/README.md):\n   * Color errors/warnings in editor to make them easier to read with a dark color scheme' $'Install \e[38;5;39mCustomizeAnnotations\e[39m Xcode plugin?')

		Divider
		
		Setting_Plugin_HideDistractions=$(AskForSetting $Setting_Plugin_HideDistractions $'The \e[38;5;39mHideDistractions\e[39m plugin (https://github.com/Malterlib/MalterlibXcodePatches/blob/xcode92/Plugins/Plugin_HideDistractions/README.md):\n   * Toggle editor only mode with ⌘-Shift-G (default, can be remapped)' $'Install \e[38;5;39mHideDistractions\e[39m Xcode plugin?')

		Divider
		
		Setting_Plugin_P4Checkout=$(AskForSetting $Setting_Plugin_P4Checkout $'The \e[38;5;39mP4Checkout\e[39m plugin (https://github.com/Malterlib/MalterlibXcodePatches/blob/xcode92/Plugins/Plugin_P4Checkout/README.md):\n   * Automatically check out files in Perfoce when editing them' $'Install \e[38;5;39mP4Checkout\e[39m Xcode plugin?')

		Divider

	fi
	UpdatePorts
	#UpdateXCTool
	InstallMeteor
	if $Setting_Plugin_Malterlib || $Setting_Plugin_NavigationFixes || $Setting_Plugin_CustomizeAnnotations || $Setting_Plugin_HideDistractions || $Setting_Plugin_P4Checkout ; then
		UnsignAllXcode
	fi
	UpdateAllXcode

	mkdir -p "$DependenciesDirectory"
	echo $DependenciesVersion > "$DependenciesFile"
}

function CheckSetup()
{
	if [ -f "$DependenciesFile" ]; then
		CurrentVersion=`cat "$DependenciesFile"`
		if (( $CurrentVersion >= $DependenciesVersion )); then
			return 0
		fi
	fi

	echo 
	echo To install/update dependencies needed to build in Xcode, you need to run:
	echo 
	echo ./mib setup 
	echo 
	exit 1
}

if [ "$1" != "" ] ; then
	"$@"
else
	DoInstall
	echo Successful
fi
