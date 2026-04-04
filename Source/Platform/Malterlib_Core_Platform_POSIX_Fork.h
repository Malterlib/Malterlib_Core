// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

namespace NMib
{
	namespace NPlatform
	{
		NThread::CMutual &fg_ForkLock();
		void fg_ForkPrepare();
		void fg_ForkParentOrChild();
	}
}
