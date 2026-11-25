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

# A Perforce root is a project directory without .git that has a .p4config file. It has no %Repository, so the settings that
# Core's %Repository "." gives a git root go in PerforceRoot, and the template's ignore rules in the ignore file p4 reads.

# The project directory's .p4config, whatever P4CONFIG is, when it is a regular file, also through a symbolic link: like mib, init
# reads no P4CONFIG file outside the project, where p4 would read those of every directory above it.
P4ConfigFile=""
if [[ -f "$ProjectDir/.p4config" ]]; then
	P4ConfigFile="$ProjectDir/.p4config"
fi

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

# Runs p4 with P4CONFIG naming an absent file in a fresh directory, by an absolute path, as p4 looks for a relative one in
# every directory it climbs, so that no P4CONFIG file gives p4 set settings. Where that path can't be made, p4 doesn't run.
function RunP4WithoutConfigFiles()
{
	local Directory
	if ! Directory="$(mktemp -d)" || [[ -z "$Directory" ]]; then
		return 1
	fi
	if [[ "$Directory" != /* && ! "$Directory" =~ ^[A-Za-z]:[/\\] ]]; then
		Directory="$PWD/$Directory"
	fi

	local ConfigFile="$Directory/none"
	if command -v cygpath > /dev/null 2>&1; then
		if ! ConfigFile="$(cygpath -w "$ConfigFile")" || [[ -z "$ConfigFile" ]]; then
			rmdir "$Directory"
			return 1
		fi
	fi

	local Result=0
	P4CONFIG="$ConfigFile" p4 "$@" 2> /dev/null || Result=$?
	rmdir "$Directory"
	return $Result
}

# A p4 setting as mib takes it in the project directory, where an empty one is unset: from its .p4config file, as p4 parses one,
# where a CR ending a line and then the spaces ending it, but not tabs, are no part of it, the first line setting the variable wins
# and the names' case is folded on Windows and macOS; else from the environment; else from what p4 set reports from the enviro
# file and, on Windows, the registry, in lines that end with CR on Windows. Without p4 in PATH, what p4 set stored is not
# considered.
function GetP4Setting()
{
	local Name="$1"
	local CaseOption=""
	if [[ "$OSTYPE" == darwin* || "$OSTYPE" == msys* || "$OSTYPE" == cygwin* ]]; then
		CaseOption="-i"
	fi

	local Line=""
	if [[ -n "$P4ConfigFile" ]]; then
		Line="$(LC_ALL=C sed -e $'s/\r$//' -e 's/ *$//' "$P4ConfigFile" 2> /dev/null | LC_ALL=C grep -a $CaseOption -m 1 "^$Name=" || true)"
	fi

	# Windows' environment ignores the case of names, which bash keeps as Windows hands them over.
	local Value="${!Name:-}"
	if [[ -z "$Value" && ( "$OSTYPE" == msys* || "$OSTYPE" == cygwin* ) ]]; then
		local Variable
		while IFS= read -r Variable; do
			if [[ -n "${!Variable:-}" ]]; then
				Value="${!Variable}"
				break
			fi
		done < <(env | LC_ALL=C sed -n 's/=.*//p' | LC_ALL=C grep -a -ix "$Name")
	fi

	if [[ -n "$Line" ]]; then
		ExpandP4Home "${Line#*=}"
	elif [[ -n "$Value" ]]; then
		ExpandP4Home "$Value"
	elif command -v p4 > /dev/null 2>&1; then
		# Without the environment's empty value, which p4 set would report ahead of what it stored, in whatever case Windows hands
		# the variable's name to bash. p4 set reports only the setting p4 takes, with its name spelled as it is stored.
		ExpandP4Home "$(
			while IFS= read -r Variable; do
				if [[ -z "${!Variable:-}" ]]; then
					unset "$Variable"
				fi
			done < <(env | LC_ALL=C sed -n 's/=.*//p' | LC_ALL=C grep -a -ix "$Name")
			RunP4WithoutConfigFiles set -q "$Name" | LC_ALL=C tr -d '\r' | LC_ALL=C grep -a -i -m 1 "^$Name=" | LC_ALL=C sed 's/^[^=]*=//' || true
		)"
	fi
}

if [[ ! -e "$ProjectDir/.git" ]] && ! command -v p4 > /dev/null 2>&1; then
	InitOutputNote "p4 is not in PATH, so the Perforce settings stored with p4 set are not considered, only the environment and the .p4config file"
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

IsPerforceRoot=false
if [[ ! -e "$ProjectDir/.git" ]] && [[ -n "$P4ConfigFile" ]]; then
	IsPerforceRoot=true
	InitOutputNote "mib reads the project's .p4config whatever P4CONFIG is. p4 itself reads that file only with P4CONFIG=.p4config set"
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
