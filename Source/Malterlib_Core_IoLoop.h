// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#ifndef DPlatformFamily_Windows
#	include <errno.h>
#endif

namespace NMib
{
	struct CVirtualDestroyBase;
}

namespace NMib::NStorage
{
	template <typename t_CType, typename... tp_COptions>
	class TCSharedPointer;
}

namespace NMib::NThread
{
	class CEventAutoReset;
}

namespace NMib::NFunction
{
	template <typename t_CSignature>
	struct TCFunctionMovable;
}

namespace NMib::NSys
{
	// A POSIX descriptor or Windows SOCKET/HANDLE; all-ones is invalid.
#if defined(DPlatformFamily_Windows)
	using CIoLoopHandle = umint;
	constexpr CIoLoopHandle gc_IoLoopHandleInvalid = ~CIoLoopHandle(0);
#else
	using CIoLoopHandle = int;
	constexpr CIoLoopHandle gc_IoLoopHandleInvalid = -1;
#endif

	// Error codes use Winsock numbering on Windows and errno elsewhere.
#ifdef DPlatformFamily_Windows
	constexpr int gc_IoErrorConnectionReset = 10054; // WSAECONNRESET
#else
	constexpr int gc_IoErrorConnectionReset = ECONNRESET;
#endif

	// All operations except wake run on the attached owner thread. Detach and wait out concurrent wakers before destruction.
	struct ICThreadIoLoop
	{
		virtual ~ICThreadIoLoop();

		virtual void f_WaitAndDispatch() = 0;
		virtual bool f_PollAndDispatch() = 0;
		virtual void f_Wake() = 0;
		virtual void f_DrainForShutdown();

		virtual void f_SetParkEvent(NThread::CEventAutoReset *_pEvent);
		virtual bool f_ParksOnQueueEvent() const;
	};


	// ReadClosed permits draining buffered input. Hup implies no error by itself; WriteClosed requires querying the socket error.
	// For Error, a zero callback error means the consumer must query the descriptor.
	enum class EIoLoopEvent : uint32
	{
		mc_None			= 0,
		mc_Read			= 1 << 0,
		mc_Write		= 1 << 1,
		mc_ReadClosed	= 1 << 2,
		mc_Hup			= 1 << 3,
		mc_WriteClosed	= 1 << 4,
		mc_Error		= 1 << 5,
	};

	// Called during dispatch on the driving thread. mc_None acknowledges registration when requested; the token is opaque to the loop.
	using FIoLoopReadinessCallback = void (*)(void *_pToken, EIoLoopEvent _Events, int _Error);

	enum class EIoCompletionStatus : uint8
	{
		mc_Done,
		mc_Cancelled, // Never submitted or cancelled; the kernel has released the buffers and no further completion follows.
		mc_Error,
	};

	// Result of a completion transfer. A receive that completes with Done and zero bytes is end
	// of stream, matching what a zero read means on a readable descriptor
	struct CIoCompletion
	{
		static constexpr umint mc_iTransferNone = ~umint(0);

		umint m_nBytes = 0;
		umint m_iTransfer = mc_iTransferNone; // Submitter-owned transfer identity, including operations resolved before reaching the loop.
		int32 m_Error = 0; // Platform error code when m_Status is Error
		EIoCompletionStatus m_Status = EIoCompletionStatus::mc_Done;
	};

	// Invoked once per submitted operation on the loop thread. Receive buffers are reusable then;
	// send buffers remain owned by the kernel until FIoBufferReleased runs.
	using FIoCompletion = NFunction::TCFunctionMovable<void (CIoCompletion _Result)>;

	// Invoked once per accepted send, on the loop thread at or after completion, when buffers become reusable.
	// Zero-copy sends may retain several completed generations until peer acknowledgement.
	using FIoBufferReleased = NFunction::TCFunctionMovable<void ()>;

	struct CIoStreamSegment;

	struct CIoStreamBackpressure;

	// Invoked on the thread that drives the loop, once per segment
	using FIoStreamSink = NFunction::TCFunctionMovable<void (CIoStreamSegment &&_Segment)>;

	struct CIoSpan;

	constexpr umint gc_IoLoopMaxSubmitSpans = 64;

	// Loop-owned; valid until blocking deregistration returns or its asynchronous continuation runs.
	struct CIoLoopRegistration;

	// Readiness-only registrations must not acquire permanent completion bindings, leaving the handle available to later owners.
	struct CIoLoopRegisterOptions
	{
		bool m_bReadinessOnly = false;
		bool m_bInheritedHandle = false; // Inherited completion notification modes cannot be changed by the new owner.
		bool m_bSendCompletesOnAck = false; // No kernel send buffer: acceptance reports completion, peer acknowledgement releases buffers.
	};

	// Registration and submission may run on any thread. Callbacks and deregistration continuations run synchronously on the driving thread.
	// One owner sequences all submissions and receive-stream operations before requesting deregistration.
	struct ICIoLoop : public ICThreadIoLoop
	{
		~ICIoLoop() override;

		virtual void f_SetOwnerThreadToCurrent() = 0;

		virtual auto f_Register
			(
				CIoLoopHandle _Handle
				, void *_pToken
				, EIoLoopEvent _EventMask
				, FIoLoopReadinessCallback _fOnEvents
				, bool _bNotifyRegistered
				, CIoLoopRegisterOptions const &_Options = CIoLoopRegisterOptions()
			)
			-> CIoLoopRegistration * = 0
		;
		virtual void f_RequestReadiness(CIoLoopRegistration *_pRegistration, EIoLoopEvent _EventMask);
		virtual void f_Deregister(CIoLoopRegistration *_pRegistration) = 0;
		virtual void f_DeregisterAsync(CIoLoopRegistration *_pRegistration, NFunction::TCFunctionMovable<void ()> &&_fOnDeregistered) = 0;

		virtual bool f_SupportsCompletionIo() const;
		virtual umint f_GetCompletionSendDepth() const;
		virtual bool f_SendReleaseIsPrompt(CIoLoopRegistration const *_pRegistration) const;
		virtual umint f_SubmitSendVectored(CIoLoopRegistration *_pRegistration, CIoSpan const *_pSpans, umint _nSpans, FIoCompletion &&_fOnComplete, FIoBufferReleased &&_fOnBufferReleased);

		virtual bool f_SupportsReceiveStream() const;
		virtual bool f_StartReceiveStream(CIoLoopRegistration *_pRegistration, umint _nBufferBytes, NStorage::TCSharedPointer<CIoStreamBackpressure> _pBackpressure, FIoStreamSink &&_fSink);
		virtual void f_ResumeReceiveStream(CIoLoopRegistration *_pRegistration);

		virtual void f_SetSendWindow(CIoLoopRegistration *_pRegistration, umint _nBytes);
		virtual bool f_IsSendWindowFull(CIoLoopRegistration *_pRegistration, umint _nUnreleasedBytes, umint _nStartBytes);

		virtual bool f_AdoptHandle(CIoLoopHandle _Handle, int &o_Error);

		bool m_bCreatedAsLoop = false; // True for caller-owned worker loops; false for the shared platform loop.
	};

	ICIoLoop *fg_CreateIoLoop();
	void fg_DestroyIoLoop(ICIoLoop *_pLoop);

	void fg_SetThreadIoLoop(ICIoLoop *_pLoop);
	ICIoLoop *fg_GetThreadIoLoop();
}
