// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NSys::NPrivate
{
	enum
	{
		EThreadNotificationCrossModule_Version_Min = 0x100
		, EThreadNotificationCrossModule_Version = 0x100
	};

	typedef void FThreadCreatedNotification(umint _ThreadID, umint _ParentThreadID);
	typedef void FThreadTerminatedNotification(umint _ThreadID);
	typedef void FThreadForkNotification();
	typedef void FThreadEnumCallback(umint _ThreadID, void *_pContext);

	struct CThreadNotificationModule
	{
		uint32 m_Version;
		umint m_Reserved[8];
		FThreadCreatedNotification *m_fCreated;
		FThreadTerminatedNotification *m_fTerminated;
		FThreadForkNotification *m_fForkPrepare;
		FThreadForkNotification *m_fForkParent;
		FThreadForkNotification *m_fForkChild;
	};

	struct CThreadNotificationCrossModule
	{
		uint32 m_Version;
		umint m_Reserved[8];
		void (DMibCrossmoduleAPI *m_fRegister)(CThreadNotificationModule *_pModule);
		void (DMibCrossmoduleAPI *m_fUnregister)(CThreadNotificationModule *_pModule);
		void (DMibCrossmoduleAPI *m_fEnum)(FThreadEnumCallback *_fThread, void *_pContext);
	};

	typedef CThreadNotificationCrossModule *FGetThreadNotificationCrossModule(uint32 _Version);
}
