// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

inline umint CIoSubSystem_Linux::f_SendDepth() const
{
#if DMibConfig_IoDebug_Enable
	return m_nSendDepth;
#else
	return gc_UringDefaultSendDepth;
#endif
}

inline umint CIoSubSystem_Linux::f_ReceiveBuffersOverride() const
{
#if DMibConfig_IoDebug_Enable
	return m_nReceiveBuffersOverride;
#else
	return 0;
#endif
}

inline umint CIoSubSystem_Linux::f_ReceiveBufferBytesOverride() const
{
#if DMibConfig_IoDebug_Enable
	return m_nReceiveBufferBytesOverride;
#else
	return 0;
#endif
}

inline EUringZeroCopyOverride CIoSubSystem_Linux::f_ZeroCopyOverride() const
{
#if DMibConfig_IoDebug_Enable
	return m_ZeroCopyOverride;
#else
	return EUringZeroCopyOverride::mc_None;
#endif
}

inline bool CIoSubSystem_Linux::f_TraceEnabled() const
{
#if DMibConfig_IoDebug_Enable
	return m_bTraceEnabled;
#else
	return false;
#endif
}
