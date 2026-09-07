#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# One build per build directory at a time. Ninja does not protect its build and dependency logs
# from a second process: two builds of one configuration corrupt them, and the recovery on the
# next load rebuilds everything.
#
# The lock is a directory holding one token, a subdirectory that changes hands by rename, since
# rename is atomic on every platform and needs no flock: of two builds renaming the same token,
# exactly one succeeds. The token is 'free' while no build holds the lock and 'held.<pid>' while
# the build with that pid does; it records the identity of that process (its start time and
# command from ps), so a pid the system has since given to some other process does not pass for
# the owner. A token whose owner is no longer running, or whose pid now belongs to another
# process, was left by a build that died and is taken over; a live owner makes this build fail.
#
# The take-over renames the very token that was inspected, named for its dead owner, so it can
# never remove a token a live build took in the meantime, whose name is that build's pid. A
# taken token is first renamed to 'claim.<pid>', filled in with the new owner's identity and
# only then published as 'held.<pid>', so a held token never shows an identity that does not
# match its owner. The lock directory itself is created once and stays; it is built under a
# temporary name with its creator's token already inside and renamed into place, so it is never
# seen empty, since an empty lock directory is one whose creator died and is removed.
#
# Source this script, then call AcquireBuildLock <build directory>. The lock is released when the
# calling script exits.

BuildLockToken=

# The start time and command name of a process, as one line; empty when ps cannot tell
BuildLockProcessIdentity()
{
	local Pid="$1"
	local Identity

	# ps with column selection (Linux, macOS)
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
		# A ninja still running keeps the token: an untrapped SIGTERM ends this shell at once,
		# this trap included, while the foreground ninja goes on, and a token handed back then
		# would let the next build start a second ninja beside it. The held token names the
		# ninja, and the next build waits for it or takes the token over once it is gone
		if BuildLockChildRunning "$BuildLockToken"; then
			BuildLockToken=
			return 0
		fi

		# Hands the token back. A take-over that judged this build dead has renamed the token
		# away already; it is then not this build's to move
		mv "$BuildLockToken" "${BuildLockToken%/*}/free" 2>/dev/null
		BuildLockToken=
	fi
}

# Fills in the token this build has claimed in the given directory and publishes it there as
# the held token
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

# The lock is this build's: the published token in the lock directory. The exit trap is what
# frees it, on a normal end and on a terminating signal alike; bash runs it at once for an
# untrapped signal, foreground ninja or not, which is why the release checks for the child
# first. A wrapper killed outright runs no trap at all, and the token it leaves names the
# ninja for the next build to wait on
BuildLockTaken()
{
	BuildLockToken="$1/held.$$"
	trap ReleaseBuildLock EXIT
}

# The start time of a process, the part of its identity that survives an exec: the child that
# records itself is a shell at the time and ninja afterwards. The MSYS ps on Windows knows no
# -o and lists the columns PID PPID PGID WINPID TTY UID STIME COMMAND
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

# Run by the subshell that becomes ninja, before it does: the pid it records is ninja's from
# the exec on, so a wrapper killed on its own leaves a ninja the lock knows about, and the lock
# stays with it for as long as it runs
BuildLockRecordSelfAsChild()
{
	local ChildPid
	[ -n "$BuildLockToken" ] || return 0

	# The subshell's own pid, which $$ does not give and $BASHPID only from bash 4 (the system
	# bash of macOS is 3.2): a child's parent is this subshell, and it writes the pid straight
	# to the file, since a command substitution would fork once more in between
	sh -c 'echo $PPID' > "$BuildLockToken/child" 2>/dev/null
	ChildPid=$(cat "$BuildLockToken/child" 2>/dev/null)
	BuildLockProcessStart "$ChildPid" > "$BuildLockToken/child_start" 2>/dev/null

	# The record races a take-over by a build that found the wrapper dead and no child recorded
	# yet: it renames the token away and looks for the child again afterwards, and this looks
	# for the token after the record, so one of the two sees the other. A token gone by now is
	# the other build's, and ninja must not start under it
	if [ -z "$ChildPid" ] || [ ! -d "$BuildLockToken" ]; then
		echo "The build lock '${BuildLockToken%/*}' was taken over by another build before ninja started, so ninja is not run." >&2
		return 1
	fi
}

# Whether the ninja a token names is still running: alive, and started when the record says,
# so that a process that reused the pid does not pass for it
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
			# The lock directory is built apart, with this build's token inside, and renamed
			# into place: exactly one of the builds doing so at once lands as the lock
			# directory, since mv moves a directory inside one that exists at the target
			# instead, which the check below tells apart
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
				# Alive, but is it the owner? The owner writes its identity into a claim right
				# after taking it; give a build that is between the two a moment. A recorded
				# identity that no longer matches means the pid was reused after the owner died
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

			# Left behind by a build that is no longer running: taking it over is renaming this
			# token, so a competing take-over that got there first leaves the rename failing. A
			# ninja that recorded itself since the check above shows in the renamed token, which
			# then goes back to it; the child looks for its token after its record, so the two
			# cannot miss each other. The dead build's child record does not carry over
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
			# No token: the directory was left empty, by a dead build whose claim named for a
			# reused pid was removed above. It is removed and built again; rmdir refuses a
			# directory that got a token in the meantime
			sleep 1
			rmdir "$LockDir" 2>/dev/null
		fi
	done

	echo "Could not take the build lock '$LockDir'" >&2
	return 1
}
