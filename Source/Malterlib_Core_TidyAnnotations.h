// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

// Annotations that the malterlib-* clang-tidy checks read and verify. They expand to nothing for other compilers.
//
// The phases of the life of an actor. Once an actor is in a phase it stays in it, because the flags that tell are only ever set:
//   Destroy: the actor is destroyed. fp_DestroyInternal sets the flag f_IsDestroyed() returns, then runs fp_Destroy.
//   StopApp: the app stops. f_StopApp sets m_bStoppingApp, then runs fp_StopApp.
//
// DMibActorEntries(Phases...): the entries of a collection are only removed in the phases, and with Owner by the call that added
//   them with a fresh key. A coroutine may then keep a reference to an entry across a suspension, with a guard of the phases alive.
// DMibActorPhase(Phases...): a function that only runs in one of the phases. Overrides inherit it.
// DMibActorPhaseEntry(Phase): sets the flag of the phase, then runs it.
// DMibActorPhaseFlag(Phase): the flag of the phase, or what returns it.
// DMibActorPhaseGuard(Phases...): ends the coroutine on resume once the actor is in one of the phases, while its result lives.
//
// DMibRunsToCompletion: a function, or the member functions of a class that isn't an actor, that only ends by finishing, never by
//   being aborted, because it only suspends on concurrent actors. Its callers join it, so it may refer to their data.
// DMibRunsToCompletionEntry: where running to completion begins, by switching to a concurrent actor before it first suspends.
//
// DMibResultEndsBeforeObject: a member function whose result must not outlive its object, like a guard whose lambda uses the
//   object when the guard is destroyed. Its lambda may use the object until it first suspends, and each caller keeps the result
//   in a local declared after the object, itself a local, in the same block, which destroys the result first.
// DMibNamedAsMemberFunction: a static member function named as if it weren't static, as it shares its name with the non-static
//   functions of the same interface, like f_Alloc and f_Free of the allocators. A function of the same name in a derived class
//   takes it from the base.
//
// DMibRunsGivenFunction: a function that calls its first callable argument with its remaining arguments, and keeps both until the
//   result of that call is done. The parameters of a closure given to it are initialized from those arguments.
// DMibKeepsArgument: a parameter whose pointer, reference or view the function keeps after it returns. The object a constructor
//   makes keeps it, and uses it where the object is used or destroyed.
// DMibSetByOwnClass: a pointer or reference member that only points where the constructors and member functions of its class point
//   it. Without it, what such a member points to can't be told.
// DMibKeepsThis: an owner parameter of a member function that owns the object the function runs on. Without it, an owner parameter
//   doesn't keep the object alive.

#if defined(__clang__)
#	define DMibActorEntries(...) [[clang::annotate("malterlib-actor-entries", #__VA_ARGS__)]]
#	define DMibActorPhase(...) [[clang::annotate("malterlib-actor-phase", #__VA_ARGS__)]]
#	define DMibActorPhaseEntry(...) [[clang::annotate("malterlib-actor-phase-entry", #__VA_ARGS__)]]
#	define DMibActorPhaseFlag(...) [[clang::annotate("malterlib-actor-phase-flag", #__VA_ARGS__)]]
#	define DMibActorPhaseGuard(...) [[clang::annotate("malterlib-actor-phase-guard", #__VA_ARGS__)]]
#	define DMibRunsToCompletion [[clang::annotate("malterlib-runs-to-completion")]]
#	define DMibRunsToCompletionEntry [[clang::annotate("malterlib-runs-to-completion-entry")]]
#	define DMibResultEndsBeforeObject [[clang::annotate("malterlib-result-ends-before-object")]]
#	define DMibNamedAsMemberFunction [[clang::annotate("malterlib-named-as-member-function")]]
#	define DMibRunsGivenFunction [[clang::annotate("malterlib-runs-given-function")]]
#	define DMibKeepsArgument [[clang::annotate("malterlib-keeps-argument")]]
#	define DMibSetByOwnClass [[clang::annotate("malterlib-set-by-own-class")]]
#	define DMibKeepsThis [[clang::annotate("malterlib-keeps-this")]]
#else
#	define DMibActorEntries(...)
#	define DMibActorPhase(...)
#	define DMibActorPhaseEntry(...)
#	define DMibActorPhaseFlag(...)
#	define DMibActorPhaseGuard(...)
#	define DMibRunsToCompletion
#	define DMibRunsToCompletionEntry
#	define DMibResultEndsBeforeObject
#	define DMibNamedAsMemberFunction
#	define DMibRunsGivenFunction
#	define DMibKeepsArgument
#	define DMibSetByOwnClass
#	define DMibKeepsThis
#endif
