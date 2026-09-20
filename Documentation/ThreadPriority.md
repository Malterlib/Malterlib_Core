# Thread priority

`EExecutionPriority` is absolute: a value gives the same scheduling in every process.
`EExecutionPriority_Default` leaves a thread as it is. Setting another thread's priority is best
effort, so use the `Try` variants unless failure is fatal.

| Priority | Linux | macOS | Windows |
| --- | --- | --- | --- |
| `Lowest` | `SCHED_IDLE` | `QOS_CLASS_BACKGROUND`, minimum relative priority | `THREAD_PRIORITY_IDLE` |
| `Low` | nice 10 | `QOS_CLASS_BACKGROUND` | `THREAD_PRIORITY_LOWEST` |
| `BelowNormal` | nice 5 | `QOS_CLASS_UTILITY` | `THREAD_PRIORITY_BELOW_NORMAL` |
| `Normal` | nice 0 | `QOS_CLASS_DEFAULT` | `THREAD_PRIORITY_NORMAL` |
| `AboveNormal` | nice -5 | `QOS_CLASS_USER_INITIATED` | `THREAD_PRIORITY_ABOVE_NORMAL` |
| `High` | nice -10 | `QOS_CLASS_USER_INTERACTIVE`, minimum relative priority | `THREAD_PRIORITY_HIGHEST` |
| `Highest` | `SCHED_RR`, priority 1 to 20 | `QOS_CLASS_USER_INTERACTIVE` | `THREAD_PRIORITY_TIME_CRITICAL` |

Windows and macOS let a thread change its own priority freely, so everything below is Linux only.
The code is in `Source/Platform/Malterlib_Core_PlatformImp_Linux_ThreadPriority.hpp`.

## Linux scale

The fair band is linear between the named values. Nice is a system wide quantity, which is why
the scale is not relative to the process. `SCHED_RR` rather than `SCHED_FIFO` because the threads
of a pool share one priority and have to rotate. A thread entering `SCHED_IDLE` is first parked at
the ceiling nice, as leaving the policy is checked against the nice the thread holds.

## Process ceiling

The ceiling is the best nice the threads of a process can hold: the better of the nice the process
started with and what `RLIMIT_NICE` grants, or anything with `CAP_SYS_NICE`. Requests above the
ceiling clamp to it instead of issuing a system call the kernel refuses. `Highest` falls back to
the ceiling unless `RLIMIT_RTPRIO` grants real-time. The soft limits are raised to the hard limits
at startup.

## Daemons

A daemon's execution priority is its ceiling. `fg_Process_GetLinuxPriorityGrant` gives what the
service definition starts it with: `RLIMIT_NICE` as `20 - nice`, `RLIMIT_RTPRIO` for `Highest`, and
a starting nice when the priority is below normal, because a limit never lowers a process. A
priority above normal needs no starting nice; the threads raise themselves through the limit. No
scheduling policy is set on the process, as it would also apply to foreign threads that never
correct themselves.

## Thread creation

A thread inherits the scheduling of the thread that creates it and can only raise itself as far as
`RLIMIT_NICE` grants, a limit that is the same whichever thread asks. When the grant reaches the
ceiling every thread corrects itself at startup and nothing else happens.

Otherwise `fg_Thread_Create` hands a creation that asks for more than the calling thread holds to a
thread that holds it. The decision reads the real scheduling of both threads, so it stays right
when a foreign library changed a priority. The created thread gets the requester as its parent
and the requester's signal mask and affinity.

- **Spawn server.** `fg_Thread_RegisterSpawnServer` makes the calling thread serve creations. It
  may only block where its wake function reaches it, as a request waits until it is served. The run
  loop of `fg_RunApp` registers itself through `CRunLoopThreadSpawnServer`.
- **Spawn helper.** Without a server the `Thread spawner` thread serves. It is created just before
  the first thread is lowered, by the thread doing the lowering, or when a foreign thread is
  created, so that it inherits the best priority the process has. A process that lowers nothing
  never gets one.
- **`fg_Thread_CanRestorePriority`** tells whether a lowered thread can be raised again, which
  without the grant it cannot. A thread that has to be raised later must not be lowered then: the
  memory manager cleanup thread, which is raised for shutdown, only runs below normal when it can.
- A forked child and dynamic library builds create threads directly.

Two cases stay lowered: a process that was started lowered, and a thread created from a foreign
thread that lowered itself before the helper existed. A thread that ends up below its requested
priority is traced. Clamping to the ceiling is the normal case and is not traced, as debug output
lands in the output of the process.

Nothing may allocate under the spawn lock: an allocation can start the memory manager's cleanup
thread and re-enter thread creation.
