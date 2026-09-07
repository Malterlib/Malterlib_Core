#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Source this script and call AcquireBuildLock <build directory>; the exit trap releases the lock.
# A token changes ownership by atomic rename. Publish identity before marking it held, and match process
# start time as well as PID to reject reuse. Takeover renames only the inspected token so it cannot steal a newer owner's lock.

BuildLockToken=

# The start time and command name of a process, as one line; empty when ps cannot tell
BuildLockProcessIdentity()
{
	local Pid="$1"
	local Identity

	Identity=$(ps -p "$Pid" -o lstart=,comm= 2>/dev/null)

	# The MSYS ps of Git Bash on Windows takes no -o: PID PPID PGID WINPID TTY UID STIME COMMAND
	if [ -z "$Identity" ]; then
		Identity=$(ps -p "$Pid" 2>/dev/null | awk -v Pid="$Pid" '$1 == Pid { print $7, $8 }')
	fi

	echo $Identity
}

ReleaseBuildLock()
{
	if [ -n "$BuildLockToken" ]; then
		# A terminated wrapper can leave Ninja running; keep its token until the recorded child exits.
		if BuildLockChildRunning "$BuildLockToken"; then
			BuildLockToken=
			return 0
		fi

		# A takeover may already have renamed the token; release only the token still owned here.
		mv "$BuildLockToken" "${BuildLockToken%/*}/free" 2>/dev/null
		BuildLockToken=
	fi
}

BuildLockPublish()
{
	local Dir="$1"

	echo $$ > "$Dir/claim.$$/pid"
	BuildLockProcessIdentity $$ > "$Dir/claim.$$/identity"

	# A held token named for this pid can only have been left by a dead process that had the
	# pid before; mv would otherwise move the claim inside it
	rm -rf "$Dir/held.$$"
	mv "$Dir/claim.$$" "$Dir/held.$$" 2>/dev/null || return 1

	return 0
}

# The exit trap checks for a surviving Ninja child before releasing; an untrapped signal can end the wrapper first.
BuildLockTaken()
{
	BuildLockToken="$1/held.$$"
	trap ReleaseBuildLock EXIT
}

# Use start time because it survives exec from shell to Ninja. MSYS ps requires its fixed-column format.
BuildLockProcessStart()
{
	local Pid="$1"
	local Start
	Start=$(ps -p "$Pid" -o lstart= 2>/dev/null)
	if [ -z "$Start" ]; then
		Start=$(ps -p "$Pid" 2>/dev/null | awk -v Pid="$Pid" '$1 == Pid { print $7 }')
	fi
	echo $Start
}

# Record the child before exec so the lock remains held if only the wrapper dies.
BuildLockRecordSelfAsChild()
{
	local ChildPid
	[ -n "$BuildLockToken" ] || return 0

	# $$ is not the subshell PID and macOS Bash 3.2 lacks BASHPID. A direct child writes its parent PID without another substitution fork.
	sh -c 'echo $PPID' > "$BuildLockToken/child" 2>/dev/null
	ChildPid=$(cat "$BuildLockToken/child" 2>/dev/null)
	BuildLockProcessStart "$ChildPid" > "$BuildLockToken/child_start" 2>/dev/null

	# Takeover rechecks the child after rename; recording rechecks the token after write. One must observe the other before Ninja starts.
	if [ -z "$ChildPid" ] || [ ! -d "$BuildLockToken" ]; then
		echo "The build lock '${BuildLockToken%/*}' was taken over by another build before ninja started, so ninja is not run." >&2
		return 1
	fi
}

# Match both PID and start time so PID reuse cannot keep a dead child's lock alive.
BuildLockChildRunning()
{
	local Token="$1"
	local ChildPid ChildStart CurrentStart
	ChildPid=$(cat "$Token/child" 2>/dev/null)
	[ -n "$ChildPid" ] || return 1
	kill -0 "$ChildPid" 2>/dev/null || return 1
	ChildStart=$(cat "$Token/child_start" 2>/dev/null)
	CurrentStart=$(BuildLockProcessStart "$ChildPid")
	[ -n "$ChildStart" ] && [ "$ChildStart" = "$CurrentStart" ]
}

AcquireBuildLock()
{
	local BuildDir="$1"
	local LockDir="$BuildDir/.mib_build_lock"
	local Init="$LockDir.init.$$"
	local Attempt Token OwnerPid OwnerIdentity CurrentIdentity FoundToken

	# A claim named for this pid was left by a dead process that had the pid before
	rm -rf "$LockDir/claim.$$"

	for Attempt in 1 2 3 4 5 6; do
		if [ ! -d "$LockDir" ]; then
			# Publish a nonempty lock directory by rename. Detect mv nesting inside an existing target as a lost race.
			rm -rf "$Init"
			mkdir "$Init" 2>/dev/null || continue
			if mkdir "$Init/claim.$$" 2>/dev/null && BuildLockPublish "$Init"; then
				mv "$Init" "$LockDir" 2>/dev/null
				if [ -d "$LockDir/held.$$" ] && [ ! -e "$LockDir/${Init##*/}" ]; then
					BuildLockTaken "$LockDir"
					return 0
				fi
			fi

			rm -rf "$Init" "$LockDir/${Init##*/}"
			continue
		fi

		if mv "$LockDir/free" "$LockDir/claim.$$" 2>/dev/null; then
			if BuildLockPublish "$LockDir"; then
				BuildLockTaken "$LockDir"
				return 0
			fi
			continue
		fi

		FoundToken=
		for Token in "$LockDir"/held.* "$LockDir"/claim.*; do
			[ -e "$Token" ] || continue
			FoundToken=1
			OwnerPid="${Token##*.}"

			if kill -0 "$OwnerPid" 2>/dev/null; then
				# Allow a live claimant time to publish identity; a mismatch means PID reuse.
				OwnerIdentity=$(cat "$Token/identity" 2>/dev/null)
				if [ -z "$OwnerIdentity" ]; then
					sleep 1
					OwnerIdentity=$(cat "$Token/identity" 2>/dev/null)
				fi
				CurrentIdentity=$(BuildLockProcessIdentity "$OwnerPid")
				if [ -z "$OwnerIdentity" ] || [ -z "$CurrentIdentity" ] || [ "$OwnerIdentity" = "$CurrentIdentity" ]; then
					echo "Another build is already running in '$BuildDir' (process $OwnerPid holds the lock '$LockDir')." >&2
					echo "Two builds of one configuration overlap on the same build and dependency logs and corrupt them, so this one is refused." >&2
					echo "To build several targets, pass them comma separated to one build-target command." >&2
					return 1
				fi
			fi

			# The owner may be gone while the ninja it started still runs, when the wrapper was
			# killed on its own; the ninja recorded in the token holds the lock while it lives
			if BuildLockChildRunning "$Token"; then
				echo "The ninja of another build (process $(cat "$Token/child")) is still running in '$BuildDir' though the build that started it is gone, so this one is refused." >&2
				return 1
			fi

			# Rename only the inspected dead token, then recheck for a child recorded during takeover.
			# The child rechecks its token after recording, so both sides cannot miss the race.
			if mv "$Token" "$LockDir/claim.$$" 2>/dev/null; then
				if BuildLockChildRunning "$LockDir/claim.$$"; then
					echo "The ninja of another build (process $(cat "$LockDir/claim.$$/child")) is still running in '$BuildDir' though the build that started it is gone, so this one is refused." >&2
					mv "$LockDir/claim.$$" "$Token" 2>/dev/null
					return 1
				fi

				rm -f "$LockDir/claim.$$/child" "$LockDir/claim.$$/child_start"
				if BuildLockPublish "$LockDir"; then
					BuildLockTaken "$LockDir"
					return 0
				fi
			fi

			break
		done

		if [ -z "$FoundToken" ]; then
			# Remove only an empty abandoned directory; rmdir refuses if a claimant added a token meanwhile.
			sleep 1
			rmdir "$LockDir" 2>/dev/null
		fi
	done

	echo "Could not take the build lock '$LockDir'" >&2
	return 1
}
