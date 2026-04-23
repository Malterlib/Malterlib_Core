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

AgentsGenerator="$ProjectDir/Malterlib/Core/Tools/generate_agents.py"

echo
if [ -x "$AgentsGenerator" ]; then
	InitOutputInfo "Generating AGENTS.md"
	(cd "$ProjectDir" && "$AgentsGenerator")
else
	InitOutputError "Skipping AGENTS.md generation: $AgentsGenerator not found or not executable"
fi

echo
InitOutputHeading "Successfully initialized Malterlib project"
echo
echo "Next steps:"
echo "  1. Edit $BuildSystemFile to set ProductCompany and ProductCompanyUniqueIdentifier"
echo "  2. Run ./mib generate to generate the default Tests workspace"
echo "  3. Prompt your favourite agent, for example:"
echo
echo "     Create a distributed app tool that reports the weather."
echo "     * Use the table renderer."
echo "     * Detect the location from the exit IP."
echo "     * Create a new workspace, because this repository was just created."
echo
