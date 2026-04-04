// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

namespace NMib
{
	namespace NPlatform
	{
		umint fg_ReadProcFS(NMib::NStr::CFStr256 const &_Path, uint8 *_pData, umint _nBytes);
		NContainer::TCVector<ch8> fg_ReadProcFS(NMib::NStr::CFStr256 const &_Path);
		NContainer::TCVector<ch8, NMemory::CAllocator_NonTrackedHeap> fg_ReadProcFSNonTracked(NMib::NStr::CFStr256 const &_Path);
		bool fg_ReadProcFS(NMib::NStr::CFStr256 const &_Path, NContainer::TCVector<ch8> &o_Output);
	}
}
