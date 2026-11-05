#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -eo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TemplateDir="$DIR/InitTemplate"

function InitOutputInfo()
{
	if [[ "$TERM" == "dumb" ]]; then
		echo $'\e[1m\e[36m'"$@"$'\e[0m'
	else
		echo $'\e[38;5;39m'"$@"$'\e[0m'
	fi
}

function InitOutputNote()
{
	if [[ "$TERM" == "dumb" ]]; then
		echo $'\e[1m\e[33m'"$@"$'\e[0m'
	else
		echo $'\e[38;5;221m'"$@"$'\e[0m'
	fi
}

function InitOutputHeading()
{
	if [[ "$TERM" == "dumb" ]]; then
		echo $'\e[1m\e[32m'"$@"$'\e[0m'
	else
		echo $'\e[38;5;118m'"$@"$'\e[0m'
	fi
}

function InitOutputError()
{
	if [[ "$TERM" == "dumb" ]]; then
		echo $'\e[1m\e[31m'"$@"$'\e[0m'
	else
		echo $'\e[38;5;198m'"$@"$'\e[0m'
	fi
}

RepoRoot=""
while [[ $# -gt 0 ]]; do
	case "$1" in
		--repo-root)
			if [[ $# -lt 2 ]] || [[ "$2" == --* ]]; then
				InitOutputError "--repo-root requires a URL value"
				exit 1
			fi
			RepoRoot="$2"
			shift 2
			;;
		--repo-root=*)
			RepoRoot="${1#*=}"
			if [[ -z "$RepoRoot" ]]; then
				InitOutputError "--repo-root= requires a URL value"
				exit 1
			fi
			shift
			;;
		*)
			InitOutputError "Unknown argument: $1"
			exit 1
			;;
	esac
done

if [[ -n "$RepoRoot" ]] && [[ "$RepoRoot" != https://* ]]; then
	InitOutputError "--repo-root must be an https:// URL: $RepoRoot"
	exit 1
fi

if [[ "$RepoRoot" == */ ]]; then
	InitOutputError "--repo-root must not end with a trailing slash: $RepoRoot"
	exit 1
fi

# If an explicit --repo-root wasn't given but the environment already overrides
# the default, bake that value into the generated .MBuildSystem so it persists
# after the unset below. Without this, env-only mirror setups would fall back
# to github.com on the following update-repos and on all future runs.
if [[ -z "$RepoRoot" ]] && [[ -n "$MalterlibRepoRoot" ]] && [[ "$MalterlibRepoRoot" != "https://github.com/Malterlib" ]]; then
	RepoRoot="$MalterlibRepoRoot"
fi

ProjectDir="$PWD"
ProjectName="$(basename "$ProjectDir")"
BuildSystemFile="$ProjectDir/${ProjectName}.MBuildSystem"

# If an .MBuildSystem already exists we bail rather than re-run init. This is
# deliberate: init writes the scaffold before `update-repos`, so a failure in
# update-repos leaves a generated .MBuildSystem behind. Resuming by re-running
# init would risk silently overwriting user edits (e.g. ProductCompany,
# ProductCompanyUniqueIdentifier) that we explicitly prompt the user to make.
# Users who want to retry a failed init should delete the generated scaffold
# (or run `./mib update-repos` directly, which works once the scaffold exists).
ExistingBuildSystemFiles=()
for f in "$ProjectDir"/*.MBuildSystem; do
	if [ -e "$f" ]; then
		ExistingBuildSystemFiles+=("$f")
	fi
done

if (( ${#ExistingBuildSystemFiles[@]} > 0 )); then
	InitOutputError "Cannot init: .MBuildSystem file already exists in $ProjectDir:"
	for f in "${ExistingBuildSystemFiles[@]}"; do
		echo "  $f"
	done
	exit 1
fi

echo
InitOutputHeading '╭────────────────────────────────╮'
InitOutputHeading '│ Initializing Malterlib project │'
InitOutputHeading '╰────────────────────────────────╯'
echo

InitOutputInfo "Creating $BuildSystemFile"

{
	echo 'Import "Malterlib/Core/Malterlib.MHeader"'
	echo
	echo 'Property'
	echo '{'
	if [[ -n "$RepoRoot" ]]; then
		printf '\tMalterlibRepoRoot "%s"\n\n' "$RepoRoot"
	fi
	cat <<'EOF'
	ProductCompanyUniqueIdentifier "com.example"
	{
		!!ProductCompanyUniqueIdentifier undefined
	}

	ProductCompany "Example"
	{
		!!ProductCompany undefined
	}

	MalterlibThread_LinuxUseStaticThreadLocal true
	{
		!!MalterlibThread_LinuxUseStaticThreadLocal undefined
	}

	Malterlib_AssumeMalterlibHost true
	{
		!!Malterlib_AssumeMalterlibHost undefined
	}
}
EOF
} > "$BuildSystemFile"

# A Perforce root is a project directory without .git that has a P4CONFIG file. It has no %Repository, so the settings
# that Core's %Repository "." gives a git root go in PerforceRoot, and the template's ignore rules in the ignore file p4 reads.

function IsAbsolutePath()
{
	[[ "$1" == /* || "$1" == \\* || "$1" =~ ^[A-Za-z]:[\\/] ]]
}

# The P4CONFIG files p4 reads in the project directory, the nearest first: the file P4CONFIG names when it is a path, or else
# the file of that name in the project directory and in each directory above it.
function GetP4ConfigFiles()
{
	if [[ -z "$P4ConfigName" ]]; then
		return 0
	fi

	if IsAbsolutePath "$P4ConfigName"; then
		if [[ -f "$P4ConfigName" ]]; then
			echo "$P4ConfigName"
		fi
		return 0
	fi

	local Directory="$ProjectDir"
	while true; do
		if [[ -f "$Directory/$P4ConfigName" ]]; then
			echo "$Directory/$P4ConfigName"
		fi
		local Parent
		Parent="$(dirname "$Directory")"
		if [[ "$Parent" == "$Directory" ]]; then
			return 0
		fi
		Directory="$Parent"
	done
}

# A p4 setting with $home in it replaced by the home directory, as p4 replaces it: USERPROFILE on Windows and HOME elsewhere,
# without a trailing slash.
function ExpandP4Home()
{
	local Value="$1"
	local Home="$HOME"
	if [[ "$OSTYPE" == msys* || "$OSTYPE" == cygwin* ]]; then
		Home="${USERPROFILE:-$HOME}"
	fi
	Home="${Home%/}"
	Home="${Home%\\}"
	# Piece by piece, as a pattern substitution reads an & in the home directory as the match in Bash 5.2 and later, and
	# quoting it there inserts the quotes in Bash 3.2, which is /bin/bash on macOS.
	local Result=""
	while [[ "$Value" == *'$home'* ]]; do
		Result="$Result${Value%%\$home*}$Home"
		Value="${Value#*\$home}"
	done
	echo "$Result$Value"
}

# A p4 setting as p4 takes it in the project directory: from the P4CONFIG files, the nearest first, then the environment, also
# when set empty, then p4 set, whose output on Windows ends its lines with CR. P4CONFIG itself comes from the last two only.
# Without p4 in PATH, what p4 set stored is not considered.
function GetP4Setting()
{
	local Name="$1"
	local Value=""
	if [[ "$Name" != P4CONFIG ]]; then
		local ConfigFile
		# As p4 parses a P4CONFIG file: a CR before the line end and the trailing spaces after it, but not tabs, are no part of
		# a line, and the nearest file that sets the variable wins, also when it sets it empty.
		local Line
		while IFS= read -r ConfigFile; do
			Line="$(sed -e $'s/\r$//' -e 's/ *$//' "$ConfigFile" | grep -m 1 "^$Name=" || true)"
			if [[ -n "$Line" ]]; then
				ExpandP4Home "${Line#*=}"
				return 0
			fi
		done < <(GetP4ConfigFiles)
	fi

	if [[ -n "${!Name+x}" ]]; then
		Value="${!Name}"
	elif command -v p4 > /dev/null 2>&1; then
		Value="$(p4 set -q "$Name" 2> /dev/null | tr -d '\r' | sed -n "s/^$Name=//p" || true)"
	fi
	ExpandP4Home "$Value"
}

# Without P4CONFIG, p4 reads no P4CONFIG file, and mib takes no directory for a Perforce root.
P4ConfigName="$(GetP4Setting P4CONFIG)"
if [[ ! -e "$ProjectDir/.git" ]] && ! command -v p4 > /dev/null 2>&1; then
	InitOutputNote "p4 is not in PATH, so the Perforce settings stored with p4 set are not considered, only the environment and the P4CONFIG files"
fi

# The ignore file p4 reads in the project directory: the first name of P4IGNORE that is not a path, as p4 looks for those in
# every directory above a file. Without P4IGNORE, p4 reads .p4ignore.
P4IgnoreName=".p4ignore"
P4IgnoreNames="$(GetP4Setting P4IGNORE)"
# P4IGNORE=unset turns ignore files off, for p4 and mib alike.
if [[ "$P4IgnoreNames" == "unset" ]]; then
	P4IgnoreName=""
elif [[ -n "$P4IgnoreNames" ]]; then
	P4IgnoreName=""
	if [[ "$OSTYPE" != msys* && "$OSTYPE" != cygwin* ]]; then
		P4IgnoreNames="${P4IgnoreNames//:/;}"
	fi
	IFS=';' read -ra P4IgnoreList <<< "$P4IgnoreNames"
	for Name in "${P4IgnoreList[@]}"; do
		if [[ -n "$Name" && "$Name" != */* && "$Name" != *\\* ]]; then
			P4IgnoreName="$Name"
			break
		fi
	done
fi

# A P4CONFIG that is a path names the file itself, as mib takes it too.
P4ConfigFile=""
if [[ -n "$P4ConfigName" ]]; then
	P4ConfigFile="$ProjectDir/$P4ConfigName"
	if IsAbsolutePath "$P4ConfigName"; then
		P4ConfigFile="$P4ConfigName"
	fi
fi

IsPerforceRoot=false
if [[ ! -e "$ProjectDir/.git" ]] && [[ -n "$P4ConfigFile" ]] && [[ -f "$P4ConfigFile" ]]; then
	IsPerforceRoot=true
elif [[ ! -e "$ProjectDir/.git" ]] && [[ -z "$P4ConfigName" ]] && [[ -f "$ProjectDir/.p4config" ]]; then
	InitOutputNote "The project has a .p4config, but P4CONFIG is not set, so p4 and mib do not use it. For ./mib format and"
	InitOutputNote "./mib tidy to cover the root, set P4CONFIG=.p4config and add PerforceRoot settings to $BuildSystemFile"
fi

if [[ "$IsPerforceRoot" == true ]]; then
	cat >> "$BuildSystemFile" <<'EOF'

Property
{
	// A Perforce root has no %Repository: these are the settings Core's %Repository "." gives a git root. Like a git root,
	// it checks no licenses until CheckLicense and License are set here.
	PerforceRoot {Format: true, Tidy: {Enabled: true, ExcludeFiles: ["BuildSystem/*"]}}
}
EOF
fi

# Core's %Repository "." formats and tidies a git root, but only one that has an origin remote, as a repository needs a URL.
GitRootWithoutOrigin=false
if [[ -e "$ProjectDir/.git" ]] && ! git -C "$ProjectDir" remote get-url origin > /dev/null 2>&1; then
	GitRootWithoutOrigin=true
fi

function CopyTemplateFile()
{
	local Source="$1"
	local Destination="$2"

	if [ -e "$Destination" ]; then
		InitOutputNote "Skipping (already exists): $Destination"
		return
	fi

	InitOutputInfo "Creating $Destination"
	mkdir -p "$(dirname "$Destination")"
	cp "$Source" "$Destination"
}

# Merge Malterlib-specific .gitignore rules into an existing file line-by-line.
# Appending only lines that aren't already present preserves whatever the user
# (or a GitHub language-template) put there while still installing the
# project-generated entries like /BuildSystem, /*.MRepoState, /.cache, etc.
function MergeGitIgnoreFile()
{
	local Source="$1"
	local Destination="$2"

	if [ ! -e "$Destination" ]; then
		InitOutputInfo "Creating $Destination"
		mkdir -p "$(dirname "$Destination")"
		cp "$Source" "$Destination"
		return
	fi

	local MissingLines
	MissingLines=$(grep -Fxv -f "$Destination" "$Source" || true)

	if [ -z "$MissingLines" ]; then
		InitOutputNote "All entries already present: $Destination"
		return
	fi

	InitOutputInfo "Merging missing entries into $Destination"
	{
		# Ensure a newline boundary before the appended block regardless of
		# whether the existing file ends with a trailing newline.
		printf '\n'
		printf '%s\n' "$MissingLines"
	} >> "$Destination"
}

echo
InitOutputInfo "Copying project templates"

MergeGitIgnoreFile "$TemplateDir/.gitignore" "$ProjectDir/.gitignore"
if [[ "$IsPerforceRoot" == true ]]; then
	if [[ -n "$P4IgnoreName" ]]; then
		MergeGitIgnoreFile "$TemplateDir/.gitignore" "$ProjectDir/$P4IgnoreName"
	else
		if [[ "$P4IgnoreNames" == "unset" ]]; then
			InitOutputNote "P4IGNORE is unset, which turns ignore files off, so ./mib format and ./mib tidy would include the generated files."
			InitOutputNote "Set P4IGNORE (or remove the setting to use .p4ignore) and add the entries of $TemplateDir/.gitignore to that file"
		else
			InitOutputNote "P4IGNORE names no ignore file of the project directory ($P4IgnoreNames): add the entries of $TemplateDir/.gitignore to one of its files"
		fi
	fi
fi
CopyTemplateFile "$TemplateDir/.editorconfig" "$ProjectDir/.editorconfig"
CopyTemplateFile "$TemplateDir/.clang-tidy" "$ProjectDir/.clang-tidy"
CopyTemplateFile "$TemplateDir/.malterlib-format" "$ProjectDir/.malterlib-format"
CopyTemplateFile "$TemplateDir/CLAUDE.md" "$ProjectDir/CLAUDE.md"
CopyTemplateFile "$TemplateDir/.vscode/extensions.json" "$ProjectDir/.vscode/extensions.json"
CopyTemplateFile "$TemplateDir/.vscode/settings.json" "$ProjectDir/.vscode/settings.json"

echo
InitOutputInfo "Running ./mib update-repos to download all repositories"
echo

# Force DetectRepoRoot to read from the generated .MBuildSystem so the project
# file is the single source of truth for MalterlibRepoRoot going forward.
unset MalterlibRepoRoot

"$ProjectDir/mib" update-repos

echo
InitOutputInfo "Generating AGENTS.md"
(cd "$ProjectDir" && ./mib update-agents)

echo
InitOutputHeading "Successfully initialized Malterlib project"
echo
echo "Next steps:"
iStep=1
echo "  $((iStep++)). Edit $BuildSystemFile to set ProductCompany and ProductCompanyUniqueIdentifier"
if [[ "$GitRootWithoutOrigin" == true ]]; then
	echo "  $((iStep++)). Add the remote of the project with git remote add origin <URL>: ./mib format and ./mib tidy cover the root only once it has one"
fi
echo "  $((iStep++)). Run ./mib generate to generate the default Tests workspace"
echo "  $((iStep++)). Prompt your favourite agent, for example:"
echo
echo "     Create a distributed app tool that reports the weather."
echo "     * Use the table renderer."
echo "     * Detect the location from the exit IP."
echo "     * Create a new workspace, because this repository was just created."
echo
