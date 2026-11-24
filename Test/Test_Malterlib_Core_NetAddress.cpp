// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Test/Test>
#include <Mib/Network/Address>

#if defined(DPlatformFamily_macOS) || defined(DPlatformFamily_Linux)

#include <sys/socket.h>
#ifdef DPlatformFamily_macOS
#	include <sys/vsock.h>
#else
#	include <linux/vm_sockets.h>
#endif

namespace
{
	using namespace NMib;

	struct CNetAddress_Tests : NTest::CTest
	{
		void f_DoTests();
	};

	void CNetAddress_Tests::f_DoTests()
	{
		// getpeername returns the size of the native address, and the sockaddr_vm of macOS is shorter than sockaddr
		DMibTestSuite("FromNativeVSock")
		{
			sockaddr_vm Native{};
			Native.svm_family = AF_VSOCK;
			Native.svm_cid = 3;
			Native.svm_port = 1234;

			NSys::NNetwork::CAddress pAddress = NSys::NNetwork::fg_CreateAddressFromNative(&Native, sizeof(Native));
			auto Cleanup = g_OnScopeExit / [&]
				{
					NSys::NNetwork::fg_FreeAddress(pAddress);
				}
			;

			DMibExpect(NSys::NNetwork::fg_GetAddressType(pAddress), ==, NNetwork::ENetAddressType_VSock);

			NNetwork::CNetAddressVSock Address;
			DMibExpectTrue(NSys::NNetwork::fg_GetAddressRaw(pAddress, NNetwork::ENetAddressType_VSock, &Address, sizeof(Address)));
			DMibExpect(Address.m_ContextID, ==, uint32(3));
			DMibExpect(Address.m_Port, ==, uint32(1234));
		};
	}
}

DMibTestRegister(CNetAddress_Tests, Malterlib::Core);

#endif
