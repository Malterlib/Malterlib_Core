// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	/// Specifies options for a coroutine. In debug mode contains properties the coroutine possess.
	enum ECoroutineFlag
	{
		ECoroutineFlag_None = 0
		, ECoroutineFlag_CaptureExceptions = DMibBit(0)
		, ECoroutineFlag_AllowReferences = DMibBit(1) 	///< Warning, when you allow references for parameters in your coroutine, make sure that you don't use the reference
														///< after the first suspension point, as it will be out of scope in this case.
		, ECoroutineFlag_BreakSelfReference = DMibBit(2)///< When co_awaiting a future don't hold a reference to the actor
#if DMibEnableSafeCheck > 0
		, ECoroutineFlag_UnsafeReferenceParameters = DMibBit(3) // Automatically generated
		, ECoroutineFlag_UnsafeThisPointer = DMibBit(4)			// Automatically generated
#endif
		, ECoroutineFlag_AllowReferencesCaptureExceptions = ECoroutineFlag_CaptureExceptions | ECoroutineFlag_AllowReferences
	};
}
