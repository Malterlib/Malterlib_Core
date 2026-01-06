// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

namespace NMib
{
	namespace NPlatform
	{
		NThread::CMutual &fg_ForkLock();
		void fg_ForkPrepare();
		void fg_ForkParentOrChild();
	}
}
