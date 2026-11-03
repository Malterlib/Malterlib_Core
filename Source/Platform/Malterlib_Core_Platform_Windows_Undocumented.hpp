// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

inline_always CUndocumentedTEB *fg_GetTEB()
{
#if defined(DArchitecture_arm64)
	return (CUndocumentedTEB *)__readx18qword(6 * sizeof(void *));
#elif defined(DArchitecture_x64)
	return (CUndocumentedTEB *)__readgsqword(6 * sizeof(void *));
#else
	return (CUndocumentedTEB *)__readfsdword(6 * sizeof(void *));
#endif
}

template <typename tf_CType>
inline_always tf_CType fg_GetTebData(umint _iIndex)
{
#if defined(DArchitecture_arm64)
	return (tf_CType)__readx18qword(_iIndex);
#elif defined(DArchitecture_x64)
	return (tf_CType)__readgsqword(_iIndex);
#else
	return (tf_CType)__readfsdword(_iIndex);
#endif
}

inline_always UndocumentedPEB *fg_GetPEB(CUndocumentedTEB *_pTeb)
{
	return _pTeb->Peb;
}
