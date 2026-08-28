// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

inline bool CIoSubSystem_Windows::f_CompletionEnabled() const
{
#if DMibConfig_IoDebug_Enable
	return m_bCompletionEnabled;
#else
	return true;
#endif
}

inline bool CIoSubSystem_Windows::f_SkipSuccessEnabled() const
{
#if DMibConfig_IoDebug_Enable
	return m_bSkipSuccessEnabled;
#else
	return false;
#endif
}

inline bool CIoSubSystem_Windows::f_LoopbackFastPathEnabled() const
{
#if DMibConfig_IoDebug_Enable
	return m_bLoopbackFastPathEnabled;
#else
	return true;
#endif
}

inline bool CIoSubSystem_Windows::f_DirectSendEnabled() const
{
#if DMibConfig_IoDebug_Enable
	return m_bDirectSendEnabled;
#else
	return true;
#endif
}

inline umint CIoSubSystem_Windows::f_SendDepth() const
{
#if DMibConfig_IoDebug_Enable
	return m_nSendDepth;
#else
	return gc_IocpDefaultSendDepth;
#endif
}

inline umint CIoSubSystem_Windows::f_RecvDepth() const
{
#if DMibConfig_IoDebug_Enable
	return m_nRecvDepth;
#else
	return gc_IocpDefaultRecvDepth;
#endif
}

inline umint CIoSubSystem_Windows::f_RecvBufferBytesOverride() const
{
#if DMibConfig_IoDebug_Enable
	return m_nRecvBufferBytesOverride;
#else
	return 0;
#endif
}

#if DMibConfig_IoDebug_Enable
inline bool CIoSubSystem_Windows::f_TraceEnabled() const
{
	return m_TraceMode != 0;
}
#endif
